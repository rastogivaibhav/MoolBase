#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>

namespace graphene {
namespace {

void add_unique(std::vector<std::string>* values, std::string value) {
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(std::move(value));
  }
}

HypoKoshRuntimeOptions bounded(HypoKoshRuntimeOptions options) {
  options.max_rounds = std::clamp<uint32_t>(options.max_rounds, 1, 3);
  return options;
}

}  // namespace

HypoKoshRuntime::HypoKoshRuntime(const GrapheneDB& db) : db_(db) {}

HypoKoshRuntimeResult HypoKoshRuntime::reason(
    const std::vector<float>& query,
    uint64_t query_signature,
    const HypoKoshRuntimeOptions& requested_options,
    uint64_t snapshot_version) const {
  const HypoKoshRuntimeOptions options = bounded(requested_options);
  HypoKoshRuntimeResult result;
  const uint64_t resolved_snapshot =
      snapshot_version == kInfVersion ? db_.snapshot() : snapshot_version;
  DialecticOptions expansion = options.expansion;
  expansion.max_opposition_rounds = 0;  // New runtime controls reopening.

  DialecticEngine legacy_expander(db_);
  FiberBundleBuilder builder(db_);
  StabilityCriticV0 critic;
  CorrectiveEscape escape;
  ConvergenceEngine convergence;
  OppositionEngine opposition;

  const uint64_t query_id = query_signature ^
                            (resolved_snapshot + 0x9e3779b97f4a7c15ULL);
  BundleSet expanded =
      legacy_expander.expand(query, query_signature, expansion, resolved_snapshot);
  result.initial_bundle = builder.from_bundle_set(expanded, query_id, options.bundle);
  result.initial_stability = critic.assess(
      result.initial_bundle, expansion.mode, options.stability_weights,
      options.stability_thresholds);
  result.initial_convergence = convergence.converge(
      result.initial_bundle, result.initial_stability, expansion.mode,
      options.convergence);
  result.initial_opposition = opposition.oppose(
      result.initial_bundle, result.initial_stability,
      result.initial_convergence, options.opposition);
  result.initial_escape = escape.plan(
      result.initial_bundle, result.initial_stability, expansion.mode,
      options.escape_budget);

  result.final_bundle = result.initial_bundle;
  result.final_stability = result.initial_stability;
  result.final_convergence = result.initial_convergence;
  result.final_opposition = result.initial_opposition;
  result.final_escape = result.initial_escape;

  result.rounds = 1;  // Initial expansion is the first reasoning round.
  uint64_t previous_hash = result.final_bundle.immutable_hash;
  for (uint32_t round = 1; round < options.max_rounds; ++round) {
    const bool should_reopen = result.final_escape.requires_reexpansion ||
                               result.final_opposition.reopen_required;
    if (!should_reopen) break;
    if (result.final_escape.requests_human_evidence) {
      result.stop_reason = RuntimeStopReason::HumanEvidenceRequired;
      break;
    }

    expansion.max_hops = std::min<uint32_t>(16,
        expansion.max_hops + options.escape_budget.max_depth_increment);
    expansion.max_paths = std::min<size_t>(256,
        expansion.max_paths * options.escape_budget.max_path_multiplier);
    expansion.max_paths_per_root = std::min<size_t>(64,
        expansion.max_paths_per_root * options.escape_budget.max_path_multiplier);
    expansion.semantic_candidates = std::min<size_t>(64,
        expansion.semantic_candidates + options.escape_budget.max_candidate_increment);
    expansion.max_visited_states = std::min<size_t>(200000,
        expansion.max_visited_states * options.escape_budget.max_path_multiplier);

    BundleSet reopened =
        legacy_expander.expand(query, query_signature, expansion, resolved_snapshot);
    FiberBundle reopened_bundle =
        builder.from_bundle_set(reopened, query_id, options.bundle);
    ++result.rounds;

    // If wider retrieval returns the exact same path set, continuing would be
    // an opposition loop with no epistemic gain.
    if (reopened_bundle.immutable_hash == previous_hash) {
      result.stop_reason = RuntimeStopReason::NoNewEvidence;
      break;
    }
    previous_hash = reopened_bundle.immutable_hash;
    result.final_bundle = std::move(reopened_bundle);
    result.final_stability = critic.assess(
        result.final_bundle, expansion.mode, options.stability_weights,
        options.stability_thresholds);
    result.final_convergence = convergence.converge(
        result.final_bundle, result.final_stability, expansion.mode,
        options.convergence);
    result.final_opposition = opposition.oppose(
        result.final_bundle, result.final_stability,
        result.final_convergence, options.opposition);
    result.final_escape = escape.plan(
        result.final_bundle, result.final_stability, expansion.mode,
        options.escape_budget);
  }

  if (FiberBundleBuilder::compute_hash(result.final_bundle) !=
      result.final_bundle.immutable_hash) {
    result.stop_reason = RuntimeStopReason::IntegrityFailure;
    result.answer.epistemic_status = "abstain";
    result.answer.residual_uncertainty.push_back(
        "FiberBundle integrity validation failed");
    return result;
  }

  result.answer.has_answer = result.final_convergence.has_answer;
  result.answer.primary_node = result.final_convergence.primary_node;
  result.answer.confidence = result.final_convergence.confidence;
  result.answer.evidence_edges = result.final_convergence.evidence_edges;
  result.answer.evidence = result.final_convergence.evidence;
  result.answer.residual_uncertainty =
      result.final_convergence.residual_uncertainty;

  if (result.final_convergence.false_promotion_risk > 0.0) {
    add_unique(&result.answer.promotion_warnings,
               "selected path contains inferred, hypothetical, analogical, or weakly sourced evidence");
  }
  for (const auto& path_ref : result.final_convergence.selected_paths) {
    for (const auto& fiber : result.final_bundle.fibers) {
      if (fiber.target_node != path_ref.target_node ||
          path_ref.path_index >= fiber.paths.size()) {
        continue;
      }
      const FiberPath& path = fiber.paths[path_ref.path_index];
      if (path.contains_hypothetical) {
        add_unique(&result.answer.promotion_warnings,
                   "hypothetical evidence is not eligible for truth promotion");
      }
      if (path.contains_inferred) {
        add_unique(&result.answer.promotion_warnings,
                   "inferred evidence remains inferred after convergence");
      }
    }
  }

  if (!result.answer.has_answer) {
    result.answer.epistemic_status =
        result.final_stability.requires_abstention ? "abstain" :
                                                     "evidence_required";
    if (result.stop_reason == RuntimeStopReason::Abstained) {
      result.stop_reason = RuntimeStopReason::Abstained;
    }
  } else if (result.final_stability.contradiction_score >= 0.25 ||
             result.final_opposition.opposition_score >= 0.50) {
    result.answer.epistemic_status = "contested";
  } else if (!result.answer.residual_uncertainty.empty() ||
             result.final_opposition.opposition_score > 0.0) {
    result.answer.epistemic_status = "provisionally_resolved";
  } else {
    result.answer.epistemic_status = "resolved";
  }

  if (result.stop_reason == RuntimeStopReason::Abstained) {
    if (result.final_stability.stable &&
        !result.final_opposition.reopen_required) {
      result.stop_reason = RuntimeStopReason::Stable;
    } else if (result.rounds >= options.max_rounds &&
               (result.final_escape.requires_reexpansion ||
                result.final_opposition.reopen_required)) {
      result.stop_reason = RuntimeStopReason::MaxRounds;
    } else if (!result.answer.has_answer) {
      result.stop_reason = RuntimeStopReason::Abstained;
    } else {
      result.stop_reason = RuntimeStopReason::Stable;
    }
  }
  result.durable_writes = false;
  return result;
}

}  // namespace graphene
