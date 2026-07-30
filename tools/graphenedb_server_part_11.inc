  else if (action == "rollback") input.action = PolicyDecisionAction::Rollback;
  else {
    *error = "action must be promote or rollback";
    return false;
  }
  *output = std::move(input);
  return true;
}

static std::string policy_state_json(const PolicyState& state,
                                     bool replay) {
  std::ostringstream output;
  output << "{\"node_id\":" << state.node_id
         << ",\"event_id\":\"" << json_escape(state.event_id)
         << "\",\"action\":\"" << policy_decision_action_name(state.action)
         << "\",\"policy\":";
  append_retrieval_policy(output, state.policy);
  output << ",\"previous_policy_version\":\""
         << json_escape(state.previous_policy_version)
         << "\",\"approver_id\":\"" << json_escape(state.approver_id)
         << "\",\"evaluation_reference\":\""
         << json_escape(state.evaluation_reference)
         << "\",\"reason\":\"" << json_escape(state.reason)
         << "\",\"idempotent_replay\":" << (replay ? "true" : "false")
         << '}';
  return output.str();
}

static std::string policy_evaluation_json(
    const PolicyEvaluationReport& report) {
  std::ostringstream output;
  output << "{\"baseline\":";
  append_retrieval_policy(output, report.baseline);
  output << ",\"has_recommendation\":"
         << (report.has_recommendation ? "true" : "false")
         << ",\"recommended\":";
  if (report.has_recommendation) append_retrieval_policy(output, report.recommended);
  else output << "null";
  output << ",\"development_utility_improvement\":"
         << report.development_utility_improvement
         << ",\"evaluation_episodes_excluded\":"
         << report.evaluation_episodes_excluded
         << ",\"training_episode_node_ids\":";
  append_u32_array(output, report.training_episode_node_ids);
  output << ",\"data_policy\":{\"episodes\":"
         << report.data_policy.episodes
         << ",\"trace_retention_ratio\":"
         << report.data_policy.trace_retention_ratio
         << ",\"decisive_evidence_retention\":"
         << report.data_policy.decisive_evidence_retention
         << ",\"useful_evidence_recall_at_20\":"
         << report.data_policy.useful_evidence_recall_at_20
         << ",\"harmful_memory_activation_rate\":"
         << report.data_policy.harmful_memory_activation_rate
         << ",\"expired_truth_activation_rate\":"
         << report.data_policy.expired_truth_activation_rate << '}';
  output << ",\"candidates\":[";
  for (size_t index = 0; index < report.candidates.size(); ++index) {
    if (index != 0) output << ',';
    const PolicyMetrics& metrics = report.candidates[index];
    output << "{\"policy\":";
    append_retrieval_policy(output, metrics.policy);
    output << ",\"training_samples\":" << metrics.training_samples
           << ",\"development_samples\":" << metrics.development_samples
           << ",\"safety_violations\":" << metrics.safety_violations
           << ",\"mean_training_utility\":"
           << metrics.mean_training_utility
           << ",\"mean_development_utility\":"
           << metrics.mean_development_utility
           << ",\"development_task_success\":"
           << metrics.development_task_success
           << ",\"worst_domain_regression\":"
           << metrics.worst_domain_regression
           << ",\"eligible\":" << (metrics.eligible ? "true" : "false")
           << ",\"ineligibility_reason\":\""
           << json_escape(metrics.ineligibility_reason) << "\"}";
  }
  output << "],\"retention_decisions\":[";
  for (size_t index = 0; index < report.retention_decisions.size(); ++index) {
    if (index != 0) output << ',';
    const DataRetentionDecision& decision =
        report.retention_decisions[index];
    output << "{\"episode_id\":\"" << json_escape(decision.episode_id)
           << "\",\"utility_class\":\""
           << data_utility_class_name(decision.utility_class)
           << "\",\"retain_for_training\":"
           << (decision.retain_for_training ? "true" : "false")
           << ",\"archive_intermediate_trace\":"
           << (decision.archive_intermediate_trace ? "true" : "false")
           << ",\"reason\":\"" << json_escape(decision.reason) << "\"}";
  }
  output << "],\"warnings\":";
  append_string_array(output, report.warnings);
  output << ",\"durable_writes\":"
         << (report.durable_writes ? "true" : "false") << '}';
  return output.str();
}

static bool extraction_metadata(const JsonDocumentValue& object,
                                std::map<std::string, std::string>* output,
                                std::string* error) {
  const JsonDocumentValue* metadata = json_member(object, "metadata");
  if (!metadata) return true;
  if (metadata->kind != JsonDocumentValue::Kind::Object) {
    *error = "metadata must be an object of string values";
    return false;
  }
  if (metadata->object.size() > kMaxExtractionMetadataEntries) {
    *error = "metadata exceeds " +
             std::to_string(kMaxExtractionMetadataEntries) + " entries";
    return false;
  }
  for (const auto& [key, value] : metadata->object) {
    if (key.empty() || key.size() > kMaxExtractionMetadataKeyBytes) {
      *error = "metadata key must be between 1 and " +
               std::to_string(kMaxExtractionMetadataKeyBytes) + " bytes";
      return false;
    }
    if (key.rfind("graphene_", 0) == 0) {
      *error = "metadata key '" + key +
               "' uses the reserved graphene_ namespace";
      return false;
    }
    if (value.kind != JsonDocumentValue::Kind::String) {
      *error = "metadata value for '" + key + "' must be a string";
      return false;
    }
    if (value.scalar.size() > kMaxExtractionMetadataValueBytes) {
      *error = "metadata value for '" + key + "' exceeds " +
               std::to_string(kMaxExtractionMetadataValueBytes) + " bytes";
      return false;
    }
    output->emplace(key, value.scalar);
  }
  return true;
}

static bool parse_extraction_role(const std::string& raw,
                                  ExtractionRole* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "node") *output = ExtractionRole::Node;
  else if (value == "root") *output = ExtractionRole::Root;
  else if (value == "symptom") *output = ExtractionRole::Symptom;
  else if (value == "impact") *output = ExtractionRole::Impact;
  else return false;
  return true;
}

static bool parse_strict_origin(const std::string& raw, EdgeOrigin* output) {
  const std::string value = lower_copy(raw);