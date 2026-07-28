#include "graphene/stability_critic.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>

namespace graphene {
namespace {

double clamp01(double value) { return std::clamp(value, 0.0, 1.0); }

void append_reason(std::vector<std::string>* reasons, const std::string& reason) {
  if (std::find(reasons->begin(), reasons->end(), reason) == reasons->end()) {
    reasons->push_back(reason);
  }
}

std::vector<const FiberPath*> all_paths(const FiberBundle& bundle) {
  std::vector<const FiberPath*> paths;
  for (const auto& fiber : bundle.fibers) {
    for (const auto& path : fiber.paths) paths.push_back(&path);
  }
  return paths;
}

// Normalised entropy over non-negative path scores.
double score_entropy(const std::vector<const FiberPath*>& paths) {
  if (paths.size() <= 1) return 0.0;
  double total = 0.0;
  for (const auto* path : paths) total += std::max(0.0, path->score);
  if (total <= 0.0) return 0.0;
  double entropy = 0.0;
  for (const auto* path : paths) {
    const double p = std::max(0.0, path->score) / total;
    if (p > 0.0) entropy -= p * std::log(p);
  }
  return clamp01(entropy / std::log(static_cast<double>(paths.size())));
}

double source_diversity(const std::vector<const FiberPath*>& paths) {
  std::set<std::string> sources;
  size_t source_occurrences = 0;
  for (const auto* path : paths) {
    source_occurrences += path->source_lineage.size();
    sources.insert(path->source_lineage.begin(), path->source_lineage.end());
  }
  if (source_occurrences == 0) return 0.0;
  return clamp01(static_cast<double>(sources.size()) /
                 static_cast<double>(source_occurrences));
}

}  // namespace

StabilityAssessment StabilityCriticV0::assess(
    const FiberBundle& bundle,
    QueryMode mode,
    const StabilityWeights& requested_weights,
    const StabilityThresholds& requested_thresholds) const {
  StabilityWeights weights = requested_weights;
  StabilityThresholds thresholds = requested_thresholds;

  // Negative weights make the diagnostic uninterpretable. Clamp rather than
  // silently allowing a contradiction to improve stability.
  weights.temporal = std::max(0.0, weights.temporal);
  weights.path_diversity = std::max(0.0, weights.path_diversity);
  weights.degeneracy = std::max(0.0, weights.degeneracy);
  weights.provenance = std::max(0.0, weights.provenance);
  weights.contradiction = std::max(0.0, weights.contradiction);
  weights.pattern_lock = std::max(0.0, weights.pattern_lock);
  weights.missing_evidence = std::max(0.0, weights.missing_evidence);

  thresholds.stable_score = clamp01(thresholds.stable_score);
  thresholds.escape_score = clamp01(thresholds.escape_score);
  thresholds.opposition_score = clamp01(thresholds.opposition_score);
  thresholds.abstention_score = clamp01(thresholds.abstention_score);
  thresholds.severe_contradiction = clamp01(thresholds.severe_contradiction);
  thresholds.high_pattern_lock = clamp01(thresholds.high_pattern_lock);

  StabilityAssessment output;
  const auto paths = all_paths(bundle);
  if (paths.empty()) {
    output.missing_evidence_penalty = 1.0;
    output.requires_abstention = true;
    output.requires_escape = !bundle.semantic_candidates.empty();
    append_reason(&output.reasons, "no valid reasoning path exists");
    return output;
  }

  double temporal = 0.0;
  double provenance = 0.0;
  double contradiction = 0.0;
  double top_score = 0.0;
  double score_total = 0.0;
  size_t hypothetical_paths = 0;
  size_t analogical_paths = 0;
  for (const auto* path : paths) {
    temporal += path->temporal_consistency;
    provenance += path->provenance_quality;
    contradiction += path->contains_contradiction ? 1.0 : 0.0;
    hypothetical_paths += path->contains_hypothetical ? 1 : 0;
    analogical_paths += path->contains_analogical ? 1 : 0;
    top_score = std::max(top_score, std::max(0.0, path->score));
    score_total += std::max(0.0, path->score);
  }
  output.temporal_consistency = clamp01(temporal / paths.size());
  output.provenance_score = clamp01(provenance / paths.size());
  output.contradiction_score = clamp01(contradiction / paths.size());

  double diversity = 0.0;
  double degeneracy = 0.0;
  for (const auto& fiber : bundle.fibers) {
    diversity += fiber.path_diversity;
    degeneracy += 1.0 - std::exp(-0.7 *
                                 static_cast<double>(fiber.independent_path_count));
  }
  output.path_diversity = bundle.fibers.empty()
                              ? 0.0
                              : clamp01(diversity / bundle.fibers.size());
  output.degeneracy_score = bundle.fibers.empty()
                                ? 0.0
                                : clamp01(degeneracy / bundle.fibers.size());

  const double top_share = score_total <= 0.0 ? 1.0 : top_score / score_total;
  const double entropy = score_entropy(paths);
  const double sources = source_diversity(paths);
  output.pattern_lock_score = clamp01(
      0.45 * top_share + 0.35 * (1.0 - entropy) + 0.20 * (1.0 - sources));

  output.missing_evidence_penalty = clamp01(
      static_cast<double>(bundle.missing_evidence.size()) /
      std::max<size_t>(1, bundle.fibers.size()));
  if (bundle.truncated) {
    output.missing_evidence_penalty =
        clamp01(output.missing_evidence_penalty + 0.20);
  }

  const double positive_weight = weights.temporal + weights.path_diversity +
                                 weights.degeneracy + weights.provenance;
  const double negative_weight = weights.contradiction + weights.pattern_lock +
                                 weights.missing_evidence;
  const double scale = positive_weight + negative_weight;
  const double raw =
      weights.temporal * output.temporal_consistency +
      weights.path_diversity * output.path_diversity +
      weights.degeneracy * output.degeneracy_score +
      weights.provenance * output.provenance_score -
      weights.contradiction * output.contradiction_score -
      weights.pattern_lock * output.pattern_lock_score -
      weights.missing_evidence * output.missing_evidence_penalty;
  output.total_score = scale <= 0.0 ? 0.0 : clamp01((raw + negative_weight) / scale);

  if (output.temporal_consistency < 1.0) {
    append_reason(&output.reasons, "one or more paths are temporally inconsistent");
  }
  if (output.provenance_score < 0.75) {
    append_reason(&output.reasons, "path provenance is incomplete or unsafe");
  }
  if (output.contradiction_score > 0.0) {
    append_reason(&output.reasons, "contradictory paths remain active");
  }
  if (output.pattern_lock_score >= thresholds.high_pattern_lock) {
    append_reason(&output.reasons, "one path dominates before independent support is established");
  }
  if (output.degeneracy_score < 0.50) {
    append_reason(&output.reasons, "independent path support is weak");
  }
  if (bundle.truncated) {
    append_reason(&output.reasons, "retrieval budget was exhausted");
  }

  // Empirical mode treats speculative dependence as a hard weakness even if
  // the underlying numeric score is otherwise high.
  const double speculative_ratio =
      static_cast<double>(hypothetical_paths + analogical_paths) /
      static_cast<double>(paths.size());
  if (mode == QueryMode::Empirical && speculative_ratio > 0.0) {
    output.total_score = clamp01(output.total_score - 0.25 * speculative_ratio);
    append_reason(&output.reasons,
                  "empirical answer contains hypothetical or analogical evidence");
  }

  output.requires_opposition =
      output.contradiction_score >= thresholds.opposition_score ||
      output.pattern_lock_score >= thresholds.high_pattern_lock ||
      bundle.fibers.size() > 1;
  output.requires_escape =
      output.total_score < thresholds.escape_score ||
      output.pattern_lock_score >= thresholds.high_pattern_lock ||
      output.missing_evidence_penalty > 0.0 || bundle.truncated;
  output.requires_abstention =
      output.total_score < thresholds.abstention_score ||
      output.contradiction_score >= thresholds.severe_contradiction ||
      (mode == QueryMode::Empirical && speculative_ratio >= 0.50);
  output.stable = output.total_score >= thresholds.stable_score &&
                  !output.requires_abstention &&
                  output.contradiction_score < thresholds.severe_contradiction;
  return output;
}

}  // namespace graphene
