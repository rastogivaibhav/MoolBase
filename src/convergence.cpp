#include "graphene/convergence.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace graphene {
namespace {

double path_rank(const FiberPath& path) {
  return 0.55 * path.score + 0.20 * path.provenance_quality +
         0.15 * path.temporal_consistency + 0.10 * path.independence_score -
         (path.contains_contradiction ? 0.20 : 0.0) -
         (path.contains_hypothetical ? 0.15 : 0.0);
}

}  // namespace

RuntimeConvergedAnswer ConvergenceEngine::converge(
    const FiberBundle& bundle,
    const StabilityAssessment& stability,
    QueryMode mode,
    const RuntimeConvergenceOptions& requested_options) const {
  RuntimeConvergenceOptions options = requested_options;
  options.max_selected_paths =
      std::clamp<size_t>(options.max_selected_paths, 1, 32);
  options.minimum_confidence =
      std::clamp(options.minimum_confidence, 0.0, 1.0);

  RuntimeConvergedAnswer output;
  output.source_bundle_hash = bundle.immutable_hash;
  if (bundle.fibers.empty()) {
    output.residual_uncertainty.push_back("no target fiber exists");
    return output;
  }

  struct Candidate {
    const TargetFiber* fiber;
    size_t path_index;
    double rank;
  };
  std::vector<Candidate> candidates;
  for (const auto& fiber : bundle.fibers) {
    for (size_t index = 0; index < fiber.paths.size(); ++index) {
      candidates.push_back({&fiber, index, path_rank(fiber.paths[index])});
    }
  }
  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              if (std::abs(left.rank - right.rank) > 1e-12) {
                return left.rank > right.rank;
              }
              if (left.fiber->target_node != right.fiber->target_node) {
                return left.fiber->target_node < right.fiber->target_node;
              }
              return left.fiber->paths[left.path_index].id <
                     right.fiber->paths[right.path_index].id;
            });

  if (candidates.empty()) {
    output.residual_uncertainty.push_back("target fibers contain no paths");
    return output;
  }

  output.primary_node = candidates.front().fiber->target_node;
  output.primary_path_id =
      candidates.front().fiber->paths[candidates.front().path_index].id;

  std::set<uint32_t> evidence_edges;
  std::set<std::string> evidence_keys;
  size_t selected = 0;
  bool empirical_supported = false;
  for (const Candidate& candidate : candidates) {
    const FiberPath& path = candidate.fiber->paths[candidate.path_index];
    if (candidate.fiber->target_node == output.primary_node &&
        selected < options.max_selected_paths) {
      output.selected_paths.push_back(
          {candidate.fiber->target_node, path.id, candidate.path_index,
           "selected by bounded provenance-aware convergence"});
      ++selected;
      evidence_edges.insert(path.edges.begin(), path.edges.end());
      for (const auto& evidence : path.evidence) {
        const std::string key = evidence.source_id + "\x1f" + evidence.span +
                                "\x1f" + evidence.observed_at;
        if (evidence_keys.insert(key).second) output.evidence.push_back(evidence);
      }
      empirical_supported = empirical_supported ||
                            (!path.contains_hypothetical &&
                             !path.contains_analogical &&
                             !path.contains_inferred &&
                             path.provenance_quality > 0.0);
    } else {
      output.discarded_paths.push_back(
          {candidate.fiber->target_node, path.id, candidate.path_index,
           candidate.fiber->target_node == output.primary_node
               ? "lower-ranked path retained in immutable FiberBundle"
               : "competing target retained for opposition"});
    }
  }
  output.evidence_edges.assign(evidence_edges.begin(), evidence_edges.end());

  const FiberPath& primary_path =
      candidates.front().fiber->paths[candidates.front().path_index];
  output.false_promotion_risk = std::clamp(
      (primary_path.contains_inferred ? 0.35 : 0.0) +
          (primary_path.contains_hypothetical ? 0.45 : 0.0) +
          (primary_path.contains_analogical ? 0.35 : 0.0) +
          (1.0 - primary_path.provenance_quality) * 0.30,
      0.0, 1.0);
  output.confidence = std::clamp(
      0.65 * primary_path.score + 0.20 * stability.total_score +
          0.15 * primary_path.provenance_quality -
          0.20 * output.false_promotion_risk,
      0.0, 1.0);

  if (stability.contradiction_score > 0.0) {
    output.residual_uncertainty.push_back("contradictory paths remain");
  }
  if (stability.pattern_lock_score >= 0.70) {
    output.residual_uncertainty.push_back("selected path may reflect pattern lock");
  }
  if (stability.missing_evidence_penalty > 0.0) {
    output.residual_uncertainty.push_back("critical provenance is incomplete");
  }
  if (bundle.truncated) {
    output.residual_uncertainty.push_back("retrieval stopped at a configured budget");
  }
  if (bundle.fibers.size() > 1) {
    output.residual_uncertainty.push_back("competing targets remain");
  }
  if (mode == QueryMode::Empirical &&
      options.require_observed_or_discovered_for_empirical &&
      !empirical_supported) {
    output.residual_uncertainty.push_back(
        "empirical mode lacks an observed or discovered source-grounded path");
  }

  output.has_answer = !stability.requires_abstention &&
                      output.confidence >= options.minimum_confidence &&
                      !(mode == QueryMode::Empirical &&
                        options.require_observed_or_discovered_for_empirical &&
                        !empirical_supported);
  if (!output.has_answer) {
    output.residual_uncertainty.push_back(
        "governed convergence threshold was not satisfied");
  }

  // Strong immutability assertion in production code: convergence is only a
  // view over the bundle. The caller can verify this hash after return.
  if (FiberBundleBuilder::compute_hash(bundle) != bundle.immutable_hash) {
    output.has_answer = false;
    output.residual_uncertainty.push_back(
        "FiberBundle integrity check failed during convergence");
  }
  return output;
}

}  // namespace graphene
