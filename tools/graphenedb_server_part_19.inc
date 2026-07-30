          for (const char* k : {"documented_at","valid_from","valid_until","confidence","fact_type","account","service","incident","repo","placement_policy"}) { std::string v = json_value(body, k); if (!v.empty()) ni.metadata[k] = v; }
          std::string placement_policy = json_value(body, "placement_policy");
          if (placement_policy == "semantic_causal_temporal" || placement_policy == "semantic") ni.lattice = choose_semantic_causal_temporal_cell(db, text, ni.vector, ni.metadata, next_lattice_slot);
          else ni.lattice = spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          uint32_t id = 0; st = put_node_with_lattice_retry(db, ni, &id, next_lattice_slot);
          if (!st) { code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
          else { metrics.node_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(id) + ",\"type\":\"temporal_fact\",\"q\":" + std::to_string(ni.lattice->q) + ",\"r\":" + std::to_string(ni.lattice->r) + ",\"layer\":" + std::to_string(ni.lattice->layer) + "}"; }
          }
        }
        }
      } else if (method == "POST" && path == "/v1/nodes") {
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
                code = 200; out = "{\"id\":" + std::to_string(existing.front()) + ",\"idempotent_replay\":true}"; idempotency_handled = true;
              }
            }
          }
          if (!idempotency_handled && lattice_capacity && db.node_count() >= lattice_capacity) {
            code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; idempotency_handled = true;
          }
          if (!idempotency_handled) {
          NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim);
          ni.metadata["source"] = json_value(body, "source").empty() ? "server" : json_value(body, "source");
          if (!idempotency_key.empty()) ni.metadata["_idempotency_key"] = scoped_idempotency_key;
          for (const char* k : {"documented_at","valid_from","valid_until","confidence","fact_type","account","service","incident","repo","placement_policy"}) { std::string v = json_value(body, k); if (!v.empty()) ni.metadata[k] = v; }
          std::string placement_policy = json_value(body, "placement_policy");
          if (placement_policy == "semantic_causal_temporal" || placement_policy == "semantic") ni.lattice = choose_semantic_causal_temporal_cell(db, text, ni.vector, ni.metadata, next_lattice_slot);
          else ni.lattice = spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          uint32_t id = 0; st = put_node_with_lattice_retry(db, ni, &id, next_lattice_slot);
          if (!st) { code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
          else { metrics.node_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(id) + ",\"q\":" + std::to_string(ni.lattice->q) + ",\"r\":" + std::to_string(ni.lattice->r) + ",\"layer\":" + std::to_string(ni.lattice->layer) + "}"; }
          }
        }
        }
      } else if (method == "POST" && path == "/v1/edges") {
        EdgeInput ei; ei.from = json_u32(body, "from", UINT32_MAX); ei.to = json_u32(body, "to", UINT32_MAX); ei.confidence = json_double(body, "confidence", 0.9);
        std::string role = json_value(body, "role");
        std::string origin = json_value(body, "origin");
        ei.role = parse_role(role);
        ei.origin = parse_origin(origin);
        for (const char* k : {"source_id","span","observed_at","derived_from","promotion_status","evidence_ref","reinforcement_count","salience_boost"}) { std::string v = json_value(body, k); if (!v.empty()) ei.metadata[k] = v; }
        uint32_t eid = 0; st = db.put_edge(ei, &eid);
        if (!st) { code = http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
        else { metrics.edge_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(eid) + "}"; }
      } else if (method == "POST" && path == "/v1/edges/provenance") {
        EdgeInput ei; ei.from = json_u32(body, "from", UINT32_MAX); ei.to = json_u32(body, "to", UINT32_MAX); ei.confidence = json_double(body, "confidence", 0.9);
        ei.role = parse_role(json_value(body, "role"));
        ei.origin = parse_origin(json_value(body, "origin"));
        for (const char* k : {"source_id","span","observed_at","derived_from","promotion_status","evidence_ref","reinforcement_count","salience_boost"}) { std::string v = json_value(body, k); if (!v.empty()) ei.metadata[k] = v; }
        uint32_t eid = 0; st = db.put_edge(ei, &eid);
        if (!st) { code = http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
        else { metrics.edge_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(eid) + ",\"origin\":\"" + origin_name(ei.origin) + "\",\"role\":\"" + role_name(ei.role) + "\"}"; }
      } else if (method == "POST" && (path == "/v1/retrieve/bundle" || path == "/v1/retrieve/explain")) {
        uint32_t anchor = json_u32(body, "anchor", json_u32(body, "anchor_node_id", 0));
        uint32_t hops = std::clamp<uint32_t>(json_u32(body, "max_hops", json_u32(body, "hops", 3)), 1, kMaxBundleHops);
        std::string mode = json_value(body, "mode"); if (mode.empty()) mode = "balanced";
        std::string as_of = json_value(body, "as_of"); if (as_of.empty()) as_of = json_value(body, "at");
        out = build_bundle_json(db, anchor, hops, mode, as_of);
      } else if (method == "POST" && path == "/v1/reason/dialectic") {
        const std::string query = json_value(body, "query");
        QueryMode mode = QueryMode::Balanced;
        const std::string requested_mode = json_value(body, "mode");
        if (query.empty()) {
          code = 400;
          out = "{\"error\":\"query is required\"}";
        } else if (query.size() > kMaxNodeTextBytes) {
          code = 400;
          out = "{\"error\":\"query_too_large\",\"max_bytes\":" +
                std::to_string(kMaxNodeTextBytes) + "}";
        } else if (!parse_query_mode(requested_mode, &mode)) {
          code = 400;
          out = "{\"error\":\"mode must be empirical, balanced, or theoretical\"}";
        } else {
          DialecticOptions reasoning;
          reasoning.mode = mode;
          reasoning.semantic_candidates =
              std::clamp<uint32_t>(json_u32(body, "semantic_candidates", 12),
                                   1, 64);
          reasoning.max_hops =
              std::clamp<uint32_t>(json_u32(body, "max_hops", 6), 1,
                                   kMaxDialecticHops);
          reasoning.max_paths =
              std::clamp<uint32_t>(json_u32(body, "max_paths", 32), 1,
                                   kMaxDialecticPaths);
          reasoning.max_paths_per_root = std::clamp<uint32_t>(
              json_u32(body, "max_paths_per_root", 8), 1, 64);
          reasoning.max_visited_states = std::clamp<uint32_t>(
              json_u32(body, "max_visited_states", 20000), 1,
              kMaxDialecticVisitedStates);
          reasoning.max_opposition_rounds = std::clamp<uint32_t>(
              json_u32(body, "max_opposition_rounds", 1), 0, 2);
          reasoning.minimum_confidence = std::clamp(
              json_double(body, "minimum_confidence", 0.45), 0.0, 1.0);
          reasoning.reexpansion_threshold = std::clamp(
              json_double(body, "reexpansion_threshold", 0.25), 0.0, 1.0);
          reasoning.as_of = json_value(body, "as_of");
          DialecticEngine engine(db);
          const DialecticResult result =
              engine.reason(embed_text(query, dim),
                            json_u64(body, "signature", 0), reasoning);
          out = dialectic_json(result);
        }
      } else if (method == "POST" && path == "/v1/reason/runtime") {
        const std::string query = json_value(body, "query");
        QueryMode mode = QueryMode::Balanced;
        const std::string requested_mode = json_value(body, "mode");
        if (query.empty()) {
          code = 400;
          out = "{\"error\":\"query is required\"}";
        } else if (query.size() > kMaxNodeTextBytes) {
          code = 400;
          out = "{\"error\":\"query_too_large\",\"max_bytes\":" +
                std::to_string(kMaxNodeTextBytes) + "}";
        } else if (!parse_query_mode(requested_mode, &mode)) {
          code = 400;
          out = "{\"error\":\"mode must be empirical, balanced, or theoretical\"}";
        } else {
          RuntimeOptions runtime_options;
          runtime_options.dialectic.mode = mode;
          runtime_options.dialectic.semantic_candidates =
              std::clamp<uint32_t>(json_u32(body, "semantic_candidates", 12), 1, 64);
          runtime_options.dialectic.max_hops =
              std::clamp<uint32_t>(json_u32(body, "max_hops", 6), 1, kMaxDialecticHops);
          runtime_options.dialectic.max_paths =
              std::clamp<uint32_t>(json_u32(body, "max_paths", 32), 1, kMaxDialecticPaths);
          runtime_options.dialectic.max_paths_per_root =
              std::clamp<uint32_t>(json_u32(body, "max_paths_per_root", 8), 1, 64);
          runtime_options.dialectic.max_visited_states =
              std::clamp<uint32_t>(json_u32(body, "max_visited_states", 20000), 1, kMaxDialecticVisitedStates);
          runtime_options.dialectic.minimum_confidence =
              std::clamp(json_double(body, "minimum_confidence", 0.45), 0.0, 1.0);
          runtime_options.dialectic.reexpansion_threshold =
              std::clamp(json_double(body, "reexpansion_threshold", 0.25), 0.0, 1.0);