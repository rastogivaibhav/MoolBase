      !extraction_integer_field(*value, "max_paths_per_root", uint32_t{8},
                                &policy.max_paths_per_root, error) ||
      !extraction_integer_field(*value, "max_visited_states",
                                uint32_t{20000},
                                &policy.max_visited_states, error) ||
      !extraction_integer_field(*value, "max_opposition_rounds",
                                uint32_t{1},
                                &policy.max_opposition_rounds, error) ||
      !extraction_double_field(*value, "minimum_confidence", 0.45,
                               &policy.minimum_confidence, error) ||
      !extraction_double_field(*value, "reexpansion_threshold", 0.25,
                               &policy.reexpansion_threshold, error)) {
    *error = field + ": " + *error;
    return false;
  }
  *output = std::move(policy);
  return true;
}

static void append_retrieval_policy(std::ostringstream& output,
                                    const RetrievalPolicy& policy) {
  output << "{\"version\":\"" << json_escape(policy.version)
         << "\",\"semantic_candidates\":" << policy.semantic_candidates
         << ",\"max_hops\":" << policy.max_hops
         << ",\"max_paths\":" << policy.max_paths
         << ",\"max_paths_per_root\":" << policy.max_paths_per_root
         << ",\"max_visited_states\":" << policy.max_visited_states
         << ",\"max_opposition_rounds\":" << policy.max_opposition_rounds
         << ",\"minimum_confidence\":" << policy.minimum_confidence
         << ",\"reexpansion_threshold\":" << policy.reexpansion_threshold
         << '}';
}

static DialecticOptions dialectic_options_for_policy(
    const RetrievalPolicy& policy) {
  DialecticOptions options;
  options.semantic_candidates = policy.semantic_candidates;
  options.max_hops = policy.max_hops;
  options.max_paths = policy.max_paths;
  options.max_paths_per_root = policy.max_paths_per_root;
  options.max_visited_states = policy.max_visited_states;
  options.max_opposition_rounds = policy.max_opposition_rounds;
  options.minimum_confidence = policy.minimum_confidence;
  options.reexpansion_threshold = policy.reexpansion_threshold;
  return options;
}

struct HypoKoshRequest {
  std::string query;
  std::string tenant_id;
  uint64_t signature{0};
  size_t max_hypotheses{8};
  bool use_active_policy{false};
  DialecticOptions options;
};

static bool parse_hypokosh_request(const std::string& body,
                                   HypoKoshRequest* output,
                                   std::string* error) {
  JsonDocumentValue root;
  if (!parse_json_object(body, &root, error) ||
      !reject_unknown_json_fields(
          root,
          {"query", "tenant_id", "signature", "max_hypotheses",
           "use_active_policy", "mode", "as_of", "semantic_candidates",
           "max_hops", "max_paths", "max_paths_per_root",
           "max_visited_states", "max_opposition_rounds",
           "minimum_confidence", "reexpansion_threshold"},
          "request", error)) {
    return false;
  }
  HypoKoshRequest request;
  std::string mode;
  uint32_t max_hypotheses = 8;
  if (!extraction_string_field(root, "query", kMaxNodeTextBytes, true,
                               &request.query, error) ||
      !extraction_string_field(root, "tenant_id", 256, false,
                               &request.tenant_id, error) ||
      !extraction_integer_field(root, "signature", uint64_t{0},
                                &request.signature, error) ||
      !extraction_integer_field(root, "max_hypotheses", uint32_t{8},
                                &max_hypotheses, error) ||
      !extraction_bool_field(root, "use_active_policy", false,
                             &request.use_active_policy, error) ||
      !extraction_string_field(root, "mode", 32, false, &mode, error) ||
      !extraction_string_field(root, "as_of", 128, false,
                               &request.options.as_of, error) ||
      !extraction_integer_field(root, "semantic_candidates", size_t{12},
                                &request.options.semantic_candidates, error) ||
      !extraction_integer_field(root, "max_hops", uint32_t{6},
                                &request.options.max_hops, error) ||
      !extraction_integer_field(root, "max_paths", size_t{32},
                                &request.options.max_paths, error) ||
      !extraction_integer_field(root, "max_paths_per_root", size_t{8},
                                &request.options.max_paths_per_root, error) ||
      !extraction_integer_field(root, "max_visited_states",
                                size_t{20000},
                                &request.options.max_visited_states, error) ||
      !extraction_integer_field(root, "max_opposition_rounds", uint32_t{1},
                                &request.options.max_opposition_rounds,
                                error) ||
      !extraction_double_field(root, "minimum_confidence", 0.45,
                               &request.options.minimum_confidence, error) ||
      !extraction_double_field(root, "reexpansion_threshold", 0.25,
                               &request.options.reexpansion_threshold,
                               error)) {
    return false;
  }
  if (!parse_query_mode(mode, &request.options.mode)) {
    *error = "mode must be empirical, balanced, or theoretical";
    return false;
  }
  if (max_hypotheses < 1 || max_hypotheses > 16) {
    *error = "max_hypotheses must be between 1 and 16";
    return false;
  }
  if (request.options.semantic_candidates < 1 ||
      request.options.semantic_candidates > 64 ||
      request.options.max_hops < 1 ||
      request.options.max_hops > kMaxDialecticHops ||
      request.options.max_paths < 1 ||
      request.options.max_paths > kMaxDialecticPaths ||
      request.options.max_paths_per_root < 1 ||
      request.options.max_paths_per_root > 64 ||
      request.options.max_visited_states < 1 ||
      request.options.max_visited_states > kMaxDialecticVisitedStates ||
      request.options.max_opposition_rounds > 2 ||
      request.options.minimum_confidence < 0.0 ||
      request.options.minimum_confidence > 1.0 ||
      request.options.reexpansion_threshold < 0.0 ||
      request.options.reexpansion_threshold > 1.0) {
    *error = "dialectic options exceed governed safety bounds";
    return false;
  }
  if (request.use_active_policy && request.tenant_id.empty()) {
    *error = "tenant_id is required when use_active_policy is true";
    return false;
  }
  request.max_hypotheses = max_hypotheses;
  *output = std::move(request);
  return true;
}

static std::string hypokosh_json(const HypothesisSet& hypotheses,
                                 const std::optional<PolicyState>& active) {
  std::ostringstream output;
  output << "{\"snapshot_version\":" << hypotheses.snapshot_version
         << ",\"epistemic_status\":\""
         << json_escape(hypotheses.epistemic_status)
         << "\",\"durable_writes\":"