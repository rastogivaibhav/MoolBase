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
  LyapunovCritic critic;
  CorrectiveEscape escape;
  RecursiveSelfHealingController self_healing;

  BundleSet initial_expansion =
      dialectic.expand(query, query_signature, options.dialectic, resolved_snapshot);
  result.initial_bundle = bundle_builder.build(initial_expansion);
  result.initial_stability = critic.assess(result.initial_bundle, options.dialectic.mode,
                                           options.stability_weights,
                                           options.stability_thresholds,
                                           options.lyapunov_weights,
                                           options.lyapunov_targets);
  result.initial_escape = escape.plan(result.initial_bundle, result.initial_stability,
                                      options.dialectic.mode);
  result.initial_convergence = dialectic.converge(initial_expansion, options.dialectic);
  result.initial_opposition =
      dialectic.oppose(initial_expansion, result.initial_convergence, options.dialectic);
  result.initial_self_healing = self_healing.plan(
      result.initial_stability, result.initial_opposition, result.initial_escape);

  std::vector<LyapunovSample> lyapunov_samples;
  lyapunov_samples.push_back(
      {result.initial_bundle.immutable_hash, result.initial_stability});

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
                      options.stability_weights, options.stability_thresholds,
                      options.lyapunov_weights, options.lyapunov_targets);
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
    lyapunov_samples.push_back(
        {result.final_bundle.immutable_hash, result.final_stability});
    const LyapunovTrajectory partial_trajectory = critic.analyse(
        lyapunov_samples, current_options.mode, options.lyapunov_weights,
