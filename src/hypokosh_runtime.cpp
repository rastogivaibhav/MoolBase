#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>
#include <set>

namespace graphene {
namespace {

bool has_action(const EscapePlan& plan, EscapeAction action) {
  return std::any_of(plan.tasks.begin(), plan.tasks.end(),
                     [&](const EscapeTask& task) {
    return task.action == action;
  });
}

DialecticOptions targeted_options(DialecticOptions options,
                                  const EscapePlan& plan) {
  if (has_action(plan, EscapeAction::GenerateMissingEvidenceQuery) ||
      has_action(plan, EscapeAction::SearchTemporalNeighbour)) {
    options.max_hops = std::min<uint32_t>(16, options.max_hops + 1);
  }
  if (has_action(plan, EscapeAction::SeekIndependentEvidence)) {
    options.semantic_candidates =
        std::min<size_t>(64, options.semantic_candidates + 4);
    options.max_paths =
        std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
  }
  if (has_action(plan, EscapeAction::SearchContradiction) ||
      has_action(plan, EscapeAction::ExpandMinorityPath)) {
    options.max_paths_per_root =
        std::min<size_t>(64, options.max_paths_per_root + 4);
    options.max_paths =
        std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
  }
  if (has_action(plan, EscapeAction::PruneRetrievalNoise)) {
    options.minimum_confidence =
        std::min(0.95, options.minimum_confidence + 0.05);
  }
  options.max_visited_states =
      std::min<size_t>(200000,
                       std::max<size_t>(2, options.max_visited_states * 2));
  return options;
}

bool options_changed(const DialecticOptions& left,
                     const DialecticOptions& right) {
  return left.semantic_candidates != right.semantic_candidates ||
         left.max_hops != right.max_hops ||
         left.max_paths != right.max_paths ||
         left.max_paths_per_root != right.max_paths_per_root ||
         left.max_visited_states != right.max_visited_states ||
         left.minimum_confidence != right.minimum_confidence;
}

std::vector<uint32_t> all_evidence_edges(
    const ConvergedAnswer& convergence) {
  std::vector<uint32_t> values = convergence.evidence_edges;
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

GovernedEpistemicStatus project_status(
    const ConvergedAnswer& convergence,
    const OppositionReport& opposition,
    const StabilityAssessment& stability,
    const EpistemicAdmissibility& admissibility,
    QueryMode mode) {
  if (!convergence.has_answer) {
    return stability.missing_evidence_penalty > 0.0
               ? GovernedEpistemicStatus::EvidenceRequired
               : GovernedEpistemicStatus::Abstain;
  }
  if (admissibility.semantic_verification ==
      SemanticVerificationStatus::Contradicted) {
    return GovernedEpistemicStatus::Contested;
  }
  if (admissibility.contradiction_blocks_resolution ||
      opposition.opposition_score >= 0.50) {
    return GovernedEpistemicStatus::Contested;
  }
  if (!admissibility.evidence_admissible) {
    return stability.completeness_score < 0.50 ||
                   stability.provenance_score < 0.25
               ? GovernedEpistemicStatus::EvidenceRequired
               : GovernedEpistemicStatus::Abstain;
  }
  if (mode == QueryMode::Theoretical &&
      admissibility.semantic_verification ==
          SemanticVerificationStatus::Unverified) {
    return GovernedEpistemicStatus::Speculative;
  }
  if (stability.stable &&
      admissibility.sufficient_independent_support &&
      admissibility.semantic_verification ==
          SemanticVerificationStatus::Verified &&
      opposition.opposition_score == 0.0) {
    return GovernedEpistemicStatus::Resolved;
  }
  return GovernedEpistemicStatus::ProvisionallyResolved;
}

}  // namespace

CompleteHypoKoshRuntime::CompleteHypoKoshRuntime(
    const GrapheneDB& db,
    ModelWorld* model_world)
    : db_(db), model_world_(model_world) {}

HypoKoshRuntimeResult CompleteHypoKoshRuntime::reason(
    const std::vector<float>& query,
    uint64_t query_signature,
    const RuntimeOptions& requested_options,
    uint64_t snapshot_version) const {
  RuntimeOptions options = requested_options;
  options.max_recursive_cycles =
      std::min<uint32_t>(options.max_recursive_cycles, 3);
  const uint64_t resolved_snapshot =
      snapshot_version == kInfVersion ? db_.snapshot() : snapshot_version;

  HypoKoshRuntimeResult result;
  DialecticEngine dialectic(db_);
  FiberBundleBuilder bundle_builder;
  LyapunovCritic critic;
  EpistemicController controller;
  CorrectiveEscape escape;
  RecursiveSelfHealingController self_healing;

  BundleSet expansion = dialectic.expand(
      query, query_signature, options.dialectic, resolved_snapshot);
  if (options.path_verifier) {
    apply_path_verifier(&expansion, *options.path_verifier, query,
                        query_signature, options.dialectic.mode);
  }
  result.initial_bundle = bundle_builder.build(expansion);
  result.initial_stability = critic.assess(
      result.initial_bundle, options.dialectic.mode,
      options.stability_weights, options.stability_thresholds,
      options.lyapunov_weights, options.lyapunov_targets);
  result.initial_admissibility = controller.assess(
      result.initial_bundle, result.initial_stability,
      options.dialectic.mode);
  result.initial_escape = escape.plan(
      result.initial_bundle, result.initial_stability,
      options.dialectic.mode);
  result.initial_convergence = controller.converge(
      result.initial_bundle, result.initial_admissibility,
      result.initial_stability, options.dialectic);
  result.initial_opposition = controller.oppose(
      result.initial_bundle, result.initial_convergence,
      result.initial_admissibility, result.initial_stability,
      options.dialectic);
  result.initial_self_healing = self_healing.plan(
      result.initial_stability, result.initial_opposition,
      result.initial_escape);

  result.final_bundle = result.initial_bundle;
  result.final_stability = result.initial_stability;
  result.final_admissibility = result.initial_admissibility;
  result.final_convergence = result.initial_convergence;
  result.final_opposition = result.initial_opposition;
  result.final_self_healing = result.initial_self_healing;

  std::vector<LyapunovSample> samples;
  samples.push_back(
      {result.initial_bundle.immutable_hash,
       result.initial_stability});

  DialecticOptions current_options = options.dialectic;
  EscapePlan current_escape = result.initial_escape;
  for (uint32_t round = 0; round < options.max_recursive_cycles; ++round) {
    if (!result.final_stability.requires_escape &&
        !result.final_opposition.requests_reexpansion) {
      break;
    }
    const DialecticOptions next_options =
        targeted_options(current_options, current_escape);
    if (!current_escape.generic_expansion_allowed &&
        !options_changed(current_options, next_options)) {
      break;
    }
    current_options = next_options;
    BundleSet reopened = dialectic.expand(
        query, query_signature, current_options, resolved_snapshot);
    if (options.path_verifier) {
      apply_path_verifier(&reopened, *options.path_verifier, query,
                          query_signature, current_options.mode);
    }
    FiberBundle reopened_bundle = bundle_builder.build(reopened);
    StabilityAssessment reopened_stability = critic.assess(
        reopened_bundle, current_options.mode,
        options.stability_weights, options.stability_thresholds,
        options.lyapunov_weights, options.lyapunov_targets);
    EpistemicAdmissibility reopened_admissibility = controller.assess(
        reopened_bundle, reopened_stability, current_options.mode);
    ConvergedAnswer reopened_convergence = controller.converge(
        reopened_bundle, reopened_admissibility,
        reopened_stability, current_options);
    OppositionReport reopened_opposition = controller.oppose(
        reopened_bundle, reopened_convergence,
        reopened_admissibility, reopened_stability,
        current_options);
    EscapePlan reopened_escape = escape.plan(
        reopened_bundle, reopened_stability, current_options.mode);

    const bool new_information =
        reopened_bundle.immutable_hash !=
        result.final_bundle.immutable_hash;
    result.final_bundle = std::move(reopened_bundle);
    result.final_stability = std::move(reopened_stability);
    result.final_admissibility = std::move(reopened_admissibility);
    result.final_convergence = std::move(reopened_convergence);
    result.final_opposition = std::move(reopened_opposition);
    result.final_self_healing = self_healing.plan(
        result.final_stability, result.final_opposition,
        reopened_escape);
    current_escape = std::move(reopened_escape);
    ++result.receipt.expansion_rounds;
    samples.push_back(
        {result.final_bundle.immutable_hash,
         result.final_stability});

    const LyapunovTrajectory partial = critic.analyse(
        samples, current_options.mode,
        options.lyapunov_weights, options.lyapunov_targets);
    if (!new_information || partial.certificate.limit_cycle_detected ||
        partial.certificate.oscillation_detected) {
      break;
    }
  }

  result.lyapunov = critic.analyse(
      samples, options.dialectic.mode,
      options.lyapunov_weights, options.lyapunov_targets);
  result.status = project_status(
      result.final_convergence, result.final_opposition,
      result.final_stability, result.final_admissibility,
      options.dialectic.mode);
  if (result.lyapunov.certificate.limit_cycle_detected ||
      result.lyapunov.certificate.oscillation_detected) {
    result.status = result.final_convergence.has_answer
                        ? GovernedEpistemicStatus::Contested
                        : GovernedEpistemicStatus::Abstain;
  } else if (result.status == GovernedEpistemicStatus::Resolved &&
             !result.lyapunov.certificate.practical_stability_observed) {
    result.status = GovernedEpistemicStatus::ProvisionallyResolved;
  }

  result.primary_node = result.final_convergence.primary_node;
  result.confidence = result.final_convergence.confidence;
  result.evidence_edges =
      all_evidence_edges(result.final_convergence);
  result.residual_uncertainty =
      result.final_convergence.residual_uncertainty;
  result.residual_uncertainty.insert(
      result.residual_uncertainty.end(),
      result.final_stability.reasons.begin(),
      result.final_stability.reasons.end());
  result.residual_uncertainty.insert(
      result.residual_uncertainty.end(),
      result.final_admissibility.reasons.begin(),
      result.final_admissibility.reasons.end());
  result.residual_uncertainty.insert(
      result.residual_uncertainty.end(),
      result.lyapunov.certificate.violations.begin(),
      result.lyapunov.certificate.violations.end());
  std::sort(result.residual_uncertainty.begin(),
            result.residual_uncertainty.end());
  result.residual_uncertainty.erase(
      std::unique(result.residual_uncertainty.begin(),
                  result.residual_uncertainty.end()),
      result.residual_uncertainty.end());

  result.receipt.snapshot_version = resolved_snapshot;
  result.receipt.initial_bundle_hash =
      result.initial_bundle.immutable_hash;
  result.receipt.final_bundle_hash =
      result.final_bundle.immutable_hash;
  result.receipt.graphene_executed = true;
  result.receipt.path_verifier_executed = options.path_verifier != nullptr;
  result.receipt.fiber_bundle_built = true;
  result.receipt.fiber_bundle_authoritative = true;
  result.receipt.stability_critic_executed = true;
  result.receipt.epistemic_admissibility_executed = true;
  result.receipt.lyapunov_trajectory_executed = true;
  result.receipt.lyapunov_certificate_valid =
      result.lyapunov.certificate.weights_positive &&
      result.lyapunov.certificate.state_bounded &&
      result.lyapunov.certificate.energy_nonnegative;
  result.receipt.lyapunov_goal_reached =
      result.lyapunov.certificate.goal_reached;
  result.receipt.semantic_verification_required =
      result.final_admissibility.requires_external_verification;
  result.receipt.escape_considered = true;
  result.receipt.convergence_executed = true;
  result.receipt.opposition_executed = true;
  result.receipt.governed_projection_executed = true;
  result.receipt.no_silent_promotion = true;

  for (uint32_t edge_id : result.evidence_edges) {
    const auto edge = db_.get_edge(edge_id, resolved_snapshot);
    if (!edge) continue;
    const bool speculative_origin =
        edge->origin == EdgeOrigin::Hypothetical ||
        edge->origin == EdgeOrigin::Inferred ||
        edge->origin == EdgeOrigin::Reinforced;
    if (speculative_origin &&
        result.status == GovernedEpistemicStatus::Resolved) {
      result.receipt.no_silent_promotion = false;
      result.status =
          GovernedEpistemicStatus::ProvisionallyResolved;
      result.residual_uncertainty.push_back(
          "selected evidence contains a non-observed edge and cannot be silently resolved");
      break;
    }
  }

  if (model_world_ && options.update_model_world &&
      result.final_convergence.has_answer) {
    ModelWorldNode hypothesis;
    hypothesis.type = ModelWorldNodeType::Hypothesis;
    hypothesis.status =
        result.status == GovernedEpistemicStatus::Contested
            ? ModelWorldStatus::Contested
            : ModelWorldStatus::Active;
    hypothesis.statement =
        "Reasoning result for Graphene node " +
        std::to_string(result.primary_node);
    hypothesis.origin = EdgeOrigin::Hypothetical;
    hypothesis.evidence_edges = result.evidence_edges;
    hypothesis.metadata["epistemic_status"] =
        governed_status_name(result.status);
    hypothesis.metadata["bundle_hash"] =
        std::to_string(result.final_bundle.immutable_hash);
    hypothesis.metadata["lyapunov_energy"] =
        std::to_string(result.lyapunov.certificate.final_energy);
    hypothesis.metadata["evidence_admissible"] =
        result.final_admissibility.evidence_admissible
            ? "true" : "false";
    hypothesis.metadata["semantic_verification"] =
        std::to_string(static_cast<int>(
            result.final_admissibility.semantic_verification));
    hypothesis.metadata["path_verifier_executed"] =
        result.receipt.path_verifier_executed ? "true" : "false";
    model_world_->add(std::move(hypothesis),
                      "record governed reasoning result");

    if (!result.final_opposition.challenged_claims.empty()) {
      ModelWorldNode opposition_node;
      opposition_node.type = ModelWorldNodeType::Opposition;
      opposition_node.status = ModelWorldStatus::Active;
      opposition_node.statement =
          result.final_opposition.challenged_claims.front();
      opposition_node.origin = EdgeOrigin::Hypothetical;
      opposition_node.evidence_edges = result.evidence_edges;
      model_world_->add(std::move(opposition_node),
                        "record dialectic opposition");
    }
    for (const auto& repair : result.final_self_healing.repairs) {
      ModelWorldNode repair_node;
      repair_node.type = ModelWorldNodeType::Experiment;
      repair_node.status = ModelWorldStatus::Proposed;
      repair_node.statement = repair.reason;
      repair_node.origin = EdgeOrigin::Hypothetical;
      repair_node.evidence_edges = result.evidence_edges;
      model_world_->add(std::move(repair_node),
                        "record bounded self-healing proposal");
    }
    result.receipt.model_world_event_hash =
        model_world_->event_log_hash();
  }
  return result;
}

const char* governed_status_name(GovernedEpistemicStatus status) {
  switch (status) {
    case GovernedEpistemicStatus::Resolved: return "resolved";
    case GovernedEpistemicStatus::ProvisionallyResolved:
      return "provisionally_resolved";
    case GovernedEpistemicStatus::Contested: return "contested";
    case GovernedEpistemicStatus::EvidenceRequired:
      return "evidence_required";
    case GovernedEpistemicStatus::Abstain: return "abstain";
    case GovernedEpistemicStatus::Speculative: return "speculative";
  }
  return "abstain";
}

}  // namespace graphene
