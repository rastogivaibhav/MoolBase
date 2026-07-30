      }
    }
    if (req.empty() && !too_large && !read_timeout) {
      ::close(fd);
      metrics.active.fetch_sub(1, std::memory_order_relaxed);
      metrics.completed.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    auto first_end = req.find("\r\n");
    auto bp = req.find("\r\n\r\n");
    std::string first = first_end == std::string::npos ? std::string{} : req.substr(0, first_end);
    std::string header_text = bp == std::string::npos ? std::string{} : req.substr(0, bp + 4);
    std::istringstream fsline(first); std::string method, path, http_version; fsline >> method >> path >> http_version;
    std::string client_ip = task.peer_ip;
    if (trust_proxy) {
      const std::string forwarded = first_forwarded_ip(header_text);
      if (!forwarded.empty()) client_ip = forwarded;
    }
    const bool public_probe = path == "/v1/health" || path == "/v1/ready" || path == "/v1/version";
    const bool authorized = public_probe || authorized_request(header_text, api_key);
    const bool forwarded_https_ok = !require_forwarded_https || public_probe || lower_copy(header_value(header_text, "x-forwarded-proto")) == "https";
    const bool rate_allowed = public_probe || rate_limiter.allow(client_ip);
    const std::string idempotency_key = header_value(header_text, "idempotency-key");
    std::string body;
    if (bp != std::string::npos) {
      body = req.substr(bp + 4);
      if (content_length > 0 && body.size() > content_length) body.resize(content_length);
    }
    metrics.request_bytes.fetch_add(req.size(), std::memory_order_relaxed);
    std::string out;
    std::string response_content_type = "application/json";
    int retry_after = 0;
    int code = 200;
    try {
      if (too_large) {
        code = 413; out = "{\"error\":\"request_too_large\",\"max_request_bytes\":" + std::to_string(max_request_bytes) + "}";
      } else if (read_timeout) {
        code = 408; out = "{\"error\":\"request_timeout\"}";
      } else if (first.empty() || method.empty() || path.empty() || http_version.rfind("HTTP/1.", 0) != 0) {
        code = 400; out = "{\"error\":\"malformed_request_line\"}";
      } else if (malformed_content_length) {
        code = 400; out = "{\"error\":\"invalid_content_length\"}";
      } else if (unsupported_transfer_encoding) {
        code = 501; out = "{\"error\":\"transfer_encoding_not_supported\",\"supported\":\"content-length\"}";
      } else if (endpoint_requires_json_body(method, path) && !content_length_present) {
        code = 411; out = "{\"error\":\"content_length_required\"}";
      } else if (endpoint_requires_json_body(method, path) && !application_json_content_type(header_text)) {
        code = 415; out = "{\"error\":\"application_json_required\"}";
      } else if (!rate_allowed) {
        metrics.rate_limited.fetch_add(1, std::memory_order_relaxed);
        code = 429; retry_after = 1; out = "{\"error\":\"rate_limited\"}";
      } else if (!forwarded_https_ok) {
        code = 403; out = "{\"error\":\"https_required_by_proxy_policy\"}";
      } else if (!authorized) {
        metrics.auth_failures.fetch_add(1, std::memory_order_relaxed);
        code = 401; out = "{\"error\":\"unauthorized\"}";
      } else if (method == "GET" && path == "/v1/health") {
        out = "{\"status\":\"ok\",\"engine\":\"physical-hex-lattice\",\"server_version\":\"" + std::string(kServerVersion) + "\",\"api_version\":" + std::to_string(kApiVersion) + "}";
      } else if (method == "GET" && path == "/v1/version") {
        std::ostringstream js;
        js << "{\"server_version\":\"" << kServerVersion
           << "\",\"api_version\":" << kApiVersion
           << ",\"storage_formats\":{\"manifest\":" << kManifestFormatVersion
           << ",\"storage\":" << kStorageFormatVersion
           << ",\"wal_frame\":" << kWalFrameFormatVersion
           << ",\"lattice\":" << kLatticeFormatVersion
           << ",\"extraction\":" << kExtractionFormatVersion
           << "},\"features\":[\"physical_lattice_primary\",\"vector_search\",\"lattice_search\",\"checkpoint\",\"backup\",\"validation\",\"idempotent_node_writes\",\"atomic_extraction_ingest\",\"bounded_dialectic_reasoning\",\"read_only_hypokosh\",\"governed_outcome_learning\"]}";
        out = js.str();
      } else if (method == "GET" && path == "/v1/ready") {
        std::string report; auto ist = db.inspect(&report);
        const uint64_t nodes = db.node_count();
        const bool capacity_ok = lattice_capacity == 0 || nodes < lattice_capacity;
        if (g_stop) {
          code = 503;
          out = "{\"status\":\"not_ready\",\"reason\":\"shutting_down\"}";
        } else if (!ist || !db.is_open() || !capacity_ok) {
          code = 503;
          out = "{\"status\":\"not_ready\",\"reason\":\"" + std::string(capacity_ok ? "storage_not_ready" : "physical_lattice_capacity_exhausted") + "\"}";
        } else {
          const double utilization = lattice_capacity ? static_cast<double>(nodes) / static_cast<double>(lattice_capacity) : 0.0;
          std::ostringstream js;
          js << "{\"status\":\"ready\",\"node_count\":" << nodes
             << ",\"physical_lattice_capacity\":" << lattice_capacity
             << ",\"physical_lattice_utilization\":" << utilization << "}";
          out = js.str();
        }
      } else if (method == "GET" && path == "/v1/metrics") {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - metrics.started).count();
        const uint64_t duration_count = metrics.completed.load();
        const double avg_duration_ms = duration_count ? (static_cast<double>(metrics.duration_us_sum.load()) / duration_count / 1000.0) : 0.0;
        std::ostringstream js;
        js << "{\"uptime_seconds\":" << uptime
           << ",\"requests\":" << metrics.requests.load()
           << ",\"completed\":" << metrics.completed.load()
           << ",\"active\":" << metrics.active.load()
           << ",\"queue_depth\":" << metrics.queue_depth.load()
           << ",\"queue_rejected\":" << metrics.queue_rejected.load()
           << ",\"rate_limited\":" << metrics.rate_limited.load()
           << ",\"auth_failures\":" << metrics.auth_failures.load()
           << ",\"request_bytes\":" << metrics.request_bytes.load()
           << ",\"response_bytes\":" << metrics.response_bytes.load()
           << ",\"request_duration_avg_ms\":" << avg_duration_ms
           << ",\"request_duration_max_ms\":" << (metrics.duration_us_max.load()/1000.0)
           << ",\"status_2xx\":" << metrics.status_2xx.load()
           << ",\"status_4xx\":" << metrics.status_4xx.load()
           << ",\"status_5xx\":" << metrics.status_5xx.load()
           << ",\"node_inserts\":" << metrics.node_inserts.load()
           << ",\"edge_inserts\":" << metrics.edge_inserts.load()
           << ",\"vector_searches\":" << metrics.vector_searches.load()
           << ",\"lattice_searches\":" << metrics.lattice_searches.load()
           << ",\"validation_calls\":" << metrics.validation_calls.load()
           << ",\"checkpoint_calls\":" << metrics.checkpoint_calls.load()
           << ",\"backup_calls\":" << metrics.backup_calls.load()
           << ",\"errors\":" << metrics.errors.load()
           << ",\"node_count\":" << db.node_count()
           << ",\"edge_count\":" << db.edge_count()
           << ",\"physical_lattice_capacity\":" << lattice_capacity << "}";
        out = js.str();
      } else if (method == "GET" && path == "/v1/metrics/prometheus") {
        response_content_type = "text/plain; version=0.0.4";
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - metrics.started).count();
        std::ostringstream pm;
        pm << "# TYPE graphenedb_uptime_seconds gauge\n"
           << "graphenedb_uptime_seconds " << uptime << "\n"
           << "# TYPE graphenedb_http_requests_total counter\n"
           << "graphenedb_http_requests_total " << metrics.requests.load() << "\n"
           << "graphenedb_http_requests_completed_total " << metrics.completed.load() << "\n"
           << "graphenedb_http_active_requests " << metrics.active.load() << "\n"
           << "graphenedb_http_queue_depth " << metrics.queue_depth.load() << "\n"
           << "graphenedb_http_queue_rejected_total " << metrics.queue_rejected.load() << "\n"
           << "graphenedb_http_rate_limited_total " << metrics.rate_limited.load() << "\n"
           << "graphenedb_http_auth_failures_total " << metrics.auth_failures.load() << "\n"
           << "graphenedb_http_request_duration_seconds_sum " << (metrics.duration_us_sum.load()/1000000.0) << "\n"
           << "graphenedb_http_request_duration_seconds_count " << metrics.completed.load() << "\n"
           << "graphenedb_nodes " << db.node_count() << "\n"
           << "graphenedb_edges " << db.edge_count() << "\n"
           << "graphenedb_physical_lattice_capacity " << lattice_capacity << "\n";
        out = pm.str();
      } else if (method == "GET" && path == "/v1/admin/capacity") {
        const uint64_t nodes = db.node_count();
        const uint64_t remaining = lattice_capacity > nodes ? lattice_capacity - nodes : 0;
        const double utilization = lattice_capacity ? static_cast<double>(nodes) / lattice_capacity : 0.0;
        std::ostringstream js;
        js << "{\"physical_lattice_primary\":" << (opt.physical_lattice_primary ? "true" : "false")
           << ",\"radius\":" << opt.physical_lattice_radius
           << ",\"capacity\":" << lattice_capacity
           << ",\"nodes\":" << nodes
           << ",\"remaining\":" << remaining
           << ",\"utilization\":" << utilization