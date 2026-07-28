          runtime_options.dialectic.as_of = json_value(body, "as_of");
          runtime_options.max_recursive_cycles =
              std::clamp<uint32_t>(json_u32(body, "max_recursive_cycles", 2), 0, 3);
          ModelWorld world;
          CompleteHypoKoshRuntime runtime(db, &world);
          const auto result = runtime.reason(
              embed_text(query, dim), json_u64(body, "signature", 0), runtime_options);
          out = hypokosh_runtime_json(result);
        }
      } else if (method == "POST" && path == "/v1/reason/hypokosh") {
        HypoKoshRequest request;
        std::string parse_error;
        if (!parse_hypokosh_request(body, &request, &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_hypokosh_request\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else {
          OutcomeLearningEngine learner(db);
          std::optional<PolicyState> active;
          if (request.use_active_policy) {
            active = learner.current_policy(request.tenant_id);
            if (!active) {
              code = 404;
              out = "{\"error\":\"active_policy_not_found\"}";
            } else {
              const QueryMode mode = request.options.mode;
              const std::string as_of = request.options.as_of;
              request.options =
                  dialectic_options_for_policy(active->policy);
              request.options.mode = mode;
              request.options.as_of = as_of;
            }
          }
          if (code == 200) {
            const HypothesisSet hypotheses = HypoKoshEngine(db).propose(
                embed_text(request.query, dim), request.signature,
                request.options, request.max_hypotheses);
            out = hypokosh_json(hypotheses, active);
          }
        }
      } else if (method == "POST" && path == "/v1/learning/episodes") {
        LearningEpisodeInput episode;
        std::string parse_error;
        if (!parse_learning_episode_request(body, &episode, &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_learning_episode\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else if (lattice_capacity &&
                   db.node_count() >= lattice_capacity) {
          code = 507;
          out = "{\"error\":\"physical_lattice_capacity_exhausted\"}";
        } else {
          episode.vector = embed_text(episode.query, dim);
          episode.place_missing_lattice = false;
          episode.lattice = spiral_coord(
              next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          LearningEpisodeResult result;
          st = OutcomeLearningEngine(db).record_episode(episode, &result);
          if (!st) {
            code = st.message.find("idempotency conflict") != std::string::npos
                       ? 409
                       : http_status_for(st);
            out = "{\"error\":\"learning_episode_failed\",\"detail\":\"" +
                  json_escape(st.message) + "\"}";
          } else {
            metrics.node_inserts.fetch_add(
                result.idempotent_replay ? 0 : 1,
                std::memory_order_relaxed);
            code = result.idempotent_replay ? 200 : 201;
            std::ostringstream response_body;
            response_body
                << "{\"node_id\":" << result.node_id
                << ",\"idempotent_replay\":"
                << (result.idempotent_replay ? "true" : "false")
                << ",\"training_eligible\":"
                << (result.training_eligible ? "true" : "false")
                << ",\"safety_negative\":"
                << (result.safety_negative ? "true" : "false")
                << ",\"utility\":" << result.utility
                << ",\"utility_class\":\""
                << data_utility_class_name(result.utility_class) << "\"}";
            out = response_body.str();
          }
        }
      } else if (method == "POST" &&
                 path == "/v1/learning/policies/evaluate") {
        std::string tenant_id;
        RetrievalPolicy baseline;
        PolicyLearningOptions learning_options;
        std::string parse_error;
        if (!parse_policy_evaluation_request(
                body, &tenant_id, &baseline, &learning_options,
                &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_policy_evaluation\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else {
          const size_t nodes_before = db.node_count();
          const PolicyEvaluationReport report =
              OutcomeLearningEngine(db).evaluate_policies(
                  tenant_id, baseline, learning_options);
          if (std::find(report.warnings.begin(), report.warnings.end(),
                        "INVALID_BASELINE_POLICY") != report.warnings.end() ||
              std::find(report.warnings.begin(), report.warnings.end(),
                        "INVALID_LEARNING_OPTIONS") !=
                  report.warnings.end()) {
            code = 400;
          }
          out = policy_evaluation_json(report);
          if (db.node_count() != nodes_before) {
            code = 500;
            out = "{\"error\":\"read_only_evaluation_wrote_data\"}";
          }
        }
      } else if (method == "POST" &&
                 path == "/v1/learning/policies/decisions") {
        PolicyDecisionInput decision_input;
        std::string parse_error;
        if (!parse_policy_decision_request(
                body, &decision_input, &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_policy_decision\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else if (lattice_capacity &&
                   db.node_count() >= lattice_capacity) {
          code = 507;
          out = "{\"error\":\"physical_lattice_capacity_exhausted\"}";
        } else {
          decision_input.vector =
              embed_text(decision_input.reason, dim);
          decision_input.place_missing_lattice = false;
          decision_input.lattice = spiral_coord(
              next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          PolicyDecisionResult result;
          st = OutcomeLearningEngine(db).record_policy_decision(
              decision_input, &result);
          if (!st) {
            code = st.message.find("idempotency conflict") != std::string::npos
                       ? 409
                       : http_status_for(st);
            out = "{\"error\":\"policy_decision_failed\",\"detail\":\"" +
                  json_escape(st.message) + "\"}";
          } else {
            metrics.node_inserts.fetch_add(
                result.idempotent_replay ? 0 : 1,
                std::memory_order_relaxed);
            code = result.idempotent_replay ? 200 : 201;
            out = policy_state_json(result.state,
                                    result.idempotent_replay);
          }