#include "graphene/learning.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>

namespace graphene {
namespace {

constexpr size_t kMaxIdentityBytes = 256;
constexpr size_t kMaxEvidenceNodes = 256;
constexpr size_t kMaxQueryBytes = 1024 * 1024;

bool finite_unit(double value) {
  return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

bool valid_identity(const std::string& value) {
  if (value.empty() || value.size() > kMaxIdentityBytes) return false;
  for (unsigned char ch : value) {
    if (ch < 0x21 || ch == 0x7f || ch == 0x1e || ch == 0x1f) return false;
  }
  return true;
}

std::string precise(double value) {
  std::ostringstream output;
  output << std::setprecision(std::numeric_limits<double>::max_digits10)
         << value;
  return output.str();
}

std::string yes_no(bool value) {
  return value ? "true" : "false";
}

bool parse_bool(const std::string& value) {
  return value == "true";
}

double parse_double(const std::string& value, double fallback = 0.0) {
  try {
    size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    if (consumed == value.size() && std::isfinite(parsed)) return parsed;
  } catch (...) {
  }
  return fallback;
}

uint64_t parse_u64(const std::string& value, uint64_t fallback = 0) {
  try {
    size_t consumed = 0;
    const uint64_t parsed = std::stoull(value, &consumed);
    if (consumed == value.size()) return parsed;
  } catch (...) {
  }
  return fallback;
}

std::string metadata_value(const std::map<std::string, std::string>& metadata,
                           const std::string& key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

std::string episode_source(const std::string& tenant_id) {
  return "graphene-learning-episodes/" + tenant_id;
}

std::string policy_source(const std::string& tenant_id) {
  return "graphene-learning-policies/" + tenant_id;
}

std::string encode_ids(const std::vector<uint32_t>& values) {
  std::ostringstream output;
  for (size_t index = 0; index < values.size(); ++index) {
    if (index) output << ',';
    output << values[index];
  }
  return output.str();
}

Status validate_policy(const RetrievalPolicy& policy) {
  if (!valid_identity(policy.version)) {
    return Status::error(ErrorCode::InvalidInput,
                         "policy version is required and must be bounded");
  }
  if (policy.semantic_candidates < 1 || policy.semantic_candidates > 64 ||
      policy.max_hops < 1 || policy.max_hops > 16 ||
      policy.max_paths < 1 || policy.max_paths > 256 ||
      policy.max_paths_per_root < 1 || policy.max_paths_per_root > 64 ||
      policy.max_visited_states < 1 ||
      policy.max_visited_states > 200000 ||
      policy.max_opposition_rounds > 2 ||
      !finite_unit(policy.minimum_confidence) ||
      !finite_unit(policy.reexpansion_threshold)) {
    return Status::error(ErrorCode::InvalidInput,
                         "retrieval policy exceeds dialectic safety bounds");
  }
  return Status::ok();
}

void write_policy_metadata(const RetrievalPolicy& policy,
                           std::map<std::string, std::string>* metadata) {
  (*metadata)["learning_policy_version"] = policy.version;
  (*metadata)["learning_semantic_candidates"] =
      std::to_string(policy.semantic_candidates);
  (*metadata)["learning_max_hops"] = std::to_string(policy.max_hops);
  (*metadata)["learning_max_paths"] = std::to_string(policy.max_paths);
  (*metadata)["learning_max_paths_per_root"] =
      std::to_string(policy.max_paths_per_root);
  (*metadata)["learning_max_visited_states"] =
      std::to_string(policy.max_visited_states);
  (*metadata)["learning_max_opposition_rounds"] =
      std::to_string(policy.max_opposition_rounds);
  (*metadata)["learning_minimum_confidence"] =
      precise(policy.minimum_confidence);
  (*metadata)["learning_reexpansion_threshold"] =
      precise(policy.reexpansion_threshold);
}

RetrievalPolicy read_policy_metadata(
    const std::map<std::string, std::string>& metadata) {
  RetrievalPolicy policy;
  policy.version = metadata_value(metadata, "learning_policy_version");
  policy.semantic_candidates = static_cast<uint32_t>(
      parse_u64(metadata_value(metadata, "learning_semantic_candidates"), 12));
  policy.max_hops = static_cast<uint32_t>(
      parse_u64(metadata_value(metadata, "learning_max_hops"), 6));
  policy.max_paths = static_cast<uint32_t>(
      parse_u64(metadata_value(metadata, "learning_max_paths"), 32));
  policy.max_paths_per_root = static_cast<uint32_t>(
      parse_u64(metadata_value(metadata, "learning_max_paths_per_root"), 8));
  policy.max_visited_states = static_cast<uint32_t>(
      parse_u64(metadata_value(metadata, "learning_max_visited_states"),
                20000));
  policy.max_opposition_rounds = static_cast<uint32_t>(
      parse_u64(metadata_value(metadata, "learning_max_opposition_rounds"), 1));
  policy.minimum_confidence = parse_double(
      metadata_value(metadata, "learning_minimum_confidence"), 0.45);
  policy.reexpansion_threshold = parse_double(
      metadata_value(metadata, "learning_reexpansion_threshold"), 0.25);
  return policy;
}

EpisodeSplit parse_split(const std::string& value) {
  if (value == "development") return EpisodeSplit::Development;
  if (value == "evaluation") return EpisodeSplit::Evaluation;
  return EpisodeSplit::Training;
}

DataUtilityClass parse_utility_class(const std::string& value) {
  if (value == "decisive") return DataUtilityClass::Decisive;
  if (value == "useful") return DataUtilityClass::Useful;
  if (value == "redundant") return DataUtilityClass::Redundant;
  if (value == "stale") return DataUtilityClass::Stale;
  if (value == "misleading") return DataUtilityClass::Misleading;
  if (value == "harmful") return DataUtilityClass::Harmful;
  return DataUtilityClass::NeverEligible;
}

PolicyDecisionAction parse_action(const std::string& value) {
  return value == "rollback" ? PolicyDecisionAction::Rollback
                             : PolicyDecisionAction::Promote;
}

bool safety_negative(const LearningEpisodeInput& input) {
  return input.false_promotion || input.harmful_action ||
         input.unauthorized_action || input.harmful_memory_activation;
}

bool training_eligible(const LearningEpisodeInput& input) {
  return input.outcome_verified && input.split == EpisodeSplit::Training &&
         !safety_negative(input) && !input.legal_hold;
}

Status validate_episode(const GrapheneDB& db,
                        const LearningEpisodeInput& input) {
  if (input.schema_version != kLearningSchemaVersion) {
    return Status::error(ErrorCode::UnsupportedMode,
                         "unsupported learning schema version");
  }
  if (!valid_identity(input.tenant_id) ||
      !valid_identity(input.episode_id) ||
      !valid_identity(input.family) || !valid_identity(input.domain) ||
      !valid_identity(input.model_version)) {
    return Status::error(ErrorCode::InvalidInput,
                         "tenant, episode, family, domain, and model identities "
                         "are required and must be bounded");
  }
  if (input.query.empty() || input.query.size() > kMaxQueryBytes) {
    return Status::error(ErrorCode::InvalidInput,
                         "episode query is required and must be bounded");
  }
  const Status policy_status = validate_policy(input.policy);
  if (!policy_status) return policy_status;
  if (input.outcome_verified &&
      (!valid_identity(input.verifier_id) ||
       input.outcome_evidence_id.empty() ||
       input.outcome_evidence_id.size() > kMaxIdentityBytes)) {
    return Status::error(
        ErrorCode::InvalidInput,
        "verified outcomes require bounded verifier and evidence identities");
  }
  if (!finite_unit(input.causal_f1) ||
      !finite_unit(input.evidence_coverage) ||
      !finite_unit(input.calibration_error) ||
      !std::isfinite(input.latency_ms) || input.latency_ms < 0.0 ||
      !std::isfinite(input.token_cost) || input.token_cost < 0.0 ||
      !std::isfinite(input.action_cost) || input.action_cost < 0.0) {
    return Status::error(ErrorCode::InvalidInput,
                         "episode metrics must be finite and within bounds");
  }
  if (input.retained_trace_bytes > input.intermediate_trace_bytes) {
    return Status::error(ErrorCode::InvalidInput,
                         "retained trace bytes exceed intermediate bytes");
  }
  if (input.decisive_evidence_retained > input.decisive_evidence_total ||
      input.useful_evidence_retrieved_at_20 >
          input.useful_evidence_total) {
    return Status::error(ErrorCode::InvalidInput,
                         "retained/retrieved evidence counts exceed totals");
  }
  if (input.evidence_node_ids.size() > kMaxEvidenceNodes) {
    return Status::error(ErrorCode::InvalidInput,
                         "episode evidence node list exceeds 256");
  }
  std::set<uint32_t> unique_evidence;
  for (uint32_t node_id : input.evidence_node_ids) {
    if (!unique_evidence.insert(node_id).second) {
      return Status::error(ErrorCode::InvalidInput,
                           "episode contains duplicate evidence node IDs");
    }
    if (!db.get_node(node_id)) {
      return Status::error(ErrorCode::NodeNotFound,
                           "episode evidence node does not exist");
    }
  }
  return Status::ok();
}

LearningEpisodeSummary summary_from_node(const Node& node) {
  LearningEpisodeSummary summary;
  summary.node_id = node.id;
  summary.tenant_id = metadata_value(node.metadata, "learning_tenant_id");
  summary.episode_id = metadata_value(node.metadata, "learning_episode_id");
  summary.family = metadata_value(node.metadata, "learning_family");
  summary.domain = metadata_value(node.metadata, "learning_domain");
  summary.split =
      parse_split(metadata_value(node.metadata, "learning_split"));
  summary.model_version =
      metadata_value(node.metadata, "learning_model_version");
  summary.policy = read_policy_metadata(node.metadata);
  summary.outcome_verified =
      parse_bool(metadata_value(node.metadata, "learning_outcome_verified"));
  summary.task_success =
      parse_bool(metadata_value(node.metadata, "learning_task_success"));
  summary.safety_negative =
      parse_bool(metadata_value(node.metadata, "learning_safety_negative"));
  summary.harmful_memory_activation = parse_bool(
      metadata_value(node.metadata, "learning_harmful_memory_activation"));
  summary.expired_truth_activation = parse_bool(
      metadata_value(node.metadata, "learning_expired_truth_activation"));
  summary.legal_hold =
      parse_bool(metadata_value(node.metadata, "learning_legal_hold"));
  summary.utility =
      parse_double(metadata_value(node.metadata, "learning_utility"));
  summary.causal_f1 =
      parse_double(metadata_value(node.metadata, "learning_causal_f1"));
  summary.evidence_coverage =
      parse_double(metadata_value(node.metadata, "learning_evidence_coverage"));
  summary.calibration_error =
      parse_double(metadata_value(node.metadata, "learning_calibration_error"));
  summary.intermediate_trace_bytes = parse_u64(
      metadata_value(node.metadata, "learning_intermediate_trace_bytes"));
  summary.retained_trace_bytes = parse_u64(
      metadata_value(node.metadata, "learning_retained_trace_bytes"));
  summary.decisive_evidence_total = static_cast<uint32_t>(parse_u64(
      metadata_value(node.metadata, "learning_decisive_evidence_total")));
  summary.decisive_evidence_retained = static_cast<uint32_t>(parse_u64(
      metadata_value(node.metadata, "learning_decisive_evidence_retained")));
  summary.useful_evidence_total = static_cast<uint32_t>(parse_u64(
      metadata_value(node.metadata, "learning_useful_evidence_total")));
  summary.useful_evidence_retrieved_at_20 =
      static_cast<uint32_t>(parse_u64(metadata_value(
          node.metadata, "learning_useful_evidence_retrieved_at_20")));
  summary.utility_class = parse_utility_class(
      metadata_value(node.metadata, "learning_utility_class"));
  return summary;
}

PolicyState policy_state_from_node(const Node& node) {
  PolicyState state;
  state.node_id = node.id;
  state.event_id = metadata_value(node.metadata, "learning_policy_event_id");
  state.action =
      parse_action(metadata_value(node.metadata, "learning_policy_action"));
  state.policy = read_policy_metadata(node.metadata);
  state.previous_policy_version =
      metadata_value(node.metadata, "learning_previous_policy_version");
  state.approver_id =
      metadata_value(node.metadata, "learning_policy_approver_id");
  state.evaluation_reference =
      metadata_value(node.metadata, "learning_evaluation_reference");
  state.reason = metadata_value(node.metadata, "learning_policy_reason");
  return state;
}

std::vector<PolicyState> policy_states(const GrapheneDB& db,
                                       const std::string& tenant_id) {
  std::vector<PolicyState> states;
  for (uint32_t node_id :
       db.metadata_search("learning_record_type", "policy_decision")) {
    const auto node = db.get_node(node_id);
    if (!node ||
        metadata_value(node->metadata, "learning_tenant_id") != tenant_id) {
      continue;
    }
    states.push_back(policy_state_from_node(*node));
  }
  std::sort(states.begin(), states.end(),
            [](const PolicyState& left, const PolicyState& right) {
              return left.node_id < right.node_id;
            });
  return states;
}

}  // namespace

bool operator==(const RetrievalPolicy& left, const RetrievalPolicy& right) {
  return left.version == right.version &&
         left.semantic_candidates == right.semantic_candidates &&
         left.max_hops == right.max_hops &&
         left.max_paths == right.max_paths &&
         left.max_paths_per_root == right.max_paths_per_root &&
         left.max_visited_states == right.max_visited_states &&
         left.max_opposition_rounds == right.max_opposition_rounds &&
         left.minimum_confidence == right.minimum_confidence &&
         left.reexpansion_threshold == right.reexpansion_threshold;
}

bool operator!=(const RetrievalPolicy& left, const RetrievalPolicy& right) {
  return !(left == right);
}

const char* episode_split_name(EpisodeSplit split) {
  switch (split) {
    case EpisodeSplit::Training: return "training";
    case EpisodeSplit::Development: return "development";
    case EpisodeSplit::Evaluation: return "evaluation";
  }
  return "training";
}

const char* data_utility_class_name(DataUtilityClass value) {
  switch (value) {
    case DataUtilityClass::Decisive: return "decisive";
    case DataUtilityClass::Useful: return "useful";
    case DataUtilityClass::Redundant: return "redundant";
    case DataUtilityClass::Stale: return "stale";
    case DataUtilityClass::Misleading: return "misleading";
    case DataUtilityClass::Harmful: return "harmful";
    case DataUtilityClass::NeverEligible: return "never_eligible";
  }
  return "never_eligible";
}

const char* policy_decision_action_name(PolicyDecisionAction action) {
  return action == PolicyDecisionAction::Rollback ? "rollback" : "promote";
}

double learning_utility(const LearningEpisodeInput& input) {
  if (safety_negative(input)) return -1.0;
  const double decisive_retention =
      input.decisive_evidence_total == 0
          ? 1.0
          : static_cast<double>(input.decisive_evidence_retained) /
                input.decisive_evidence_total;
  const double useful_recall =
      input.useful_evidence_total == 0
          ? 1.0
          : static_cast<double>(input.useful_evidence_retrieved_at_20) /
                input.useful_evidence_total;
  const double benefit =
      0.35 * (input.task_success ? 1.0 : 0.0) +
      0.20 * input.causal_f1 + 0.15 * input.evidence_coverage +
      0.10 * decisive_retention + 0.10 * useful_recall +
      0.10 * (1.0 - input.calibration_error);
  const double cost =
      std::min(0.05, input.latency_ms * 0.00001) +
      std::min(0.05, input.token_cost * 0.001) +
      std::min(0.05, input.action_cost * 0.01);
  return std::clamp(benefit - cost, -1.0, 1.0);
}

DataUtilityClass classify_data_utility(const LearningEpisodeInput& input,
                                       double utility) {
  if (safety_negative(input)) return DataUtilityClass::Harmful;
  if (input.expired_truth_activation) return DataUtilityClass::Stale;
  if (!input.outcome_verified || input.split == EpisodeSplit::Evaluation ||
      input.legal_hold) {
    return DataUtilityClass::NeverEligible;
  }
  if (input.task_success && input.decisive_evidence_total > 0 &&
      input.decisive_evidence_retained == input.decisive_evidence_total) {
    return DataUtilityClass::Decisive;
  }
  if (!input.task_success && input.evidence_coverage >= 0.75) {
    return DataUtilityClass::Misleading;
  }
  if (utility >= 0.55 || input.task_success) return DataUtilityClass::Useful;
  return DataUtilityClass::Redundant;
}

OutcomeLearningEngine::OutcomeLearningEngine(GrapheneDB& db) : db_(db) {}

Status OutcomeLearningEngine::record_episode(
    const LearningEpisodeInput& input,
    LearningEpisodeResult* out) {
  const Status validation = validate_episode(db_, input);
  if (!validation) return validation;
  const double utility = learning_utility(input);
  const DataUtilityClass utility_class =
      classify_data_utility(input, utility);
  const bool eligible = training_eligible(input);

  ExtractionInput extraction;
  extraction.source_id = episode_source(input.tenant_id);
  extraction.source_uri = input.outcome_evidence_id;
  extraction.extraction_run_id = input.model_version;
  extraction.signature = input.signature;
  extraction.place_missing_lattice = input.place_missing_lattice;
  extraction.idempotent = true;

  ExtractionNode node;
  node.external_id = "episode/" + input.episode_id;
  node.content = input.query;
  node.vector = input.vector;
  node.signature = input.signature;
  node.metadata = {
      {"learning_record_type", "episode"},
      {"learning_schema_version", std::to_string(input.schema_version)},
      {"learning_tenant_id", input.tenant_id},
      {"learning_episode_id", input.episode_id},
      {"learning_family", input.family},
      {"learning_domain", input.domain},
      {"learning_split", episode_split_name(input.split)},
      {"learning_model_version", input.model_version},
      {"learning_outcome_verified", yes_no(input.outcome_verified)},
      {"learning_verifier_id", input.verifier_id},
      {"learning_outcome_evidence_id", input.outcome_evidence_id},
      {"learning_task_success", yes_no(input.task_success)},
      {"learning_causal_f1", precise(input.causal_f1)},
      {"learning_evidence_coverage", precise(input.evidence_coverage)},
      {"learning_calibration_error", precise(input.calibration_error)},
      {"learning_latency_ms", precise(input.latency_ms)},
      {"learning_token_cost", precise(input.token_cost)},
      {"learning_action_cost", precise(input.action_cost)},
      {"learning_false_promotion", yes_no(input.false_promotion)},
      {"learning_harmful_action", yes_no(input.harmful_action)},
      {"learning_unauthorized_action", yes_no(input.unauthorized_action)},
      {"learning_harmful_memory_activation",
       yes_no(input.harmful_memory_activation)},
      {"learning_expired_truth_activation",
       yes_no(input.expired_truth_activation)},
      {"learning_safety_negative", yes_no(safety_negative(input))},
      {"learning_training_eligible", yes_no(eligible)},
      {"learning_intermediate_trace_bytes",
       std::to_string(input.intermediate_trace_bytes)},
      {"learning_retained_trace_bytes",
       std::to_string(input.retained_trace_bytes)},
      {"learning_decisive_evidence_total",
       std::to_string(input.decisive_evidence_total)},
      {"learning_decisive_evidence_retained",
       std::to_string(input.decisive_evidence_retained)},
      {"learning_useful_evidence_total",
       std::to_string(input.useful_evidence_total)},
      {"learning_useful_evidence_retrieved_at_20",
       std::to_string(input.useful_evidence_retrieved_at_20)},
      {"learning_evidence_node_ids", encode_ids(input.evidence_node_ids)},
      {"learning_legal_hold", yes_no(input.legal_hold)},
      {"learning_utility", precise(utility)},
      {"learning_utility_class", data_utility_class_name(utility_class)}};
  write_policy_metadata(input.policy, &node.metadata);
  node.lattice = input.lattice;
  extraction.nodes.push_back(std::move(node));

  ExtractionResult result;
  const Status status = db_.put_extraction(extraction, &result);
  if (!status) return status;
  if (out) {
    const auto it =
        result.external_to_node_id.find("episode/" + input.episode_id);
    out->node_id =
        it == result.external_to_node_id.end() ? 0 : it->second;
    out->idempotent_replay = result.inserted_node_ids.empty();
    out->training_eligible = eligible;
    out->safety_negative = safety_negative(input);
    out->utility = utility;
    out->utility_class = utility_class;
  }
  return Status::ok();
}

std::vector<LearningEpisodeSummary> OutcomeLearningEngine::episodes(
    const std::string& tenant_id) const {
  std::vector<LearningEpisodeSummary> output;
  if (!valid_identity(tenant_id)) return output;
  for (uint32_t node_id :
       db_.metadata_search("learning_record_type", "episode")) {
    const auto node = db_.get_node(node_id);
    if (!node ||
        metadata_value(node->metadata, "learning_tenant_id") != tenant_id) {
      continue;
    }
    output.push_back(summary_from_node(*node));
  }
  std::sort(output.begin(), output.end(),
            [](const LearningEpisodeSummary& left,
               const LearningEpisodeSummary& right) {
              return left.node_id < right.node_id;
            });
  return output;
}

std::vector<uint32_t> OutcomeLearningEngine::training_episode_ids(
    const std::string& tenant_id) const {
  std::vector<uint32_t> output;
  for (const LearningEpisodeSummary& episode : episodes(tenant_id)) {
    if (episode.outcome_verified &&
        episode.split == EpisodeSplit::Training &&
        !episode.safety_negative && !episode.legal_hold) {
      output.push_back(episode.node_id);
    }
  }
  return output;
}

PolicyEvaluationReport OutcomeLearningEngine::evaluate_policies(
    const std::string& tenant_id,
    const RetrievalPolicy& baseline,
    const PolicyLearningOptions& options) const {
  PolicyEvaluationReport report;
  report.baseline = baseline;
  if (!validate_policy(baseline)) {
    report.warnings.push_back("INVALID_BASELINE_POLICY");
    return report;
  }
  if (options.minimum_training_samples == 0 ||
      options.minimum_development_samples == 0 ||
      !finite_unit(options.minimum_development_utility_improvement) ||
      !finite_unit(options.maximum_domain_regression)) {
    report.warnings.push_back("INVALID_LEARNING_OPTIONS");
    return report;
  }

  struct Aggregate {
    RetrievalPolicy policy;
    bool initialized{false};
    bool conflicting_configuration{false};
    double training_utility{0.0};
    double development_utility{0.0};
    uint64_t training_samples{0};
    uint64_t development_samples{0};
    uint64_t development_successes{0};
    uint64_t safety_violations{0};
    std::map<std::string, std::pair<uint64_t, uint64_t>> domain_success;
  };
  std::map<std::string, Aggregate> aggregates;

  const auto all_episodes = episodes(tenant_id);
  uint64_t trace_total = 0;
  uint64_t trace_retained = 0;
  uint64_t decisive_total = 0;
  uint64_t decisive_retained = 0;
  uint64_t useful_total = 0;
  uint64_t useful_retrieved = 0;
  uint64_t harmful_activations = 0;
  uint64_t expired_activations = 0;

  for (const LearningEpisodeSummary& episode : all_episodes) {
    ++report.data_policy.episodes;
    trace_total += episode.intermediate_trace_bytes;
    trace_retained += episode.retained_trace_bytes;
    decisive_total += episode.decisive_evidence_total;
    decisive_retained += episode.decisive_evidence_retained;
    useful_total += episode.useful_evidence_total;
    useful_retrieved += episode.useful_evidence_retrieved_at_20;
    if (episode.harmful_memory_activation) ++harmful_activations;
    if (episode.expired_truth_activation) ++expired_activations;

    if (report.retention_decisions.size() <
        options.maximum_retention_decisions) {
      DataRetentionDecision decision;
      decision.episode_id = episode.episode_id;
      decision.utility_class = episode.utility_class;
      decision.retain_for_training =
          episode.outcome_verified &&
          episode.split == EpisodeSplit::Training &&
          !episode.safety_negative && !episode.legal_hold;
      decision.archive_intermediate_trace =
          episode.utility_class != DataUtilityClass::Decisive &&
          episode.utility_class != DataUtilityClass::Useful;
      decision.reason = decision.archive_intermediate_trace
                            ? "low-utility trace is archive-eligible"
                            : "retain evidence-backed useful material";
      report.retention_decisions.push_back(std::move(decision));
    }

    if (episode.split == EpisodeSplit::Evaluation) {
      ++report.evaluation_episodes_excluded;
      continue;
    }
    if (!episode.outcome_verified) continue;
    // Legal-hold material remains discoverable for audit and aggregate data
    // policy metrics, but must not participate in fitting or candidate
    // selection. This is stricter than merely excluding it from export.
    if (episode.legal_hold) continue;
    Aggregate& aggregate = aggregates[episode.policy.version];
    if (!aggregate.initialized) {
      aggregate.policy = episode.policy;
      aggregate.initialized = true;
    } else if (aggregate.policy != episode.policy) {
      aggregate.conflicting_configuration = true;
    }
    if (episode.split == EpisodeSplit::Training) {
      ++aggregate.training_samples;
      aggregate.training_utility += episode.utility;
      if (!episode.safety_negative && !episode.legal_hold) {
        report.training_episode_node_ids.push_back(episode.node_id);
      }
    } else {
      ++aggregate.development_samples;
      aggregate.development_utility += episode.utility;
      if (episode.task_success) ++aggregate.development_successes;
      auto& domain = aggregate.domain_success[episode.domain];
      ++domain.second;
      if (episode.task_success) ++domain.first;
    }
    if (episode.safety_negative) ++aggregate.safety_violations;
  }

  report.data_policy.trace_retention_ratio =
      trace_total == 0 ? 0.0
                       : static_cast<double>(trace_retained) / trace_total;
  report.data_policy.decisive_evidence_retention =
      decisive_total == 0
          ? 1.0
          : static_cast<double>(decisive_retained) / decisive_total;
  report.data_policy.useful_evidence_recall_at_20 =
      useful_total == 0
          ? 1.0
          : static_cast<double>(useful_retrieved) / useful_total;
  report.data_policy.harmful_memory_activation_rate =
      all_episodes.empty()
          ? 0.0
          : static_cast<double>(harmful_activations) / all_episodes.size();
  report.data_policy.expired_truth_activation_rate =
      all_episodes.empty()
          ? 0.0
          : static_cast<double>(expired_activations) / all_episodes.size();

  auto metrics_for = [&](const Aggregate& aggregate) {
    PolicyMetrics metrics;
    metrics.policy = aggregate.policy;
    metrics.training_samples = aggregate.training_samples;
    metrics.development_samples = aggregate.development_samples;
    metrics.safety_violations = aggregate.safety_violations;
    metrics.mean_training_utility =
        aggregate.training_samples == 0
            ? 0.0
            : aggregate.training_utility / aggregate.training_samples;
    metrics.mean_development_utility =
        aggregate.development_samples == 0
            ? 0.0
            : aggregate.development_utility / aggregate.development_samples;
    metrics.development_task_success =
        aggregate.development_samples == 0
            ? 0.0
            : static_cast<double>(aggregate.development_successes) /
                  aggregate.development_samples;
    if (aggregate.conflicting_configuration) {
      metrics.ineligibility_reason =
          "one policy version maps to conflicting configurations";
    } else if (aggregate.training_samples <
               options.minimum_training_samples) {
      metrics.ineligibility_reason = "insufficient training samples";
    } else if (aggregate.development_samples <
               options.minimum_development_samples) {
      metrics.ineligibility_reason = "insufficient development samples";
    } else if (aggregate.safety_violations != 0) {
      metrics.ineligibility_reason = "policy has safety-negative episodes";
    } else {
      metrics.eligible = true;
    }
    return metrics;
  };

  const auto baseline_it = aggregates.find(baseline.version);
  if (baseline_it == aggregates.end() ||
      baseline_it->second.policy != baseline) {
    report.warnings.push_back("BASELINE_EPISODES_NOT_FOUND");
  }

  for (const auto& [version, aggregate] : aggregates) {
    (void)version;
    PolicyMetrics metrics = metrics_for(aggregate);
    if (baseline_it != aggregates.end() &&
        baseline_it->second.policy == baseline &&
        aggregate.policy.version != baseline.version) {
      double worst_regression = 0.0;
      for (const auto& [domain_name, candidate_domain] :
           aggregate.domain_success) {
        const auto baseline_domain =
            baseline_it->second.domain_success.find(domain_name);
        if (baseline_domain == baseline_it->second.domain_success.end() ||
            baseline_domain->second.second == 0 ||
            candidate_domain.second == 0) {
          continue;
        }
        const double candidate_rate =
            static_cast<double>(candidate_domain.first) /
            candidate_domain.second;
        const double baseline_rate =
            static_cast<double>(baseline_domain->second.first) /
            baseline_domain->second.second;
        worst_regression =
            std::max(worst_regression, baseline_rate - candidate_rate);
      }
      metrics.worst_domain_regression = worst_regression;
      if (metrics.eligible &&
          worst_regression > options.maximum_domain_regression) {
        metrics.eligible = false;
        metrics.ineligibility_reason =
            "development domain regression exceeds configured limit";
      }
    }
    report.candidates.push_back(std::move(metrics));
  }

  if (baseline_it != aggregates.end() &&
      baseline_it->second.policy == baseline) {
    const double baseline_utility =
        baseline_it->second.development_samples == 0
            ? 0.0
            : baseline_it->second.development_utility /
                  baseline_it->second.development_samples;
    const PolicyMetrics* best = nullptr;
    for (const PolicyMetrics& candidate : report.candidates) {
      if (!candidate.eligible ||
          candidate.policy.version == baseline.version) {
        continue;
      }
      const double improvement =
          candidate.mean_development_utility - baseline_utility;
      if (improvement <
          options.minimum_development_utility_improvement) {
        continue;
      }
      if (!best ||
          candidate.mean_development_utility >
              best->mean_development_utility ||
          (candidate.mean_development_utility ==
               best->mean_development_utility &&
           candidate.policy.version < best->policy.version)) {
        best = &candidate;
      }
    }
    if (best) {
      report.has_recommendation = true;
      report.recommended = best->policy;
      report.development_utility_improvement =
          best->mean_development_utility - baseline_utility;
    } else {
      report.warnings.push_back("NO_POLICY_PASSED_PROMOTION_GATES");
    }
  }
  report.durable_writes = false;
  return report;
}

Status OutcomeLearningEngine::record_policy_decision(
    const PolicyDecisionInput& input,
    PolicyDecisionResult* out) {
  if (input.schema_version != kLearningSchemaVersion) {
    return Status::error(ErrorCode::UnsupportedMode,
                         "unsupported learning schema version");
  }
  if (!valid_identity(input.tenant_id) || !valid_identity(input.event_id) ||
      !input.approved || !valid_identity(input.approver_id) ||
      input.evaluation_reference.empty() ||
      input.evaluation_reference.size() > kMaxIdentityBytes ||
      input.reason.empty() || input.reason.size() > 4096) {
    return Status::error(
        ErrorCode::InvalidInput,
        "policy decisions require bounded identity, explicit approval, "
        "approver, evaluation reference, and reason");
  }
  const Status policy_status = validate_policy(input.policy);
  if (!policy_status) return policy_status;

  const auto states = policy_states(db_, input.tenant_id);
  std::optional<PolicyState> existing_event;
  for (const PolicyState& state : states) {
    if (state.event_id == input.event_id) existing_event = state;
  }
  if (input.action == PolicyDecisionAction::Rollback) {
    const bool target_exists = std::any_of(
        states.begin(), states.end(), [&](const PolicyState& state) {
          return state.policy == input.policy;
        });
    if (!target_exists) {
      return Status::error(ErrorCode::InvalidInput,
                           "rollback target is not present in policy history");
    }
  }
  const std::string previous =
      existing_event
          ? existing_event->previous_policy_version
          : (states.empty() ? std::string{} : states.back().policy.version);

  ExtractionInput extraction;
  extraction.source_id = policy_source(input.tenant_id);
  extraction.source_uri = input.evaluation_reference;
  extraction.extraction_run_id = input.approver_id;
  extraction.place_missing_lattice = input.place_missing_lattice;
  extraction.idempotent = true;

  ExtractionNode node;
  node.external_id = "decision/" + input.event_id;
  node.content = std::string(policy_decision_action_name(input.action)) +
                 " retrieval policy " + input.policy.version + ": " +
                 input.reason;
  node.vector = input.vector;
  node.metadata = {
      {"learning_record_type", "policy_decision"},
      {"learning_schema_version", std::to_string(input.schema_version)},
      {"learning_tenant_id", input.tenant_id},
      {"learning_policy_event_id", input.event_id},
      {"learning_policy_action",
       policy_decision_action_name(input.action)},
      {"learning_policy_approved", "true"},
      {"learning_policy_approver_id", input.approver_id},
      {"learning_evaluation_reference", input.evaluation_reference},
      {"learning_policy_reason", input.reason},
      {"learning_previous_policy_version", previous}};
  write_policy_metadata(input.policy, &node.metadata);
  node.lattice = input.lattice;
  extraction.nodes.push_back(std::move(node));

  ExtractionResult result;
  const Status status = db_.put_extraction(extraction, &result);
  if (!status) return status;
  const auto it =
      result.external_to_node_id.find("decision/" + input.event_id);
  const uint32_t node_id =
      it == result.external_to_node_id.end() ? 0 : it->second;
  const auto stored = db_.get_node(node_id);
  if (!stored) {
    return Status::error(ErrorCode::DataCorrupt,
                         "committed policy decision cannot be read");
  }
  if (out) {
    out->state = policy_state_from_node(*stored);
    out->idempotent_replay = result.inserted_node_ids.empty();
  }
  return Status::ok();
}

std::optional<PolicyState> OutcomeLearningEngine::current_policy(
    const std::string& tenant_id) const {
  if (!valid_identity(tenant_id)) return std::nullopt;
  const auto states = policy_states(db_, tenant_id);
  if (states.empty()) return std::nullopt;
  return states.back();
}

Status OutcomeLearningEngine::quarantine_episode(
    const std::string& tenant_id,
    const std::string& episode_id) {
  if (!valid_identity(tenant_id) || !valid_identity(episode_id)) {
    return Status::error(ErrorCode::InvalidInput,
                         "tenant and episode identities are required");
  }
  std::optional<Node> target;
  for (uint32_t node_id :
       db_.metadata_search("learning_episode_id", episode_id)) {
    const auto node = db_.get_node(node_id);
    if (node &&
        metadata_value(node->metadata, "learning_tenant_id") == tenant_id) {
      target = node;
    }
  }
  if (!target) {
    return Status::error(ErrorCode::NodeNotFound,
                         "learning episode not found");
  }
  if (parse_bool(metadata_value(target->metadata, "learning_legal_hold"))) {
    return Status::error(ErrorCode::InvalidInput,
                         "learning episode is under legal hold");
  }
  return db_.delete_node(target->id);
}

}  // namespace graphene
