#include "graphene/hypokosh.hpp"
#include "graphene/learning.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << "FAIL " << operation << ": " << status.message << "\n";
    std::abort();
  }
}

void reject(Status status, const char* operation) {
  if (status) {
    std::cerr << "FAIL " << operation << ": unexpectedly succeeded\n";
    std::abort();
  }
}

RetrievalPolicy policy(std::string version,
                       uint32_t candidates,
                       double confidence) {
  RetrievalPolicy value;
  value.version = std::move(version);
  value.semantic_candidates = candidates;
  value.minimum_confidence = confidence;
  return value;
}

LearningEpisodeInput episode(std::string id,
                             EpisodeSplit split,
                             const RetrievalPolicy& retrieval,
                             bool success,
                             double quality) {
  LearningEpisodeInput input;
  input.tenant_id = "merchant-risk";
  input.episode_id = std::move(id);
  input.family = "incident-diagnosis";
  input.domain = "checkout";
  input.split = split;
  input.query = "Why did checkout latency increase?";
  input.vector = {0.0f, 1.0f, 0.0f, 0.0f};
  input.signature = signature_for(4, 11);
  input.model_version = "reasoner-1";
  input.policy = retrieval;
  input.outcome_verified = true;
  input.verifier_id = "postmortem-reviewer";
  input.outcome_evidence_id = "postmortem-2026-07-24";
  input.task_success = success;
  input.causal_f1 = quality;
  input.evidence_coverage = quality;
  input.calibration_error = 1.0 - quality;
  input.latency_ms = 25.0;
  input.token_cost = 0.2;
  input.action_cost = 0.1;
  input.intermediate_trace_bytes = 1000;
  input.retained_trace_bytes = success ? 200 : 600;
  input.decisive_evidence_total = 2;
  input.decisive_evidence_retained = success ? 2 : 1;
  input.useful_evidence_total = 4;
  input.useful_evidence_retrieved_at_20 = success ? 4 : 2;
  return input;
}

PolicyDecisionInput decision(std::string event_id,
                             PolicyDecisionAction action,
                             const RetrievalPolicy& retrieval) {
  PolicyDecisionInput input;
  input.tenant_id = "merchant-risk";
  input.event_id = std::move(event_id);
  input.action = action;
  input.policy = retrieval;
  input.approved = true;
  input.approver_id = "memory-governance-board";
  input.evaluation_reference = "eval://GDB-GL-0/run-1";
  input.reason = "passed preregistered development and safety gates";
  input.vector = {0.2f, 0.2f, 0.2f, 0.2f};
  return input;
}

bool contains_id(const std::vector<uint32_t>& values, uint32_t id) {
  return std::find(values.begin(), values.end(), id) != values.end();
}

}  // namespace

int main() {
  const fs::path directory =
      fs::temp_directory_path() / "graphenedb_governed_learning_tests";
  fs::remove_all(directory);

  DBOptions options;
  options.dimension = 4;
  options.fsync_on_commit = false;
  GrapheneDB db;
  require(db.open(directory, options), "open");

  const uint64_t signature = signature_for(4, 11);
  NodeInput root_input;
  root_input.content = "Connection pool exhaustion caused checkout latency";
  root_input.vector = {1.0f, 0.0f, 0.0f, 0.0f};
  root_input.signature = signature;
  root_input.root = true;
  uint32_t root = 0;
  require(db.put_node(root_input, &root), "put root");

  NodeInput symptom_input;
  symptom_input.content = "Checkout latency exceeded the SLO";
  symptom_input.vector = {0.0f, 1.0f, 0.0f, 0.0f};
  symptom_input.signature = signature;
  symptom_input.symptom = true;
  uint32_t symptom = 0;
  require(db.put_node(symptom_input, &symptom), "put symptom");

  uint32_t evidence_edge = 0;
  require(db.put_edge({root, symptom, EdgeOrigin::Observed, EdgeRole::Causal,
                       0.95, {{"source_id", "incident-postmortem"}}},
                      &evidence_edge),
          "put causal evidence");

  const size_t node_count_before_hypotheses = db.node_count();
  const size_t edge_count_before_hypotheses = db.edge_count();
  DialecticOptions dialectic;
  dialectic.semantic_candidates = 4;
  dialectic.max_hops = 4;
  dialectic.max_paths = 8;
  dialectic.max_paths_per_root = 4;
  HypothesisSet hypotheses =
      HypoKoshEngine(db).propose(symptom_input.vector, signature, dialectic, 2);
  assert(!hypotheses.proposals.empty());
  assert(hypotheses.proposals.size() <= 2);
  for (const HypothesisProposal& proposal : hypotheses.proposals) {
    assert(proposal.proposal_origin == EdgeOrigin::Hypothetical);
    assert(!proposal.eligible_for_truth_promotion);
  }
  assert(!hypotheses.durable_writes);
  assert(db.node_count() == node_count_before_hypotheses);
  assert(db.edge_count() == edge_count_before_hypotheses);

  OutcomeLearningEngine learner(db);
  const RetrievalPolicy baseline = policy("baseline-v1", 8, 0.55);
  const RetrievalPolicy candidate = policy("candidate-v2", 16, 0.45);
  const RetrievalPolicy unsafe = policy("unsafe-v1", 32, 0.20);
  const RetrievalPolicy regressing = policy("regressing-v1", 24, 0.40);

  std::vector<LearningEpisodeInput> inputs;
  inputs.push_back(
      episode("baseline-train-1", EpisodeSplit::Training, baseline, false, 0.45));
  inputs.push_back(
      episode("baseline-train-2", EpisodeSplit::Training, baseline, true, 0.60));
  inputs.push_back(episode("baseline-dev-1", EpisodeSplit::Development,
                           baseline, false, 0.45));
  inputs.push_back(
      episode("candidate-train-1", EpisodeSplit::Training, candidate, true, 0.92));
  inputs.push_back(
      episode("candidate-train-2", EpisodeSplit::Training, candidate, true, 0.95));
  inputs.push_back(
      episode("candidate-dev-1", EpisodeSplit::Development, candidate, true, 0.96));
  inputs.push_back(
      episode("candidate-eval-1", EpisodeSplit::Evaluation, candidate, true, 1.0));

  std::vector<uint32_t> episode_nodes;
  for (LearningEpisodeInput& input : inputs) {
    input.evidence_node_ids = {root, symptom};
    LearningEpisodeResult result;
    require(learner.record_episode(input, &result), "record episode");
    assert(!result.idempotent_replay);
    episode_nodes.push_back(result.node_id);
  }

  LearningEpisodeResult replay;
  require(learner.record_episode(inputs.front(), &replay), "replay episode");
  assert(replay.idempotent_replay);
  assert(replay.node_id == episode_nodes.front());
  LearningEpisodeInput conflicting = inputs.front();
  conflicting.task_success = !conflicting.task_success;
  reject(learner.record_episode(conflicting), "changed episode replay");

  LearningEpisodeInput wrong_dimension =
      episode("wrong-dimension", EpisodeSplit::Training, candidate, true, 0.9);
  wrong_dimension.vector = {1.0f};
  reject(learner.record_episode(wrong_dimension),
         "wrong episode vector dimension");
  LearningEpisodeInput invalid_counts =
      episode("invalid-counts", EpisodeSplit::Training, candidate, true, 0.9);
  invalid_counts.retained_trace_bytes =
      invalid_counts.intermediate_trace_bytes + 1;
  reject(learner.record_episode(invalid_counts),
         "invalid episode evidence counts");
  LearningEpisodeInput invalid_metric =
      episode("invalid-metric", EpisodeSplit::Training, candidate, true, 0.9);
  invalid_metric.causal_f1 = std::numeric_limits<double>::quiet_NaN();
  reject(learner.record_episode(invalid_metric),
         "non-finite episode metric");
  assert(learning_utility(inputs.front()) ==
         learning_utility(inputs.front()));
  assert(learning_utility(inputs.front()) >= -1.0);
  assert(learning_utility(inputs.front()) <= 1.0);

  LearningEpisodeInput unverified =
      episode("unverified-train", EpisodeSplit::Training, candidate, true, 0.99);
  unverified.outcome_verified = false;
  unverified.verifier_id.clear();
  unverified.outcome_evidence_id.clear();
  LearningEpisodeResult unverified_result;
  require(learner.record_episode(unverified, &unverified_result),
          "record unverified episode");
  assert(!unverified_result.training_eligible);
  assert(unverified_result.utility_class == DataUtilityClass::NeverEligible);

  LearningEpisodeInput harmful =
      episode("unsafe-train", EpisodeSplit::Training, unsafe, true, 1.0);
  harmful.false_promotion = true;
  LearningEpisodeResult harmful_result;
  require(learner.record_episode(harmful, &harmful_result),
          "record safety-negative episode");
  assert(harmful_result.safety_negative);
  assert(harmful_result.utility == -1.0);
  assert(!harmful_result.training_eligible);
  assert(harmful_result.utility_class == DataUtilityClass::Harmful);

  LearningEpisodeInput harmful_two =
      episode("unsafe-train-2", EpisodeSplit::Training, unsafe, true, 1.0);
  harmful_two.unauthorized_action = true;
  require(learner.record_episode(harmful_two),
          "record second safety-negative episode");
  LearningEpisodeInput harmful_development =
      episode("unsafe-dev-1", EpisodeSplit::Development, unsafe, true, 1.0);
  harmful_development.harmful_action = true;
  require(learner.record_episode(harmful_development),
          "record development safety-negative episode");

  LearningEpisodeInput baseline_fraud =
      episode("baseline-dev-fraud", EpisodeSplit::Development, baseline,
              true, 0.10);
  baseline_fraud.domain = "fraud";
  require(learner.record_episode(baseline_fraud),
          "record baseline domain episode");
  LearningEpisodeInput regress_train_one =
      episode("regress-train-1", EpisodeSplit::Training, regressing,
              true, 0.90);
  LearningEpisodeInput regress_train_two =
      episode("regress-train-2", EpisodeSplit::Training, regressing,
              true, 0.90);
  LearningEpisodeInput regress_dev_checkout =
      episode("regress-dev-checkout", EpisodeSplit::Development, regressing,
              true, 0.90);
  LearningEpisodeInput regress_dev_fraud =
      episode("regress-dev-fraud", EpisodeSplit::Development, regressing,
              false, 0.90);
  regress_dev_fraud.domain = "fraud";
  require(learner.record_episode(regress_train_one), "record regress train 1");
  require(learner.record_episode(regress_train_two), "record regress train 2");
  require(learner.record_episode(regress_dev_checkout),
          "record regress checkout development");
  require(learner.record_episode(regress_dev_fraud),
          "record regress fraud development");

  LearningEpisodeInput held =
      episode("held-train", EpisodeSplit::Training, candidate, true, 0.99);
  held.legal_hold = true;
  LearningEpisodeResult held_result;
  require(learner.record_episode(held, &held_result),
          "record legal-hold episode");
  assert(!held_result.training_eligible);

  const size_t nodes_before_evaluation = db.node_count();
  PolicyEvaluationReport report =
      learner.evaluate_policies("merchant-risk", baseline);
  assert(!report.durable_writes);
  assert(db.node_count() == nodes_before_evaluation);
  assert(report.has_recommendation);
  assert(report.recommended == candidate);
  assert(report.development_utility_improvement > 0.02);
  assert(report.evaluation_episodes_excluded == 1);
  assert(!contains_id(report.training_episode_node_ids, episode_nodes.back()));
  assert(!contains_id(report.training_episode_node_ids, held_result.node_id));
  assert(!contains_id(report.training_episode_node_ids, harmful_result.node_id));
  assert(report.data_policy.trace_retention_ratio > 0.0);
  assert(report.data_policy.trace_retention_ratio < 1.0);
  const auto unsafe_metrics = std::find_if(
      report.candidates.begin(), report.candidates.end(),
      [&](const PolicyMetrics& value) { return value.policy == unsafe; });
  assert(unsafe_metrics != report.candidates.end());
  assert(unsafe_metrics->safety_violations == 3);
  assert(!unsafe_metrics->eligible);
  const auto regression_metrics = std::find_if(
      report.candidates.begin(), report.candidates.end(),
      [&](const PolicyMetrics& value) { return value.policy == regressing; });
  assert(regression_metrics != report.candidates.end());
  assert(regression_metrics->worst_domain_regression == 1.0);
  assert(!regression_metrics->eligible);

  PolicyDecisionInput unapproved =
      decision("unapproved", PolicyDecisionAction::Promote, baseline);
  unapproved.approved = false;
  reject(learner.record_policy_decision(unapproved),
         "unapproved policy decision");

  PolicyDecisionResult baseline_result;
  require(learner.record_policy_decision(
              decision("promote-baseline", PolicyDecisionAction::Promote,
                       baseline),
              &baseline_result),
          "promote baseline");
  assert(!baseline_result.idempotent_replay);

  PolicyDecisionInput promote_candidate =
      decision("promote-candidate", PolicyDecisionAction::Promote, candidate);
  PolicyDecisionResult candidate_result;
  require(learner.record_policy_decision(promote_candidate, &candidate_result),
          "promote candidate");
  assert(candidate_result.state.previous_policy_version == baseline.version);
  const auto current_candidate = learner.current_policy("merchant-risk");
  assert(current_candidate && current_candidate->policy == candidate);

  PolicyDecisionResult decision_replay;
  require(learner.record_policy_decision(promote_candidate, &decision_replay),
          "replay policy decision");
  assert(decision_replay.idempotent_replay);
  PolicyDecisionInput conflicting_decision = promote_candidate;
  conflicting_decision.reason = "different retry payload";
  reject(learner.record_policy_decision(conflicting_decision),
         "changed policy decision replay");

  require(learner.record_policy_decision(
              decision("rollback-baseline", PolicyDecisionAction::Rollback,
                       baseline)),
          "rollback baseline");
  const auto rolled_back = learner.current_policy("merchant-risk");
  assert(rolled_back && rolled_back->policy == baseline);
  assert(rolled_back->action == PolicyDecisionAction::Rollback);

  reject(learner.quarantine_episode("merchant-risk", "held-train"),
         "quarantine legal-hold episode");
  require(learner.quarantine_episode("merchant-risk", "candidate-train-1"),
          "quarantine episode");
  PolicyEvaluationReport after_quarantine =
      learner.evaluate_policies("merchant-risk", baseline);
  assert(!after_quarantine.has_recommendation);

  require(db.close(), "close");
  require(db.open(directory, options), "reopen");
  OutcomeLearningEngine reopened(db);
  const auto durable_current = reopened.current_policy("merchant-risk");
  assert(durable_current && durable_current->policy == baseline);
  assert(reopened.episodes("merchant-risk").size() == 16);
  PolicyEvaluationReport durable_report =
      reopened.evaluate_policies("merchant-risk", baseline);
  assert(durable_report.evaluation_episodes_excluded == 1);

  require(db.close(), "final close");
  fs::remove_all(directory);
  std::cout << "governed_learning_contract_passed=true\n";
  return 0;
}
