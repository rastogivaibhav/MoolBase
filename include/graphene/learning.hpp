#pragma once

#include "graphene/db.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace graphene {

static constexpr uint32_t kLearningSchemaVersion = 1;

enum class EpisodeSplit : uint8_t {
  Training = 0,
  Development = 1,
  Evaluation = 2
};

enum class DataUtilityClass : uint8_t {
  Decisive = 0,
  Useful = 1,
  Redundant = 2,
  Stale = 3,
  Misleading = 4,
  Harmful = 5,
  NeverEligible = 6
};

enum class PolicyDecisionAction : uint8_t {
  Promote = 0,
  Rollback = 1
};

struct RetrievalPolicy {
  std::string version;
  uint32_t semantic_candidates{12};
  uint32_t max_hops{6};
  uint32_t max_paths{32};
  uint32_t max_paths_per_root{8};
  uint32_t max_visited_states{20000};
  uint32_t max_opposition_rounds{1};
  double minimum_confidence{0.45};
  double reexpansion_threshold{0.25};
};

bool operator==(const RetrievalPolicy& left, const RetrievalPolicy& right);
bool operator!=(const RetrievalPolicy& left, const RetrievalPolicy& right);

struct LearningEpisodeInput {
  uint32_t schema_version{kLearningSchemaVersion};
  std::string tenant_id;
  std::string episode_id;
  std::string family;
  std::string domain;
  EpisodeSplit split{EpisodeSplit::Training};
  std::string query;
  std::vector<float> vector;
  uint64_t signature{0};
  std::string model_version;
  RetrievalPolicy policy;

  bool outcome_verified{false};
  std::string verifier_id;
  std::string outcome_evidence_id;
  bool task_success{false};
  double causal_f1{0.0};
  double evidence_coverage{0.0};
  double calibration_error{0.0};
  double latency_ms{0.0};
  double token_cost{0.0};
  double action_cost{0.0};

  bool false_promotion{false};
  bool harmful_action{false};
  bool unauthorized_action{false};
  bool harmful_memory_activation{false};
  bool expired_truth_activation{false};

  uint64_t intermediate_trace_bytes{0};
  uint64_t retained_trace_bytes{0};
  uint32_t decisive_evidence_total{0};
  uint32_t decisive_evidence_retained{0};
  uint32_t useful_evidence_total{0};
  uint32_t useful_evidence_retrieved_at_20{0};
  std::vector<uint32_t> evidence_node_ids;
  bool legal_hold{false};

  bool place_missing_lattice{true};
  std::optional<LatticeCoord> lattice;
};

struct LearningEpisodeResult {
  uint32_t node_id{0};
  bool idempotent_replay{false};
  bool training_eligible{false};
  bool safety_negative{false};
  double utility{0.0};
  DataUtilityClass utility_class{DataUtilityClass::NeverEligible};
};

struct LearningEpisodeSummary {
  uint32_t node_id{0};
  std::string tenant_id;
  std::string episode_id;
  std::string family;
  std::string domain;
  EpisodeSplit split{EpisodeSplit::Training};
  std::string model_version;
  RetrievalPolicy policy;
  bool outcome_verified{false};
  bool task_success{false};
  bool safety_negative{false};
  bool harmful_memory_activation{false};
  bool expired_truth_activation{false};
  bool legal_hold{false};
  double utility{0.0};
  double causal_f1{0.0};
  double evidence_coverage{0.0};
  double calibration_error{0.0};
  uint64_t intermediate_trace_bytes{0};
  uint64_t retained_trace_bytes{0};
  uint32_t decisive_evidence_total{0};
  uint32_t decisive_evidence_retained{0};
  uint32_t useful_evidence_total{0};
  uint32_t useful_evidence_retrieved_at_20{0};
  DataUtilityClass utility_class{DataUtilityClass::NeverEligible};
};

struct DataPolicyMetrics {
  uint64_t episodes{0};
  double trace_retention_ratio{0.0};
  double decisive_evidence_retention{1.0};
  double useful_evidence_recall_at_20{1.0};
  double harmful_memory_activation_rate{0.0};
  double expired_truth_activation_rate{0.0};
};

struct DataRetentionDecision {
  std::string episode_id;
  DataUtilityClass utility_class{DataUtilityClass::NeverEligible};
  bool retain_for_training{false};
  bool archive_intermediate_trace{false};
  std::string reason;
};

struct PolicyMetrics {
  RetrievalPolicy policy;
  uint64_t training_samples{0};
  uint64_t development_samples{0};
  uint64_t safety_violations{0};
  double mean_training_utility{0.0};
  double mean_development_utility{0.0};
  double development_task_success{0.0};
  double worst_domain_regression{0.0};
  bool eligible{false};
  std::string ineligibility_reason;
};

struct PolicyLearningOptions {
  uint32_t minimum_training_samples{2};
  uint32_t minimum_development_samples{1};
  double minimum_development_utility_improvement{0.02};
  double maximum_domain_regression{0.02};
  size_t maximum_retention_decisions{1000};
};

struct PolicyEvaluationReport {
  RetrievalPolicy baseline;
  std::vector<PolicyMetrics> candidates;
  bool has_recommendation{false};
  RetrievalPolicy recommended;
  double development_utility_improvement{0.0};
  uint64_t evaluation_episodes_excluded{0};
  std::vector<uint32_t> training_episode_node_ids;
  DataPolicyMetrics data_policy;
  std::vector<DataRetentionDecision> retention_decisions;
  std::vector<std::string> warnings;
  bool durable_writes{false};
};

struct PolicyDecisionInput {
  uint32_t schema_version{kLearningSchemaVersion};
  std::string tenant_id;
  std::string event_id;
  PolicyDecisionAction action{PolicyDecisionAction::Promote};
  RetrievalPolicy policy;
  bool approved{false};
  std::string approver_id;
  std::string evaluation_reference;
  std::string reason;
  std::vector<float> vector;
  bool place_missing_lattice{true};
  std::optional<LatticeCoord> lattice;
};

struct PolicyState {
  uint32_t node_id{0};
  std::string event_id;
  PolicyDecisionAction action{PolicyDecisionAction::Promote};
  RetrievalPolicy policy;
  std::string previous_policy_version;
  std::string approver_id;
  std::string evaluation_reference;
  std::string reason;
};

struct PolicyDecisionResult {
  PolicyState state;
  bool idempotent_replay{false};
};

double learning_utility(const LearningEpisodeInput& input);
DataUtilityClass classify_data_utility(const LearningEpisodeInput& input,
                                       double utility);
const char* episode_split_name(EpisodeSplit split);
const char* data_utility_class_name(DataUtilityClass value);
const char* policy_decision_action_name(PolicyDecisionAction action);

class OutcomeLearningEngine {
 public:
  explicit OutcomeLearningEngine(GrapheneDB& db);

  Status record_episode(const LearningEpisodeInput& input,
                        LearningEpisodeResult* out = nullptr);
  std::vector<LearningEpisodeSummary> episodes(
      const std::string& tenant_id) const;
  std::vector<uint32_t> training_episode_ids(
      const std::string& tenant_id) const;
  PolicyEvaluationReport evaluate_policies(
      const std::string& tenant_id,
      const RetrievalPolicy& baseline,
      const PolicyLearningOptions& options = {}) const;

  Status record_policy_decision(const PolicyDecisionInput& input,
                                PolicyDecisionResult* out = nullptr);
  std::optional<PolicyState> current_policy(
      const std::string& tenant_id) const;
  Status quarantine_episode(const std::string& tenant_id,
                            const std::string& episode_id);

 private:
  GrapheneDB& db_;
};

}  // namespace graphene
