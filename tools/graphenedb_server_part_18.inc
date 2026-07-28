           << ",\"recommended_radius_for_expected_max_nodes\":" << minimum_radius_for_nodes(expected_max_nodes ? expected_max_nodes : nodes) << "}";
        out = js.str();
      } else if (method == "POST" && path == "/v1/admin/validate") {
        metrics.validation_calls.fetch_add(1, std::memory_order_relaxed);
        std::string report; auto vst = db.validate(&report);
        if (!vst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(vst.message) + "\",\"report\":\"" + json_escape(report) + "\"}"; }
        else out = "{\"ok\":true,\"report\":\"" + json_escape(report) + "\"}";
      } else if (method == "POST" && (path == "/v1/admin/compact" || path == "/v1/admin/checkpoint")) {
        metrics.checkpoint_calls.fetch_add(1, std::memory_order_relaxed);
        auto cst = db.compact();
        if (!cst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(cst.message) + "\"}"; }
        else out = "{\"ok\":true}";
      } else if (method == "GET" && path == "/v1/admin/inspect") {
        std::string report; auto ist = db.inspect(&report);
        if (!ist) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(ist.message) + "\"}"; }
        else out = "{\"ok\":true,\"report\":\"" + json_escape(report) + "\"}";
      } else if (method == "POST" && path == "/v1/admin/backup") {
        metrics.backup_calls.fetch_add(1, std::memory_order_relaxed);
        std::string dest = json_value(body, "destination");
        if (dest.empty()) { code = 400; out = "{\"error\":\"destination is required\"}"; }
        else { auto bst = db.backup(dest); if (!bst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(bst.message) + "\"}"; } else out = "{\"ok\":true,\"destination\":\"" + json_escape(dest) + "\"}"; }
      } else if (method == "POST" && path == "/v1/nodes/bulk") {
        uint32_t count = json_u32(body, "count", 0);
        const uint64_t remaining_capacity = lattice_capacity > db.node_count() ? lattice_capacity - db.node_count() : 0;
        if (lattice_capacity && count > remaining_capacity) { code = 507; out = "{\"error\":\"physical_lattice_capacity_exceeded\",\"remaining\":" + std::to_string(remaining_capacity) + "}"; }
        else
        if (count == 0 || count > max_bulk_nodes) { code = 400; out = "{\"error\":\"count_out_of_range\",\"minimum\":1,\"maximum\":" + std::to_string(max_bulk_nodes) + "}"; }
        else {
          std::string prefix = json_value(body, "prefix");
          if (prefix.empty()) prefix = "bulk node";
          if (prefix.size() > kMaxBulkPrefixBytes) {
            code = 400;
            out = "{\"error\":\"bulk_prefix_too_large\",\"max_bytes\":" + std::to_string(kMaxBulkPrefixBytes) + "}";
          }
          std::string source = json_value(body, "source");
          if (source.empty()) source = "bulk";
          if (code != 200) {
            // Validation has already produced a response.
          } else {
          BatchInput batch;
          batch.nodes.reserve(count);
          const uint32_t base_slot = next_lattice_slot.fetch_add(count, std::memory_order_relaxed);
          for (uint32_t i = 0; i < count; ++i) {
            std::string text = prefix + " " + std::to_string(i);
            NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim); ni.metadata["source"] = source;
            ni.lattice = spiral_coord(base_slot + i);
            batch.nodes.push_back(std::move(ni));
          }
          auto t0 = std::chrono::steady_clock::now();
          BatchResult batch_result;
          st = db.put_batch(batch, &batch_result);
          if (!st) {
            code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : 500;
            out = "{\"error\":\"" + json_escape(st.message) + "\",\"inserted\":0,\"atomic\":true}";
          } else {
            const uint32_t inserted = static_cast<uint32_t>(batch_result.node_ids.size());
            const uint32_t first_id = batch_result.node_ids.empty() ? UINT32_MAX : batch_result.node_ids.front();
            const uint32_t last_id = batch_result.node_ids.empty() ? UINT32_MAX : batch_result.node_ids.back();
            metrics.node_inserts.fetch_add(inserted, std::memory_order_relaxed);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            code = 201; out = "{\"inserted\":" + std::to_string(inserted) + ",\"first_id\":" + std::to_string(first_id) + ",\"last_id\":" + std::to_string(last_id) + ",\"elapsed_ms\":" + std::to_string(ms) + ",\"atomic\":true}";
          }
          }
        }
      } else if (method == "POST" && path == "/v1/extractions") {
        ExtractionInput extraction;
        std::string parse_error;
        if (!parse_extraction_request(body, dim, max_bulk_nodes, &extraction,
                                      &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_extraction\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else {
          if (extraction.place_missing_lattice && !extraction.nodes.empty()) {
            const uint32_t base_slot = next_lattice_slot.fetch_add(
                static_cast<uint32_t>(extraction.nodes.size()),
                std::memory_order_relaxed);
            for (size_t index = 0; index < extraction.nodes.size(); ++index) {
              if (!extraction.nodes[index].lattice) {
                extraction.nodes[index].lattice =
                    spiral_coord(base_slot + static_cast<uint32_t>(index));
              }
            }
            extraction.place_missing_lattice = false;
          }
          ExtractionResult result;
          const auto started = std::chrono::steady_clock::now();
          st = db.put_extraction(extraction, &result);
          if (!st) {
            if (st.message.find("capacity") != std::string::npos ||
                st.message.find("physical lattice") != std::string::npos) {
              code = 507;
            } else if (st.message.find("idempotency conflict") !=
                       std::string::npos) {
              code = 409;
            } else {
              code = http_status_for(st);
            }
            out = "{\"error\":\"extraction_failed\",\"detail\":\"" +
                  json_escape(st.message) + "\",\"atomic\":true,"
                  "\"inserted_nodes\":0,\"inserted_edges\":0}";
          } else {
            metrics.node_inserts.fetch_add(result.inserted_node_ids.size(),
                                           std::memory_order_relaxed);
            metrics.edge_inserts.fetch_add(result.inserted_edge_ids.size(),
                                           std::memory_order_relaxed);
            const bool replay = result.inserted_node_ids.empty() &&
                                result.inserted_edge_ids.empty();
            code = replay ? 200 : 201;
            out = extraction_result_json(result, extraction.idempotent);
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                            started)
                                     .count();
            out.pop_back();
            out += ",\"elapsed_ms\":" + std::to_string(elapsed) + "}";
          }
        }
      } else if (method == "POST" && path == "/v1/facts") {
        if (idempotency_key.size() > 128) { code = 400; out = "{\"error\":\"idempotency_key_too_long\",\"max_length\":128}"; }
        else if (lattice_capacity && db.node_count() >= lattice_capacity && idempotency_key.empty()) { code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; }
        else {
        std::string text = json_value(body, "text");
        if (text.empty()) text = json_value(body, "content");
        if (text.empty()) { code = 400; out = "{\"error\":\"text/content is required\"}"; }
        else if (text.size() > kMaxNodeTextBytes) { code = 400; out = "{\"error\":\"text_too_large\",\"max_bytes\":" + std::to_string(kMaxNodeTextBytes) + "}"; }
        else {
          std::unique_lock<std::mutex> idempotency_lock;
          bool idempotency_handled = false;
          const std::string scoped_idempotency_key = path + ":" + idempotency_key;
          if (!idempotency_key.empty()) {
            idempotency_lock = std::unique_lock<std::mutex>(idempotency_mutex);
            const auto existing = db.metadata_search("_idempotency_key", scoped_idempotency_key);
            if (!existing.empty()) {
              const auto prior = db.get_node(existing.front());
              if (!prior || prior->content != text) {
                code = 409; out = "{\"error\":\"idempotency_conflict\"}"; idempotency_handled = true;
              } else {
                code = 200; out = "{\"id\":" + std::to_string(existing.front()) + ",\"type\":\"temporal_fact\",\"idempotent_replay\":true}"; idempotency_handled = true;
              }
            }
          }
          if (!idempotency_handled && lattice_capacity && db.node_count() >= lattice_capacity) {
            code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; idempotency_handled = true;
          }
          if (!idempotency_handled) {
          NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim);
          ni.metadata["source"] = json_value(body, "source").empty() ? "server" : json_value(body, "source");
          ni.metadata["graphene_type"] = "temporal_fact";
          if (!idempotency_key.empty()) ni.metadata["_idempotency_key"] = scoped_idempotency_key;