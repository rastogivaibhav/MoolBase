#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>
#include <set>

namespace graphene {
namespace {

DialecticOptions expanded_options(DialecticOptions options) {
  options.max_hops = std::min<uint32_t>(16, options.max_hops + 1);
  options.max_paths = std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
  options.max_paths_per_root =
      std::min<size_t>(64, std::max<size_t>(2, options.max_paths_per_root * 2));
  options.max_visited_states =
      std::min<size_t>(200000, std::max<size_t>(2, options.max_visited_states * 2));
  options.semantic_candidates =
      std::min<size_t>(64, std::max<size_t>(2, options.semantic_candidates + 4));
  return options;
}

std::vector<uint32_t> all_evidence_edges(const ConvergedAnswer& convergence) {
  std::vector<uint32_t> values = convergence.evidence_edges;
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

GovernedEpistemicStatus project_status(const ConvergedAnswer& convergence,
                                       const OppositionReport& opposition,
                                       const StabilityAssessment& stability,
                                       QueryMode mode) {
  if (!convergence.has_answer || stability.requires_abstention) {
    return stability.missing_evidence_penalty > 0.0
               ? GovernedEpistemicStatus::EvidenceRequired
               : GovernedEpistemicStatus::Abstain;
  }
  if (mode == QueryMode::Theoretical &&
      (stability.provenance_score < 0.75 || convergence.false_promotion_risk > 0.0)) {
    return GovernedEpistemicStatus::Speculative;
  }
  if (opposition.opposition_score >= 0.50 || stability.contradiction_score >= 0.25) {
    return GovernedEpistemicStatus::Contested;
  }
  if (!stability.stable || !convergence.residual_uncertainty.empty() ||
      opposition.opposition_score > 0.0) {
    return GovernedEpistemicStatus::ProvisionallyResolved;
  }
  return GovernedEpistemicStatus::Resolved;
}

}  // namespace

CompleteHypoKoshRuntime::CompleteHypoKoshRuntime(const GrapheneDB& db,
                                                 ModelWorld* model_world)
    : db_(db), model_world_(model_world) {}

HypoKoshRuntimeResult CompleteHypoKoshRuntime::reason(
    const std::vector<float>& query,
    uint64_t query_signature,
    const RuntimeOptions& requested_options,
    uint64_t snapshot_version) const {
  RuntimeOptions options = requested_options;
  options.max_recursive_cycles = std::min<uint32_t>(options.max_recursive_cycles, 3);
  const uint64_t resolved_snapshot = snapshot_version == kInfVersion ? db_.snapshot() : snapshot_version;

  HypoKoshRuntimeResult result;
  DialecticEngine dialectic(db_);
  FiberBundleBuilder bundle_builder;
  StabilityCriticV0 critic;
  CorrectiveEscape escape;
  RecursiveSelfHealingController self_healing;

  BundleSet initial_expansion =
      dialectic.expand(query, query_signature, options.dialectic, resolved_snapshot);
  result.initial_bundle = bundle_builder.build(initial_expansion);
  result.initial_stability = critic.assess(result.initial_bundle, options.dialectic.mode,
                                           options.stability_weights,
                                           options.stability_thresholds);
  result.initial_escape = escape.plan(result.initial_bundle, result.initial_stability,
                                      options.dialectic.mode);
  result.initial_convergence = dialectic.converge(initial_expansion, options.dialectic);
  result.initial_opposition =
      dialectic.oppose(initial_expansion, result.initial_convergence, options.dialectic);
  result.initial_self_healing = self_healing.plan(
      result.initial_stability, result.initial_opposition, result.initial_escape);

  result.final_bundle = result.initial_bundle;
  result.final_stability = result.initial_stability;
  result.final_convergence = result.initial_convergence;
  result.final_opposition = result.initial_opposition;
  result.final_self_healing = result.initial_self_healing;

  DialecticOptions current_options = options.dialectic;
  for (uint32_t round = 0; round < options.max_recursive_cycles; ++round) {
    if (!result.final_stability.requires_escape &&
        !result.final_opposition.requests_reexpansion) {
      break;
    }
    current_options = expanded_options(current_options);
    BundleSet reopened =
        dialectic.expand(query, query_signature, current_options, resolved_snapshot);
    FiberBundle reopened_bundle = bundle_builder.build(reopened);
    StabilityAssessment reopened_stability =
        critic.assess(reopened_bundle, current_options.mode,
                      options.stability_weights, options.stability_thresholds);
    ConvergedAnswer reopened_convergence = dialectic.converge(reopened, current_options);
    OppositionReport reopened_opposition =
        dialectic.oppose(reopened, reopened_convergence, current_options);

    const bool new_information = reopened_bundle.immutable_hash != result.final_bundle.immutable_hash;
    result.final_bundle = std::move(reopened_bundle);
    result.final_stability = std::move(reopened_stability);
    result.final_convergence = std::move(reopened_convergence);
    result.final_opposition = std::move(reopened_opposition);
    const EscapePlan reopened_escape = escape.plan(
        result.final_bundle, result.final_stability, current_options.mode);
    result.final_self_healing = self_healing.plan(
        result.final_stability, result.final_opposition, reopened_escape);
    ++result.receipt.expansion_rounds;
    if (!new_information) break;
  }

  result.status = project_status(result.final_convergence, result.final_opposition,
                                 result.final_stability, options.dialectic.mode);
  result.primary_node = result.final_convergence.primary_node;
  result.confidence = result.final_convergence.confidence;
  result.evidence_edges = all_evidence_edges(result.final_convergence);
  result.residual_uncertainty = result.final_convergence.residual_uncertainty;
  result.residual_uncertainty.insert(result.residual_uncertainty.end(),
                                     result.final_stability.reasons.begin(),
                                     result.final_stability.reasons.end());
  std::sort(result.residual_uncertainty.begin(), result.residual_uncertainty.end());
  result.residual_uncertainty.erase(
      std::unique(result.residual_uncertainty.begin(), result.residual_uncertainty.end()),
      result.residual_uncertainty.end());

  result.receipt.snapshot_version = resolved_snapshot;
  result.receipt.initial_bundle_hash = result.initial_bundle.immutable_hash;
  result.receipt.final_bundle_hash = result.final_bundle.immutable_hash;
  result.receipt.graphene_executed = true;
  result.receipt.fiber_bundle_built = true;
  result.receipt.stability_critic_executed = true;
  result.receipt.escape_considered = true;
  result.receipt.convergence_executed = true;
  result.receipt.opposition_executed = true;
  result.receipt.governed_projection_executed = true;
  result.receipt.no_silent_promotion = true;
  for (uint32_t edge_id : result.evidence_edges) {
    const auto edge = db_.get_edge(edge_id, resolved_snapshot);
    if (!edge) continue;
    const bool speculative_origin = edge->origin == EdgeOrigin::Hypothetical ||
                                    edge->origin == EdgeOrigin::Inferred ||
                                    edge->origin == EdgeOrigin::Reinforced;
    if (speculative_origin && result.status == GovernedEpistemicStatus::Resolved) {
      result.receipt.no_silent_promotion = false;
      result.status = GovernedEpistemicStatus::ProvisionallyResolved;
      result.residual_uncertainty.push_back(
          "selected evidence contains a non-observed edge and cannot be silently resolved");
      break;
    }
  }

  if (model_world_ && options.update_model_world && result.final_convergence.has_answer) {
    ModelWorldNode hypothesis;
    hypothesis.type = ModelWorldNodeType::Hypothesis;
    hypothesis.status = result.status == GovernedEpistemicStatus::Contested
                            ? ModelWorldStatus::Contested
                            : ModelWorldStatus::Active;
    hypothesis.statement = "Reasoning result for Graphene node " + std::to_string(result.primary_node);
    hypothesis.origin = EdgeOrigin::Hypothetical;
    hypothesis.evidence_edges = result.evidence_edges;
    hypothesis.metadata["epistemic_status"] = governed_status_name(result.status);
    hypothesis.metadata["bundle_hash"] = std::to_string(result.final_bundle.immutable_hash);
    model_world_->add(std::move(hypothesis), "record governed reasoning result");

    if (!result.final_opposition.challenged_claims.empty()) {
      ModelWorldNode opposition_node;
      opposition_node.type = ModelWorldNodeType::Opposition;
      opposition_node.status = ModelWorldStatus::Active;
      opposition_node.statement = result.final_opposition.challenged_claims.front();
      opposition_node.origin = EdgeOrigin::Hypothetical;
      opposition_node.evidence_edges = result.evidence_edges;
      model_world_->add(std::move(opposition_node), "record dialectic opposition");
    }
    for (const auto& repair : result.final_self_healing.repairs) {
      ModelWorldNode repair_node;
      repair_node.type = ModelWorldNodeType::Experiment;
      repair_node.status = ModelWorldStatus::Proposed;
      repair_node.statement = repair.reason;
      repair_node.origin = EdgeOrigin::Hypothetical;
      repair_node.evidence_edges = result.evidence_edges;
      model_world_->add(std::move(repair_node), "record bounded self-healing proposal");
    }
    result.receipt.model_world_event_hash = model_world_->event_log_hash();
  }
  return result;
}

const char* governed_status_name(GovernedEpistemicStatus status) {
  switch (status) {
    case GovernedEpistemicStatus::Resolved: return "resolved";
    case GovernedEpistemicStatus::ProvisionallyResolved: return "provisionally_resolved";
    case GovernedEpistemicStatus::Contested: return "contested";
    case GovernedEpistemicStatus::EvidenceRequired: return "evidence_required";
    case GovernedEpistemicStatus::Abstain: return "abstain";
    case GovernedEpistemicStatus::Speculative: return "speculative";
  }
  return "abstain";
}

}  // namespace graphene
