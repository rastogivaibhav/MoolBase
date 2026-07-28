        }
      } else if (method == "GET" &&
                 (path == "/v1/learning/policies/current" ||
                  (path.rfind("/v1/learning/policies/current", 0) == 0 &&
                   path.size() >
                       std::strlen("/v1/learning/policies/current") &&
                   path[std::strlen("/v1/learning/policies/current")] ==
                       '?'))) {
        const std::string tenant_id = query_string(path, "tenant_id");
        if (tenant_id.empty() || tenant_id.size() > 256) {
          code = 400;
          out = "{\"error\":\"tenant_id query parameter is required\"}";
        } else {
          const auto current =
              OutcomeLearningEngine(db).current_policy(tenant_id);
          if (!current) {
            code = 404;
            out = "{\"error\":\"active_policy_not_found\"}";
          } else {
            out = policy_state_json(*current, false);
          }
        }
      } else if (method == "POST" &&
                 path == "/v1/learning/episodes/quarantine") {
        JsonDocumentValue quarantine;
        std::string parse_error;
        std::string tenant_id;
        std::string episode_id;
        if (!parse_json_object(body, &quarantine, &parse_error) ||
            !reject_unknown_json_fields(
                quarantine, {"tenant_id", "episode_id"}, "request",
                &parse_error) ||
            !extraction_string_field(quarantine, "tenant_id", 256, true,
                                     &tenant_id, &parse_error) ||
            !extraction_string_field(quarantine, "episode_id", 256, true,
                                     &episode_id, &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_quarantine_request\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else {
          st = OutcomeLearningEngine(db).quarantine_episode(tenant_id,
                                                            episode_id);
          if (!st) {
            code = http_status_for(st);
            out = "{\"error\":\"quarantine_failed\",\"detail\":\"" +
                  json_escape(st.message) + "\"}";
          } else {
            out = "{\"quarantined\":true,\"episode_id\":\"" +
                  json_escape(episode_id) + "\"}";
          }
        }
      } else if (method == "POST" && path == "/v1/admin/validate/provenance") {
        uint32_t missing = 0;
        uint32_t inferred_without_derived = 0;
        uint32_t reinforced_truth_risk = 0;
        uint32_t compressed_without_mechanism = 0;
        uint32_t invalid_observed_at = 0;
        uint32_t invalid_temporal_metadata = 0;
        const size_t total = db.edge_count();
        for (uint32_t eid = 0; eid < total + 1000; ++eid) {
          auto e = db.get_edge(eid); if (!e) { if (eid > total + 16) break; continue; }
          const EdgeProvenance provenance = assess_edge_provenance(*e);
          for (const auto& finding : provenance.findings) {
            if (finding.code == "MISSING_EVIDENCE") ++missing;
            else if (finding.code == "INFERRED_WITHOUT_DERIVATION") ++inferred_without_derived;
            else if (finding.code == "REINFORCED_TRUTH_PROMOTION") ++reinforced_truth_risk;
            else if (finding.code == "COMPRESSED_WITHOUT_MECHANISM") ++compressed_without_mechanism;
            else if (finding.code == "INVALID_OBSERVED_AT") ++invalid_observed_at;
            else if (finding.code == "INVALID_TEMPORAL_METADATA") ++invalid_temporal_metadata;
          }
        }
        const bool valid = missing == 0 && inferred_without_derived == 0 &&
                           reinforced_truth_risk == 0 &&
                           compressed_without_mechanism == 0 &&
                           invalid_observed_at == 0 &&
                           invalid_temporal_metadata == 0;
        out = "{\"ok\":" + std::string(valid ? "true" : "false") +
              ",\"edges_checked\":" + std::to_string(total) +
              ",\"missing_evidence\":" + std::to_string(missing) +
              ",\"inferred_without_derived_from\":" +
              std::to_string(inferred_without_derived) +
              ",\"reinforced_truth_promotion_risk\":" +
              std::to_string(reinforced_truth_risk) +
              ",\"compressed_without_mechanism\":" +
              std::to_string(compressed_without_mechanism) +
              ",\"invalid_observed_at\":" +
              std::to_string(invalid_observed_at) +
              ",\"invalid_temporal_metadata\":" +
              std::to_string(invalid_temporal_metadata) + "}";
      } else if (method == "POST" && path == "/v1/admin/lattice/quality") {
        out = lattice_quality_json(db);
      } else if (method == "POST" && path == "/v1/retrieve/temporal") {
        std::string as_of = json_value(body, "as_of"); if (as_of.empty()) as_of = json_value(body, "at");
        std::string key = json_value(body, "metadata_key");
        std::string val = json_value(body, "metadata_value");
        const uint32_t limit = std::clamp<uint32_t>(json_u32(body, "limit", 100), 1, static_cast<uint32_t>(kMaxTemporalResults));
        const uint32_t offset = json_u32(body, "offset", 0);
        std::ostringstream js; js << "{\"as_of\":\"" << json_escape(as_of) << "\",\"limit\":" << limit << ",\"offset\":" << offset << ",\"results\":[";
        bool first = true;
        uint32_t matched = 0;
        uint32_t returned = 0;
        size_t total = db.node_count();
        for (uint32_t id = 0; id < total + 1000; ++id) {
          auto n0 = db.get_node(id);
          if (!n0) { if (id > total + 16) break; continue; }
          if (!node_valid_at(*n0, as_of)) continue;
          if (!key.empty()) {
            auto it = n0->metadata.find(key);
            if (it == n0->metadata.end() || it->second != val) continue;
          }
          if (matched++ < offset) continue;
          if (returned >= limit) break;
          if (!first) js << ',';
          first = false;
          js << "{\"id\":" << id << ",\"content\":\"" << json_escape(n0->content) << "\",\"metadata\":" << metadata_json(n0->metadata) << "}";
          ++returned;
        }
        js << "],\"returned\":" << returned << ",\"next_offset\":" << (offset + returned) << "}"; out = js.str();
      } else if (method == "POST" && path == "/v1/search/hybrid") {
        metrics.vector_searches.fetch_add(1, std::memory_order_relaxed);
        std::string q = json_value(body, "query");
        if (q.empty()) {
          code = 400;
          out = "{\"error\":\"query is required\"}";
        } else {
        size_t topk = 5;
        try { auto k = json_value(body, "top_k"); if (!k.empty()) topk = static_cast<size_t>(std::stoull(k)); } catch (...) {}
        topk = std::clamp<size_t>(topk, 1, kMaxSearchTopK);
        auto hits = db.vector_search(embed_text(q, dim), topk);
        std::ostringstream js; js << "{\"results\":[";
        for (size_t i = 0; i < hits.size(); ++i) {
          auto n0 = db.get_node(hits[i].node_id);
          if (i) js << ',';
          js << "{\"id\":" << hits[i].node_id << ",\"score\":" << hits[i].score;
          if (n0) js << ",\"content\":\"" << json_escape(n0->content) << "\"";
          js << '}';
        }
        js << "]}"; out = js.str();
        }
      } else if (method == "GET" && path.rfind("/v1/nodes/", 0) == 0) {
        const std::string raw_id = path.substr(10);
        if (raw_id.empty() || !std::all_of(raw_id.begin(), raw_id.end(), [](unsigned char c){ return std::isdigit(c); })) {
          code = 400; out = "{\"error\":\"invalid_node_id\"}";
        } else {
          const auto parsed = std::stoull(raw_id);
          if (parsed > UINT32_MAX) { code = 400; out = "{\"error\":\"invalid_node_id\"}"; }
          else {
            const uint32_t id = static_cast<uint32_t>(parsed);
            auto n0 = db.get_node(id);
            if (!n0) { code = 404; out = "{\"error\":\"not found\"}"; }