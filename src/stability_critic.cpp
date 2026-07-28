#include "graphene/stability_critic.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <numeric>

namespace graphene {
namespace {

double clamp01(double value) { return std::clamp(value, 0.0, 1.0); }

double jaccard_distance(const std::vector<uint32_t>& left,
                        const std::vector<uint32_t>& right) {
  std::set<uint32_t> a(left.begin(), left.end());
  std::set<uint32_t> b(right.begin(), right.end());
  size_t intersection = 0;
  for (uint32_t value : a) if (b.count(value)) ++intersection;
  const size_t union_size = a.size() + b.size() - intersection;
  return union_size == 0 ? 0.0 : 1.0 - static_cast<double>(intersection) /
                                      static_cast<double>(union_size);
}

double entropy(const std::vector<double>& values) {
  double total = 0.0;
  for (double value : values) total += std::max(0.0, value);
  if (total <= 0.0 || values.size() <= 1) return 0.0;
  double h = 0.0;
  for (double value : values) {
    const double p = std::max(0.0, value) / total;
    if (p > 0.0) h -= p * std::log(p);
  }
  return clamp01(h / std::log(static_cast<double>(values.size())));
}

}  // namespace

StabilityAssessment StabilityCriticV0::assess(
    const FiberBundle& bundle,
    QueryMode mode,
    const StabilityWeights& requested_weights,
    const StabilityThresholds& thresholds) const {
  StabilityAssessment output;
  if (bundle.fibers.empty()) {
    output.missing_evidence_penalty = 1.0;
    output.requires_escape = true;
    output.requires_abstention = true;
    output.reasons.push_back("no causal fibers were produced");
    return output;
  }

  const TargetFiber& primary = bundle.fibers.front();
  if (primary.paths.empty()) {
    output.missing_evidence_penalty = 1.0;
    output.requires_escape = true;
    output.requires_abstention = true;
    output.reasons.push_back("the primary fiber has no reasoning paths");
    return output;
  }

  double temporal = 0.0;
  double provenance = 0.0;
  double contradictory = 0.0;
  size_t missing_source_paths = 0;
  std::vector<double> confidences;
  for (const auto& path : primary.paths) {
    temporal += path.temporal_consistency;
    provenance += path.provenance_quality;
    contradictory += path.contains_contradiction ? 1.0 : 0.0;
    if (path.source_lineage.empty() && !path.edges.empty()) ++missing_source_paths;
    confidences.push_back(path.confidence);
  }
  const double count = static_cast<double>(primary.paths.size());
  output.temporal_consistency = clamp01(temporal / count);
  output.provenance_score = clamp01(provenance / count);
  output.contradiction_score = clamp01(contradictory / count);
  output.degeneracy_score = clamp01(primary.degeneracy);
  output.missing_evidence_penalty = clamp01(
      static_cast<double>(missing_source_paths) / count + (bundle.truncated ? 0.20 : 0.0));

  if (primary.paths.size() == 1) {
    output.path_diversity = 0.0;
  } else {
    double diversity_sum = 0.0;
    size_t pairs = 0;
    for (size_t i = 0; i < primary.paths.size(); ++i) {
      for (size_t j = i + 1; j < primary.paths.size(); ++j) {
        diversity_sum += jaccard_distance(primary.paths[i].edges, primary.paths[j].edges);
        ++pairs;
      }
    }
    output.path_diversity = pairs == 0 ? 0.0 : clamp01(diversity_sum / static_cast<double>(pairs));
  }

  const double sum = std::accumulate(confidences.begin(), confidences.end(), 0.0);
  const double top_share = sum <= 0.0 ? 1.0 : *std::max_element(confidences.begin(), confidences.end()) / sum;
  const double normalised_entropy = entropy(confidences);
  const double source_diversity = clamp01(static_cast<double>(primary.independent_path_count) /
                                           static_cast<double>(std::max<size_t>(1, primary.paths.size())));
  output.pattern_lock_score = clamp01(
      0.45 * top_share + 0.35 * (1.0 - normalised_entropy) + 0.20 * (1.0 - source_diversity));

  StabilityWeights weights = requested_weights;
  if (mode == QueryMode::Empirical) {
    weights.provenance += 0.08;
    weights.temporal += 0.05;
    weights.contradiction += 0.05;
  } else if (mode == QueryMode::Theoretical) {
    weights.diversity += 0.08;
    weights.pattern_lock += 0.05;
    weights.missing_evidence -= 0.02;
  }
  const double positive_weight = std::max(1e-9, weights.temporal + weights.diversity +
                                                   weights.degeneracy + weights.provenance);
  const double negative_weight = std::max(1e-9, weights.contradiction + weights.pattern_lock +
                                                   weights.missing_evidence);
  const double positive = (weights.temporal * output.temporal_consistency +
                           weights.diversity * output.path_diversity +
                           weights.degeneracy * output.degeneracy_score +
                           weights.provenance * output.provenance_score) / positive_weight;
  const double negative = (weights.contradiction * output.contradiction_score +
                           weights.pattern_lock * output.pattern_lock_score +
                           weights.missing_evidence * output.missing_evidence_penalty) / negative_weight;
  output.total_score = clamp01(0.75 * positive + 0.25 * (1.0 - negative));

  output.stable = output.total_score >= thresholds.stable_score &&
                  output.contradiction_score < 0.25 &&
                  output.missing_evidence_penalty < 0.50;
  output.requires_escape = output.total_score < thresholds.escape_score ||
                           output.pattern_lock_score > 0.70 ||
                           output.missing_evidence_penalty > 0.25;
  output.requires_opposition = output.contradiction_score >= thresholds.opposition_score ||
                               output.pattern_lock_score > 0.55 ||
                               bundle.fibers.size() > 1;
  output.requires_abstention = output.total_score < thresholds.abstention_score ||
                               output.provenance_score < 0.25 ||
                               output.temporal_consistency < 0.25;

  if (output.path_diversity < 0.25) output.reasons.push_back("path diversity is low");
  if (output.degeneracy_score < 0.50) output.reasons.push_back("independent support is weak");
  if (output.contradiction_score > 0.0) output.reasons.push_back("contradictory paths are present");
  if (output.pattern_lock_score > 0.55) output.reasons.push_back("the bundle shows premature pattern lock");
  if (output.missing_evidence_penalty > 0.0) output.reasons.push_back("some paths lack source evidence or expansion was truncated");
  if (!output.stable && output.reasons.empty()) output.reasons.push_back("stability score is below threshold");
  return output;
}

}  // namespace graphene
