  const int port = std::stoi(argv[3]);
  DBOptions opt; opt.dimension = dim; opt.create_if_missing = true; opt.vector_index_kind = VectorIndexKind::Auto;
  std::string api_key;
  if (const char* env_key = std::getenv("GRAPHENEDB_API_KEY")) api_key = env_key;
  std::string bind_address = "127.0.0.1";
  size_t worker_count = std::clamp<size_t>(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4, 4, 32);
  size_t queue_capacity = 1024;
  size_t max_bulk_nodes = 10000;
  double rate_limit_rps = 200.0;
  double rate_limit_burst = 400.0;
  size_t max_request_bytes = 4 * 1024 * 1024;
  int socket_timeout_seconds = 30;
  uint64_t expected_max_nodes = 0;
  bool trust_proxy = false;
  bool behind_tls_proxy = false;
  bool require_forwarded_https = false;
  bool allow_insecure_public_bind = false;
  bool shutdown_checkpoint = true;
  for (int i = 4; i < argc; ++i) {
    std::string f = argv[i];
    if (f == "--bind-address" && i + 1 < argc) bind_address = argv[++i];
    else if (f == "--physical-lattice-primary") { opt.require_lattice = true; opt.physical_lattice_storage = true; opt.physical_lattice_primary = true; }
    else if (f == "--physical-lattice-radius" && i + 1 < argc) opt.physical_lattice_radius = static_cast<uint32_t>(std::stoul(argv[++i]));
    else if (f == "--expected-max-nodes" && i + 1 < argc) expected_max_nodes = std::stoull(argv[++i]);
    else if (f == "--wal-rotate-bytes" && i + 1 < argc) opt.wal_rotate_bytes = std::stoull(argv[++i]);
    else if (f == "--vector-index" && i + 1 < argc) {
      if (!parse_vector_index_kind(argv[++i], &opt.vector_index_kind)) {
        std::cerr << "invalid --vector-index; expected auto|flat|kdtree|faiss\n";
        return 2;
      }
    }
    else if (f == "--api-key" && i + 1 < argc) api_key = argv[++i];
    else if (f == "--workers" && i + 1 < argc) worker_count = std::max<size_t>(1, std::stoull(argv[++i]));
    else if (f == "--queue-capacity" && i + 1 < argc) queue_capacity = std::max<size_t>(1, std::stoull(argv[++i]));
    else if (f == "--max-bulk-nodes" && i + 1 < argc) max_bulk_nodes = std::clamp<size_t>(std::stoull(argv[++i]), 1, 50000);
    else if (f == "--rate-limit-rps" && i + 1 < argc) rate_limit_rps = std::stod(argv[++i]);
    else if (f == "--rate-limit-burst" && i + 1 < argc) rate_limit_burst = std::stod(argv[++i]);
    else if (f == "--max-request-bytes" && i + 1 < argc) max_request_bytes = std::max<size_t>(1024, std::stoull(argv[++i]));
    else if (f == "--socket-timeout-seconds" && i + 1 < argc) socket_timeout_seconds = std::max(1, std::stoi(argv[++i]));
    else if (f == "--trust-proxy") trust_proxy = true;
    else if (f == "--behind-tls-proxy") behind_tls_proxy = true;
    else if (f == "--require-forwarded-https") { behind_tls_proxy = true; require_forwarded_https = true; }
    else if (f == "--allow-insecure-public-bind") allow_insecure_public_bind = true;
    else if (f == "--no-shutdown-checkpoint") shutdown_checkpoint = false;
    else { std::cerr << "unknown or incomplete option: " << f << "\n"; return 2; }
  }
  if (!loopback_address(bind_address) && !allow_insecure_public_bind) {
    if (api_key.empty()) {
      std::cerr << "refusing non-loopback bind without GRAPHENEDB_API_KEY/--api-key\n";
      return 2;
    }
    if (!behind_tls_proxy) {
      std::cerr << "refusing non-loopback bind without --behind-tls-proxy; use a TLS reverse proxy or explicitly pass --allow-insecure-public-bind\n";
      return 2;
    }
  }
  if (trust_proxy && !behind_tls_proxy) {
    std::cerr << "--trust-proxy requires --behind-tls-proxy\n";
    return 2;
  }
  if (opt.physical_lattice_primary) {
    const uint64_t capacity = physical_capacity(opt.physical_lattice_radius);
    if (expected_max_nodes > capacity) {
      std::cerr << "physical lattice radius " << opt.physical_lattice_radius << " has capacity " << capacity
                << ", below --expected-max-nodes " << expected_max_nodes
                << "; minimum radius is " << minimum_radius_for_nodes(expected_max_nodes) << "\n";
      return 2;
    }
  }
  GrapheneDB db;
  auto st = db.open(dir, opt);
  if (!st) { std::cerr << st.message << "\n"; return 1; }

  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) { perror("socket"); return 1; }
  int yes = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr) != 1) {
    std::cerr << "invalid --bind-address: " << bind_address << "\n"; return 2;
  }
  if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) { perror("bind"); return 1; }
  const int listen_backlog = static_cast<int>(std::min<size_t>(queue_capacity, 4096));
  if (listen(server_fd, std::max(16, listen_backlog)) < 0) { perror("listen"); return 1; }
  g_listen_fd = server_fd;
  const uint64_t lattice_capacity = opt.physical_lattice_primary ? physical_capacity(opt.physical_lattice_radius) : 0;
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"info\",\"event\":\"server_started\",\"bind_address\":\"" << json_escape(bind_address)
        << "\",\"port\":" << port
        << ",\"workers\":" << worker_count
        << ",\"queue_capacity\":" << queue_capacity
        << ",\"max_bulk_nodes\":" << max_bulk_nodes
        << ",\"rate_limit_rps\":" << rate_limit_rps
        << ",\"rate_limit_burst\":" << rate_limit_burst
        << ",\"max_request_bytes\":" << max_request_bytes
        << ",\"shutdown_checkpoint\":" << (shutdown_checkpoint ? "true" : "false")
        << ",\"physical_lattice_radius\":" << opt.physical_lattice_radius
        << ",\"physical_lattice_capacity\":" << lattice_capacity << "}";
    emit_structured_log(log.str());
  }

  ServerMetrics metrics;
  graphenedb::server::TokenBucketRateLimiter rate_limiter(rate_limit_rps, rate_limit_burst);
  std::atomic<uint64_t> request_sequence{1};
  std::atomic<uint32_t> next_lattice_slot{static_cast<uint32_t>(db.node_count())};
  std::mutex idempotency_mutex;
  auto handle_client = [&](ConnectionTask task) {
    const int fd = task.fd;
    metrics.requests.fetch_add(1, std::memory_order_relaxed);
    metrics.active.fetch_add(1, std::memory_order_relaxed);
    const auto request_started = std::chrono::steady_clock::now();
    const std::string request_id = std::to_string(::getpid()) + "-" + std::to_string(request_sequence.fetch_add(1, std::memory_order_relaxed));
    timeval socket_timeout{}; socket_timeout.tv_sec = socket_timeout_seconds; socket_timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &socket_timeout, sizeof(socket_timeout));
    std::string req;
    char buf[8192];
    size_t content_length = 0;
    size_t header_end = std::string::npos;
    bool too_large = false;
    bool read_timeout = false;
    bool malformed_content_length = false;
    bool unsupported_transfer_encoding = false;
    bool content_length_present = false;
    while (true) {
      ssize_t n = ::read(fd, buf, sizeof(buf));
      if (n < 0 && errno == EINTR) continue;
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { read_timeout = true; break; }
      if (n <= 0) break;
      req.append(buf, static_cast<size_t>(n));
      if (req.size() > max_request_bytes) { too_large = true; break; }
      header_end = req.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        std::string headers = req.substr(0, header_end + 4);
        const std::string transfer_encoding = lower_copy(header_value(headers, "transfer-encoding"));
        if (!transfer_encoding.empty() && transfer_encoding != "identity") {
          unsupported_transfer_encoding = true;
          break;
        }
        const auto cl = parse_content_length(headers);
        content_length_present = cl.present;
        if (!cl.valid) {
          malformed_content_length = true;
          break;
        }
        content_length = cl.value;
        if (header_end + 4 + content_length > max_request_bytes) { too_large = true; break; }
        const size_t have_body = req.size() - (header_end + 4);
        if (!content_length_present || have_body >= content_length) break;