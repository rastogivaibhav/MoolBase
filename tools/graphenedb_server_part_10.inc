                             &input.harmful_action, error) ||
      !extraction_bool_field(root, "unauthorized_action", false,
                             &input.unauthorized_action, error) ||
      !extraction_bool_field(root, "harmful_memory_activation", false,
                             &input.harmful_memory_activation, error) ||
      !extraction_bool_field(root, "expired_truth_activation", false,
                             &input.expired_truth_activation, error) ||
      !extraction_integer_field(root, "intermediate_trace_bytes", uint64_t{0},
                                &input.intermediate_trace_bytes, error) ||
      !extraction_integer_field(root, "retained_trace_bytes", uint64_t{0},
                                &input.retained_trace_bytes, error) ||
      !extraction_integer_field(root, "decisive_evidence_total", uint32_t{0},
                                &input.decisive_evidence_total, error) ||
      !extraction_integer_field(root, "decisive_evidence_retained",
                                uint32_t{0},
                                &input.decisive_evidence_retained, error) ||
      !extraction_integer_field(root, "useful_evidence_total", uint32_t{0},
                                &input.useful_evidence_total, error) ||
      !extraction_integer_field(root, "useful_evidence_retrieved_at_20",
                                uint32_t{0},
                                &input.useful_evidence_retrieved_at_20,
                                error) ||
      !extraction_bool_field(root, "legal_hold", false,
                             &input.legal_hold, error)) {
    return false;
  }
  if (!parse_episode_split(split, &input.split)) {
    *error = "split must be training, development, or evaluation";
    return false;
  }
  const JsonDocumentValue* evidence = json_member(root, "evidence_node_ids");
  if (evidence) {
    if (evidence->kind != JsonDocumentValue::Kind::Array ||
        evidence->array.size() > 256) {
      *error = "evidence_node_ids must be an array of at most 256 integers";
      return false;
    }
    for (const JsonDocumentValue& value : evidence->array) {
      if (value.kind != JsonDocumentValue::Kind::Number ||
          value.scalar.find_first_of(".eE") != std::string::npos) {
        *error = "evidence_node_ids values must be integers";
        return false;
      }
      uint32_t node_id = 0;
      const auto parsed = std::from_chars(
          value.scalar.data(), value.scalar.data() + value.scalar.size(),
          node_id);
      if (parsed.ec != std::errc{} ||
          parsed.ptr != value.scalar.data() + value.scalar.size()) {
        *error = "evidence_node_ids value is outside uint32 range";
        return false;
      }
      input.evidence_node_ids.push_back(node_id);
    }
  }
  *output = std::move(input);
  return true;
}

static bool parse_policy_evaluation_request(
    const std::string& body,
    std::string* tenant_id,
    RetrievalPolicy* baseline,
    PolicyLearningOptions* options,
    std::string* error) {
  JsonDocumentValue root;
  if (!parse_json_object(body, &root, error) ||
      !reject_unknown_json_fields(root, {"tenant_id", "baseline", "options"},
                                  "request", error) ||
      !extraction_string_field(root, "tenant_id", 256, true, tenant_id,
                               error) ||
      !parse_retrieval_policy(root, "baseline", true, baseline, error)) {
    return false;
  }
  const JsonDocumentValue* value = json_member(root, "options");
  if (!value) return true;
  if (value->kind != JsonDocumentValue::Kind::Object ||
      !reject_unknown_json_fields(
          *value,
          {"minimum_training_samples", "minimum_development_samples",
           "minimum_development_utility_improvement",
           "maximum_domain_regression", "maximum_retention_decisions"},
          "options", error)) {
    if (value->kind != JsonDocumentValue::Kind::Object) {
      *error = "options must be an object";
    }
    return false;
  }
  uint64_t retention_limit = options->maximum_retention_decisions;
  if (!extraction_integer_field(*value, "minimum_training_samples",
                                options->minimum_training_samples,
                                &options->minimum_training_samples, error) ||
      !extraction_integer_field(*value, "minimum_development_samples",
                                options->minimum_development_samples,
                                &options->minimum_development_samples, error) ||
      !extraction_double_field(
          *value, "minimum_development_utility_improvement",
          options->minimum_development_utility_improvement,
          &options->minimum_development_utility_improvement, error) ||
      !extraction_double_field(*value, "maximum_domain_regression",
                               options->maximum_domain_regression,
                               &options->maximum_domain_regression, error) ||
      !extraction_integer_field(*value, "maximum_retention_decisions",
                                retention_limit, &retention_limit, error)) {
    return false;
  }
  if (retention_limit > 10000) {
    *error = "maximum_retention_decisions exceeds 10000";
    return false;
  }
  options->maximum_retention_decisions =
      static_cast<size_t>(retention_limit);
  return true;
}

static bool parse_policy_decision_request(
    const std::string& body,
    PolicyDecisionInput* output,
    std::string* error) {
  JsonDocumentValue root;
  if (!parse_json_object(body, &root, error) ||
      !reject_unknown_json_fields(
          root,
          {"schema_version", "tenant_id", "event_id", "action", "policy",
           "approved", "approver_id", "evaluation_reference", "reason"},
          "request", error)) {
    return false;
  }
  PolicyDecisionInput input;
  std::string action;
  if (!extraction_integer_field(root, "schema_version",
                                kLearningSchemaVersion,
                                &input.schema_version, error) ||
      !extraction_string_field(root, "tenant_id", 256, true,
                               &input.tenant_id, error) ||
      !extraction_string_field(root, "event_id", 256, true,
                               &input.event_id, error) ||
      !extraction_string_field(root, "action", 32, true, &action, error) ||
      !parse_retrieval_policy(root, "policy", true, &input.policy, error) ||
      !extraction_bool_field(root, "approved", false, &input.approved,
                             error) ||
      !extraction_string_field(root, "approver_id", 256, true,
                               &input.approver_id, error) ||
      !extraction_string_field(root, "evaluation_reference", 256, true,
                               &input.evaluation_reference, error) ||
      !extraction_string_field(root, "reason", 4096, true,
                               &input.reason, error)) {
    return false;
  }
  if (action == "promote") input.action = PolicyDecisionAction::Promote;