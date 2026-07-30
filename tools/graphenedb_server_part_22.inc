            else out = "{\"id\":" + std::to_string(id) + ",\"content\":\"" + json_escape(n0->content) + "\",\"metadata\":" + metadata_json(n0->metadata) + "}";
          }
        }
      } else if (method == "GET" && path.rfind("/v1/search/lattice", 0) == 0) {
        metrics.lattice_searches.fetch_add(1, std::memory_order_relaxed);
        uint32_t id = query_u32(path, "node_id", 0);
        uint32_t hops = std::clamp<uint32_t>(query_u32(path, "hops", 2), 1, kMaxLatticeHops);
        if (!db.get_node(id)) { code = 404; out = "{\"error\":\"not found\"}"; }
        else {
        auto ns = db.lattice_neighbors(id, hops);
        std::ostringstream js; js << "{\"neighbors\":[";
        for (size_t i = 0; i < ns.size(); ++i) { if (i) js << ','; js << ns[i]; }
        js << "],\"hops\":" << hops << "}"; out = js.str();
        }
      } else {
        code = 404; out = "{\"error\":\"unknown endpoint\"}";
      }
    } catch (const std::exception& ex) {
      code = 500; out = "{\"error\":\"" + json_escape(ex.what()) + "\"}";
    }
    if (code >= 400) metrics.errors.fetch_add(1, std::memory_order_relaxed);
    if (code >= 200 && code < 300) metrics.status_2xx.fetch_add(1, std::memory_order_relaxed);
    else if (code >= 400 && code < 500) metrics.status_4xx.fetch_add(1, std::memory_order_relaxed);
    else if (code >= 500) metrics.status_5xx.fetch_add(1, std::memory_order_relaxed);
    auto resp = response(code, out, request_id, response_content_type, retry_after);
    (void)write_all(fd, resp);
    metrics.response_bytes.fetch_add(resp.size(), std::memory_order_relaxed);
    ::close(fd);
    const uint64_t duration_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - request_started).count());
    metrics.duration_us_sum.fetch_add(duration_us, std::memory_order_relaxed);
    graphenedb::server::atomic_max(metrics.duration_us_max, duration_us);
    metrics.active.fetch_sub(1, std::memory_order_relaxed);
    metrics.completed.fetch_add(1, std::memory_order_relaxed);
    structured_request_log(request_id, client_ip, method, path, code, duration_us, req.size(), resp.size(), !rate_allowed);
  };

  graphenedb::server::BoundedWorkerPool pool(worker_count, queue_capacity, handle_client, metrics);
  while (!g_stop) {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    int fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (fd < 0) {
      if (g_stop) break;
      if (errno == EINTR) continue;
      perror("accept");
      break;
    }
    char ipbuf[INET_ADDRSTRLEN]{};
    const char* ip = inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
    ConnectionTask task{fd, ip ? std::string(ip) : std::string("unknown")};
    if (!pool.submit(std::move(task))) {
      const std::string body = "{\"error\":\"server_overloaded\"}";
      const auto resp = response(503, body, {}, "application/json", 1);
      (void)write_all(fd, resp);
      ::close(fd);
    }
  }
  if (g_listen_fd >= 0) {
    ::close(static_cast<int>(g_listen_fd));
    g_listen_fd = -1;
  }
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"info\",\"event\":\"shutdown_started\",\"active_requests\":"
        << metrics.active.load() << ",\"queued_requests\":" << metrics.queue_depth.load() << "}";
    emit_structured_log(log.str());
  }
  pool.shutdown();
  int exit_code = 0;
  if (shutdown_checkpoint) {
    const auto checkpoint_started = std::chrono::steady_clock::now();
    const auto cst = db.compact();
    const auto checkpoint_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - checkpoint_started).count();
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"" << (cst ? "info" : "error")
        << "\",\"event\":\"shutdown_checkpoint\",\"ok\":" << (cst ? "true" : "false")
        << ",\"duration_ms\":" << checkpoint_ms;
    if (!cst) {
      log << ",\"error\":\"" << json_escape(cst.message) << "\"";
      exit_code = 1;
    }
    log << "}";
    emit_structured_log(log.str());
  }
  const auto close_status = db.close();
  if (!close_status) exit_code = 1;
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"" << (exit_code == 0 ? "info" : "error")
        << "\",\"event\":\"shutdown_complete\",\"exit_code\":" << exit_code << "}";
    emit_structured_log(log.str());
  }
  return exit_code;
}
