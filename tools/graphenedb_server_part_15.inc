      if (!e) { if (eid > edge_total + 16) break; continue; }
      if (!edge_valid_at(db, *e, as_of)) continue;
      if (e->role == EdgeRole::Contradicts && (bundle_nodes.count(e->from) || bundle_nodes.count(e->to))) {
        if (attached_ids.insert(e->id).second) attached_contradictions.push_back(*e);
        has_contra = true;
      }
    }
    js << "{\"target\":" << kv.first << ",\"degeneracy\":" << kv.second.size() << ",\"best_confidence\":" << best << ",\"has_contradiction\":" << (has_contra?"true":"false") << ",\"attached_contradictions\":[";
    for (size_t ci=0; ci<attached_contradictions.size(); ++ci) {
      const auto& ce = attached_contradictions[ci];
      if (ci) js << ',';
      js << "{\"edge\":" << ce.id << ",\"from\":" << ce.from << ",\"to\":" << ce.to << ",\"origin\":\"" << origin_name(ce.origin) << "\",\"role\":\"" << role_name(ce.role) << "\",\"confidence\":" << ce.confidence << "}";
    }
    js << "],\"paths\":[";
    bool first_path = true;
    for (const auto& st : kv.second) {
      if (!first_path) js << ',';
      first_path = false;
      js << "{\"nodes\":[";
      for (size_t i=0;i<st.nodes.size();++i){ if(i) js << ','; js << st.nodes[i]; }
      js << "],\"edges\":[";
      for (size_t i=0;i<st.edges.size();++i){ if(i) js << ','; js << st.edges[i]; }
      js << "],\"roles\":[";
      for (size_t i=0;i<st.edges.size();++i){ auto e=db.get_edge(st.edges[i]); if(i) js << ','; js << "\"" << (e?role_name(e->role):"") << "\""; }
      js << "],\"origins\":[";
      for (size_t i=0;i<st.edges.size();++i){ auto e=db.get_edge(st.edges[i]); if(i) js << ','; js << "\"" << (e?origin_name(e->origin):"") << "\""; }
      js << "],\"confidence\":" << st.confidence << ",\"temporal_consistent\":" << (as_of.empty()?"true":"true") << ",\"contains_contradiction\":" << (st.contradiction?"true":"false") << ",\"contains_hypothetical\":" << (st.hypothetical?"true":"false") << "}";
    }
    js << "]}";
  }
  js << "]}";
  return js.str();
}

static uint64_t physical_capacity(uint32_t radius, uint32_t layers = 1) {
  const uint64_t per_layer = 1ull + 3ull * radius * (static_cast<uint64_t>(radius) + 1ull);
  return per_layer * std::max<uint32_t>(1, layers);
}

static uint32_t minimum_radius_for_nodes(uint64_t nodes) {
  if (nodes <= 1) return 0;
  const double root = (-3.0 + std::sqrt(std::max(0.0, 12.0 * static_cast<double>(nodes) - 3.0))) / 6.0;
  return static_cast<uint32_t>(std::ceil(root));
}

static bool loopback_address(const std::string& address) {
  return address == "127.0.0.1" || address == "::1" || address == "localhost";
}

static std::string status_text(int code) {
  switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    case 507: return "Insufficient Storage";
    default: return "Error";
  }
}

static std::string response(int code, const std::string& body,
                            const std::string& request_id = {},
                            const std::string& content_type = "application/json",
                            int retry_after_seconds = 0) {
  std::ostringstream os;
  os << "HTTP/1.1 " << code << ' ' << status_text(code)
     << "\r\nContent-Type: " << content_type
     << "\r\nContent-Length: " << body.size()
     << "\r\nConnection: close"
     << "\r\nCache-Control: no-store"
     << "\r\nX-Content-Type-Options: nosniff"
     << "\r\nX-GrapheneDB-Version: " << kServerVersion
     << "\r\nX-GrapheneDB-API-Version: " << kApiVersion;
  if (!request_id.empty()) os << "\r\nX-Request-ID: " << request_id;
  if (retry_after_seconds > 0) os << "\r\nRetry-After: " << retry_after_seconds;
  os << "\r\n\r\n" << body;
  return os.str();
}

static bool write_all(int fd, const std::string& payload) {
  size_t sent = 0;
  while (sent < payload.size()) {
    const ssize_t n = ::write(fd, payload.data() + sent, payload.size() - sent);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

static void structured_request_log(const std::string& request_id,
                                   const std::string& client_ip,
                                   const std::string& method,
                                   const std::string& path,
                                   int status,
                                   uint64_t duration_us,
                                   size_t bytes_in,
                                   size_t bytes_out,
                                   bool rate_limited) {
  std::ostringstream log;
  log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
      << "\",\"level\":\"" << (status >= 500 ? "error" : status >= 400 ? "warn" : "info")
      << "\",\"event\":\"http_request\",\"request_id\":\"" << json_escape(request_id)
      << "\",\"client_ip\":\"" << json_escape(client_ip)
      << "\",\"method\":\"" << json_escape(method)
      << "\",\"path\":\"" << json_escape(path)
      << "\",\"status\":" << status
      << ",\"duration_us\":" << duration_us
      << ",\"bytes_in\":" << bytes_in
      << ",\"bytes_out\":" << bytes_out
      << ",\"rate_limited\":" << (rate_limited ? "true" : "false") << "}";
  emit_structured_log(log.str());
}


int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "usage: graphenedb_server <db_dir> <dimension> <port> [options]\n"
              << "  --bind-address IP\n"
              << "  --physical-lattice-primary --physical-lattice-radius N\n"
              << "  --expected-max-nodes N --wal-rotate-bytes N\n"
              << "  --workers N --queue-capacity N\n"
              << "  --max-bulk-nodes N\n"
              << "  --rate-limit-rps N --rate-limit-burst N\n"
              << "  --max-request-bytes N --socket-timeout-seconds N\n"
              << "  --vector-index auto|flat|kdtree|faiss\n"
              << "  --api-key KEY --trust-proxy --behind-tls-proxy\n"
              << "  --require-forwarded-https --allow-insecure-public-bind\n"
              << "  --no-shutdown-checkpoint\n";
    return 2;
  }
  std::signal(SIGINT, on_sig); std::signal(SIGTERM, on_sig);
#ifdef SIGPIPE
  // A client may disconnect before reading the response. Ignore SIGPIPE so one
  // reset connection cannot terminate the entire database server.
  std::signal(SIGPIPE, SIG_IGN);
#endif
  const fs::path dir = argv[1];
  const uint32_t dim = static_cast<uint32_t>(std::stoul(argv[2]));