         << "\"rerun_required\":" << (result.final_self_healing.rerun_required ? "true" : "false")
         << ",\"human_review_required\":" << (result.final_self_healing.human_review_required ? "true" : "false")
         << ",\"discovery_questions\":";
  append_string_array(output, result.final_self_healing.discovery_questions);
  output << "},\"receipt\":{" 
         << "\"snapshot_version\":" << result.receipt.snapshot_version
         << ",\"initial_bundle_hash\":" << result.receipt.initial_bundle_hash
         << ",\"final_bundle_hash\":" << result.receipt.final_bundle_hash
         << ",\"model_world_event_hash\":" << result.receipt.model_world_event_hash
         << ",\"expansion_rounds\":" << result.receipt.expansion_rounds
         << ",\"graphene_executed\":" << (result.receipt.graphene_executed ? "true" : "false")
         << ",\"fiber_bundle_built\":" << (result.receipt.fiber_bundle_built ? "true" : "false")
         << ",\"stability_critic_executed\":" << (result.receipt.stability_critic_executed ? "true" : "false")
         << ",\"lyapunov_trajectory_executed\":" << (result.receipt.lyapunov_trajectory_executed ? "true" : "false")
         << ",\"lyapunov_certificate_valid\":" << (result.receipt.lyapunov_certificate_valid ? "true" : "false")
         << ",\"lyapunov_goal_reached\":" << (result.receipt.lyapunov_goal_reached ? "true" : "false")
         << ",\"escape_considered\":" << (result.receipt.escape_considered ? "true" : "false")
         << ",\"convergence_executed\":" << (result.receipt.convergence_executed ? "true" : "false")
         << ",\"opposition_executed\":" << (result.receipt.opposition_executed ? "true" : "false")
         << ",\"governed_projection_executed\":" << (result.receipt.governed_projection_executed ? "true" : "false")
         << ",\"no_silent_promotion\":" << (result.receipt.no_silent_promotion ? "true" : "false")
         << "}}";
  return output.str();
}

static bool parse_episode_split(const std::string& value,
                                EpisodeSplit* output) {
  if (value == "training") *output = EpisodeSplit::Training;
  else if (value == "development") *output = EpisodeSplit::Development;
  else if (value == "evaluation") *output = EpisodeSplit::Evaluation;
  else return false;
  return true;
}

static bool parse_learning_episode_request(
    const std::string& body,
    LearningEpisodeInput* output,
    std::string* error) {
  JsonDocumentValue root;
  if (!parse_json_object(body, &root, error) ||
      !reject_unknown_json_fields(
          root,
          {"schema_version", "tenant_id", "episode_id", "family", "domain",
           "split", "query", "signature", "model_version", "policy",
           "outcome_verified", "verifier_id", "outcome_evidence_id",
           "task_success", "causal_f1", "evidence_coverage",
           "calibration_error", "latency_ms", "token_cost", "action_cost",
           "false_promotion", "harmful_action", "unauthorized_action",
           "harmful_memory_activation", "expired_truth_activation",
           "intermediate_trace_bytes", "retained_trace_bytes",
           "decisive_evidence_total", "decisive_evidence_retained",
           "useful_evidence_total", "useful_evidence_retrieved_at_20",
           "evidence_node_ids", "legal_hold"},
          "request", error)) {
    return false;
  }
  LearningEpisodeInput input;
  std::string split;
  if (!extraction_integer_field(root, "schema_version",
                                kLearningSchemaVersion,
                                &input.schema_version, error) ||
      !extraction_string_field(root, "tenant_id", 256, true,
                               &input.tenant_id, error) ||
      !extraction_string_field(root, "episode_id", 256, true,
                               &input.episode_id, error) ||
      !extraction_string_field(root, "family", 256, true,
                               &input.family, error) ||
      !extraction_string_field(root, "domain", 256, true,
                               &input.domain, error) ||
      !extraction_string_field(root, "split", 32, true, &split, error) ||
      !extraction_string_field(root, "query", kMaxNodeTextBytes, true,
                               &input.query, error) ||
      !extraction_integer_field(root, "signature", uint64_t{0},
                                &input.signature, error) ||
      !extraction_string_field(root, "model_version", 256, true,
                               &input.model_version, error) ||
      !parse_retrieval_policy(root, "policy", true, &input.policy, error) ||
      !extraction_bool_field(root, "outcome_verified", false,
                             &input.outcome_verified, error) ||
      !extraction_string_field(root, "verifier_id", 256, false,
                               &input.verifier_id, error) ||
      !extraction_string_field(root, "outcome_evidence_id", 256, false,
                               &input.outcome_evidence_id, error) ||
      !extraction_bool_field(root, "task_success", false,
                             &input.task_success, error) ||
      !extraction_double_field(root, "causal_f1", 0.0,
                               &input.causal_f1, error) ||
      !extraction_double_field(root, "evidence_coverage", 0.0,
                               &input.evidence_coverage, error) ||
      !extraction_double_field(root, "calibration_error", 0.0,
                               &input.calibration_error, error) ||
      !extraction_double_field(root, "latency_ms", 0.0,
                               &input.latency_ms, error) ||
      !extraction_double_field(root, "token_cost", 0.0,
                               &input.token_cost, error) ||
      !extraction_double_field(root, "action_cost", 0.0,
                               &input.action_cost, error) ||
      !extraction_bool_field(root, "false_promotion", false,
                             &input.false_promotion, error) ||
      !extraction_bool_field(root, "harmful_action", false,
