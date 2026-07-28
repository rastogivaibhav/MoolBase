#include "graphene/stability_critic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_map>

namespace graphene {
namespace {

constexpr double kEpsilon = 1e-12;

double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

bool finite01(double value) {
  return std::isfinite(value) && value >= -kEpsilon &&
         value <= 1.0 + kEpsilon;
}

double lower_deficit(double actual, double target) {
  if (!std::isfinite(actual) || !std::isfinite(target) || target <= kEpsilon)
    return 1.0;
  return clamp01((target - actual) / target);
}

double upper_excess(double actual, double maximum) {
  if (!std::isfinite(actual) || !std::isfinite(maximum)) return 1.0;
  if (actual <= maximum) return 0.0;
  return clamp01((actual - maximum) /
                 std::max(kEpsilon, 1.0 - maximum));
}

double entropy(const std::vector<double>& values) {
  double total = 0.0;
  for (double value : values) total += std::max(0.0, value);
  if (total <= 0.0 || values.size() <= 1) return 0.0;
  double result = 0.0;
  for (double value : values) {
    const double probability = std::max(0.0, value) / total;
    if (probability > 0.0) result -= probability * std::log(probability);
  }
  return clamp01(result / std::log(static_cast<double>(values.size())));
}

StabilityWeights adjusted(StabilityWeights weights, QueryMode mode) {
  if (mode == QueryMode::Empirical) {
    weights.provenance += 0.06;
    weights.temporal += 0.04;
    weights.contradiction += 0.06;
    weights.completeness += 0.04;
  } else if (mode == QueryMode::Theoretical) {
    weights.diversity += 0.04;
    weights.relevance += 0.03;
  }
  return weights;
}

LyapunovWeights adjusted(LyapunovWeights weights, QueryMode mode) {
  if (mode == QueryMode::Empirical) {
    weights.provenance += 0.06;
    weights.temporal += 0.04;
    weights.contradiction += 0.06;
    weights.completeness += 0.04;
  } else if (mode == QueryMode::Theoretical) {
    weights.diversity += 0.04;
    weights.relevance += 0.03;
  }
  return weights;
}

LyapunovTargets adjusted(LyapunovTargets targets, QueryMode mode) {
  if (mode == QueryMode::Empirical) {
    targets.temporal_min = std::max(targets.temporal_min, 0.95);
    targets.provenance_min = std::max(targets.provenance_min, 0.85);
    targets.completeness_min = std::max(targets.completeness_min, 0.85);
    targets.relevance_min = std::max(targets.relevance_min, 0.70);
    targets.target_consistency_min =
        std::max(targets.target_consistency_min, 0.70);
    targets.contradiction_max = std::min(targets.contradiction_max, 0.10);
    targets.missing_evidence_max =
        std::min(targets.missing_evidence_max, 0.05);
    targets.retrieval_noise_max =
        std::min(targets.retrieval_noise_max, 0.05);
  } else if (mode == QueryMode::Theoretical) {
    targets.diversity_min = std::max(targets.diversity_min, 0.35);
    targets.degeneracy_min = std::max(targets.degeneracy_min, 0.50);
    targets.provenance_min = std::min(targets.provenance_min, 0.60);
  }
  targets.equilibrium_dwell_steps =
      std::max<size_t>(1, targets.equilibrium_dwell_steps);
  targets.oscillation_window =
      std::max<size_t>(3, targets.oscillation_window);
  return targets;
}

std::vector<double> weight_values(const LyapunovWeights& weights) {
  return {weights.temporal,
          weights.diversity,
          weights.degeneracy,
          weights.provenance,
          weights.contradiction,
          weights.pattern_lock,
          weights.missing_evidence,
          weights.relevance,
          weights.target_consistency,
          weights.completeness,
          weights.retrieval_noise};
}

bool positive_weights(const LyapunovWeights& weights) {
  for (double value : weight_values(weights))
    if (!std::isfinite(value) || value <= 0.0) return false;
  return true;
}

bool bounded(const LyapunovState& state) {
  const std::vector<double> values = {
      state.temporal_deficit,
      state.diversity_deficit,
      state.degeneracy_deficit,
      state.provenance_deficit,
      state.contradiction_excess,
      state.pattern_lock_excess,
      state.missing_evidence_excess,
      state.relevance_deficit,
      state.target_consistency_deficit,
      state.completeness_deficit,
      state.retrieval_noise_excess};
  for (double value : values) if (!finite01(value)) return false;
  return std::isfinite(state.norm_squared) && state.norm_squared >= -kEpsilon &&
         state.norm_squared <= 11.0 + kEpsilon;
}

const FiberPath* representative(const TargetFiber& fiber,
                                const EvidenceCorrelationGroup& group) {
  const auto found = std::find_if(
      fiber.paths.begin(), fiber.paths.end(),
      [&](const FiberPath& path) {
        return path.id == group.representative_path_id;
      });
  return found == fiber.paths.end() ? nullptr : &*found;
}

}  // namespace

StabilityAssessment LyapunovCritic::assess(
    const FiberBundle& bundle,
    QueryMode mode,
    const StabilityWeights& requested_weights,
    const StabilityThresholds& thresholds,
    const LyapunovWeights& requested_lyapunov_weights,
    const LyapunovTargets& requested_targets) const {
  StabilityAssessment output;
  if (bundle.fibers.empty()) {
    output.relevance_score = 0.0;
    output.target_consistency_score = 0.0;
    output.completeness_score = 0.0;
    output.independent_support_score = 0.0;
    output.missing_evidence_penalty = 1.0;
    output.lyapunov_energy = 1.0;
    output.lyapunov_state_norm = 1.0;
    output.requires_escape = true;
    output.requires_abstention = true;
    output.reasons.push_back("no causal fibers were produced");
    return output;
  }

  double temporal = 0.0;
  double provenance = 0.0;
  double relevance = 0.0;
  double target_consistency = 0.0;
  double completeness = 0.0;
  double diversity = 0.0;
  double opposition_fraction = 0.0;
  double material_contradiction = 0.0;
  double best_support = 0.0;
  size_t support_count = 0;
  size_t fibers_with_support = 0;
  size_t total_paths = 0;
  size_t noise_paths = 0;
  size_t missing_lineage = 0;
  std::vector<double> confidences;

  for (const TargetFiber& fiber : bundle.fibers) {
    total_paths += fiber.paths.size();
    noise_paths += fiber.noise_path_count;
    material_contradiction =
        std::max(material_contradiction, fiber.contradiction_mass);
    best_support = std::max(best_support, fiber.independent_support_score);
    size_t supports = 0;
    size_t oppositions = 0;
    for (const EvidenceCorrelationGroup& group : fiber.correlation_groups) {
      const FiberPath* path = representative(fiber, group);
      if (!path) continue;
      if (group.role == FiberPathRole::Support && path->eligible_for_support) {
        ++supports;
        ++support_count;
        temporal += path->temporal_consistency;
        provenance += path->provenance_quality;
        relevance += path->query_relevance;
        target_consistency += path->target_consistency;
        completeness += path->completeness;
        confidences.push_back(path->confidence);
        if (path->evidence_family_lineage.empty()) ++missing_lineage;
      } else if (group.role == FiberPathRole::Opposition &&
                 path->eligible_for_opposition) {
        ++oppositions;
      }
    }
    if (supports > 0) {
      ++fibers_with_support;
      diversity += fiber.relevant_route_diversity;
      opposition_fraction += static_cast<double>(oppositions) /
                             static_cast<double>(supports + oppositions);
    }
  }

  if (support_count == 0) {
    output.relevance_score = 0.0;
    output.target_consistency_score = 0.0;
    output.completeness_score = 0.0;
    output.independent_support_score = 0.0;
    output.retrieval_noise_penalty = total_paths == 0
        ? 0.0
        : static_cast<double>(noise_paths) / static_cast<double>(total_paths);
    output.material_contradiction = material_contradiction;
    output.missing_evidence_penalty = 1.0;
    output.lyapunov_energy = 1.0;
    output.lyapunov_state_norm = 1.0;
    output.requires_escape = true;
    output.requires_abstention = true;
    output.reasons.push_back("no support-eligible evidence groups remain");
    return output;
  }

  const double count = static_cast<double>(support_count);
  output.temporal_consistency = clamp01(temporal / count);
  output.provenance_score = clamp01(provenance / count);
  output.relevance_score = clamp01(relevance / count);
  output.target_consistency_score = clamp01(target_consistency / count);
  output.completeness_score = clamp01(completeness / count);
  output.independent_support_score = clamp01(best_support);
  output.degeneracy_score = output.independent_support_score;
  output.path_diversity = fibers_with_support == 0
      ? 0.0
      : clamp01(diversity / static_cast<double>(fibers_with_support));
  output.contradiction_score = fibers_with_support == 0
      ? 0.0
      : clamp01(opposition_fraction /
                static_cast<double>(fibers_with_support));
  output.material_contradiction = clamp01(material_contradiction);
  output.retrieval_noise_penalty = total_paths == 0
      ? 0.0
      : clamp01(static_cast<double>(noise_paths) /
                static_cast<double>(total_paths));
  output.missing_evidence_penalty = clamp01(
      static_cast<double>(missing_lineage) / count +
      (1.0 - output.completeness_score) +
      (bundle.truncated ? 0.20 : 0.0));

  const double confidence_sum =
      std::accumulate(confidences.begin(), confidences.end(), 0.0);
  const double top_share = confidence_sum <= 0.0
      ? 1.0
      : *std::max_element(confidences.begin(), confidences.end()) /
            confidence_sum;
  const double group_diversity = clamp01(
      output.independent_support_score /
      std::max(0.50, output.independent_support_score + 0.50));
  output.pattern_lock_score = clamp01(
      0.45 * top_share + 0.35 * (1.0 - entropy(confidences)) +
      0.20 * (1.0 - group_diversity));

  const StabilityWeights weights = adjusted(requested_weights, mode);
  const double positive_weight = std::max(
      kEpsilon,
      weights.temporal + weights.diversity + weights.degeneracy +
      weights.provenance + weights.relevance + weights.target_consistency +
      weights.completeness);
  const double negative_weight = std::max(
      kEpsilon,
      weights.contradiction + weights.pattern_lock +
      weights.missing_evidence + weights.retrieval_noise);
  const double positive =
      (weights.temporal * output.temporal_consistency +
       weights.diversity * output.path_diversity +
       weights.degeneracy * output.independent_support_score +
       weights.provenance * output.provenance_score +
       weights.relevance * output.relevance_score +
       weights.target_consistency * output.target_consistency_score +
       weights.completeness * output.completeness_score) /
      positive_weight;
  const double negative =
      (weights.contradiction * output.material_contradiction +
       weights.pattern_lock * output.pattern_lock_score +
       weights.missing_evidence * output.missing_evidence_penalty +
       weights.retrieval_noise * output.retrieval_noise_penalty) /
      negative_weight;
  output.total_score = clamp01(0.72 * positive + 0.28 * (1.0 - negative));

  output.contradiction_blocks_resolution =
      output.material_contradiction >= thresholds.material_contradiction;
  output.evidence_admissible =
      output.relevance_score >= thresholds.minimum_relevance &&
      output.target_consistency_score >= thresholds.minimum_target_consistency &&
      output.completeness_score >= thresholds.minimum_completeness &&
      output.provenance_score >= 0.25 &&
      output.retrieval_noise_penalty <= thresholds.maximum_noise &&
      !output.contradiction_blocks_resolution;
  output.requires_external_verification = true;

  const LyapunovTargets effective_targets = adjusted(requested_targets, mode);
  const LyapunovState error = state(output, mode, requested_targets);
  output.lyapunov_state_norm = std::sqrt(error.norm_squared);
  output.lyapunov_energy = energy(error, mode, requested_lyapunov_weights);
  output.lyapunov_goal_reached =
      output.lyapunov_energy <= effective_targets.equilibrium_energy;
  output.stable = output.evidence_admissible &&
                  output.total_score >= thresholds.stable_score &&
                  output.lyapunov_goal_reached;
  output.requires_escape =
      !output.evidence_admissible ||
      output.total_score < thresholds.escape_score ||
      output.lyapunov_energy > thresholds.lyapunov_escape_energy ||
      output.pattern_lock_score > 0.70 ||
      output.missing_evidence_penalty > 0.25 ||
      output.retrieval_noise_penalty > 0.10;
  output.requires_opposition =
      output.material_contradiction >= thresholds.opposition_score ||
      output.contradiction_score > 0.0 ||
      output.pattern_lock_score > 0.55 || bundle.fibers.size() > 1;
  output.requires_abstention =
      output.total_score < thresholds.abstention_score ||
      output.lyapunov_energy > thresholds.lyapunov_abstention_energy ||
      output.provenance_score < 0.25 || output.temporal_consistency < 0.25 ||
      output.relevance_score < 0.25 || output.target_consistency_score < 0.25;

  if (output.path_diversity < 0.25)
    output.reasons.push_back("relevant independent route diversity is low");
  if (output.independent_support_score < 0.50)
    output.reasons.push_back("independent evidence-family support is weak");
  if (output.material_contradiction > 0.0)
    output.reasons.push_back("material contradictory evidence is present");
  if (output.contradiction_blocks_resolution)
    output.reasons.push_back("unresolved material contradiction blocks resolution");
  if (output.pattern_lock_score > 0.55)
    output.reasons.push_back("the bundle shows premature pattern lock");
  if (output.missing_evidence_penalty > 0.0)
    output.reasons.push_back("the evidence chain is incomplete or under-sourced");
  if (output.retrieval_noise_penalty > 0.0)
    output.reasons.push_back("irrelevant retrieval paths were quarantined");
  if (output.lyapunov_energy > thresholds.lyapunov_stable_energy)
    output.reasons.push_back(
        "Lyapunov energy remains outside the practical stability set");
  return output;
}

LyapunovState LyapunovCritic::state(
    const StabilityAssessment& assessment,
    QueryMode mode,
    const LyapunovTargets& requested_targets) const {
  const LyapunovTargets targets = adjusted(requested_targets, mode);
  LyapunovState output;
  output.temporal_deficit =
      lower_deficit(assessment.temporal_consistency, targets.temporal_min);
  output.diversity_deficit =
      lower_deficit(assessment.path_diversity, targets.diversity_min);
  output.degeneracy_deficit = lower_deficit(
      assessment.independent_support_score, targets.degeneracy_min);
  output.provenance_deficit =
      lower_deficit(assessment.provenance_score, targets.provenance_min);
  const double contradiction = assessment.material_contradiction > 0.0
      ? assessment.material_contradiction
      : assessment.contradiction_score;
  output.contradiction_excess =
      upper_excess(contradiction, targets.contradiction_max);
  output.pattern_lock_excess =
      upper_excess(assessment.pattern_lock_score, targets.pattern_lock_max);
  output.missing_evidence_excess = upper_excess(
      assessment.missing_evidence_penalty, targets.missing_evidence_max);
  output.relevance_deficit =
      lower_deficit(assessment.relevance_score, targets.relevance_min);
  output.target_consistency_deficit = lower_deficit(
      assessment.target_consistency_score, targets.target_consistency_min);
  output.completeness_deficit =
      lower_deficit(assessment.completeness_score, targets.completeness_min);
  output.retrieval_noise_excess = upper_excess(
      assessment.retrieval_noise_penalty, targets.retrieval_noise_max);
  const std::vector<double> coordinates = {
      output.temporal_deficit,
      output.diversity_deficit,
      output.degeneracy_deficit,
      output.provenance_deficit,
      output.contradiction_excess,
      output.pattern_lock_excess,
      output.missing_evidence_excess,
      output.relevance_deficit,
      output.target_consistency_deficit,
      output.completeness_deficit,
      output.retrieval_noise_excess};
  for (double value : coordinates) output.norm_squared += value * value;
  return output;
}

double LyapunovCritic::energy(
    const LyapunovState& state_value,
    QueryMode mode,
    const LyapunovWeights& requested_weights) const {
  const LyapunovWeights weights = adjusted(requested_weights, mode);
  if (!positive_weights(weights) || !bounded(state_value))
    return std::numeric_limits<double>::infinity();
  const std::vector<double> weights_list = weight_values(weights);
  const double total =
      std::accumulate(weights_list.begin(), weights_list.end(), 0.0);
  double result =
      (weights.temporal * state_value.temporal_deficit * state_value.temporal_deficit +
       weights.diversity * state_value.diversity_deficit * state_value.diversity_deficit +
       weights.degeneracy * state_value.degeneracy_deficit * state_value.degeneracy_deficit +
       weights.provenance * state_value.provenance_deficit * state_value.provenance_deficit +
       weights.contradiction * state_value.contradiction_excess * state_value.contradiction_excess +
       weights.pattern_lock * state_value.pattern_lock_excess * state_value.pattern_lock_excess +
       weights.missing_evidence * state_value.missing_evidence_excess * state_value.missing_evidence_excess +
       weights.relevance * state_value.relevance_deficit * state_value.relevance_deficit +
       weights.target_consistency * state_value.target_consistency_deficit * state_value.target_consistency_deficit +
       weights.completeness * state_value.completeness_deficit * state_value.completeness_deficit +
       weights.retrieval_noise * state_value.retrieval_noise_excess * state_value.retrieval_noise_excess) /
      std::max(kEpsilon, total);

  // Inadmissible states have a non-negotiable energy floor. Positive route
  // diversity cannot cancel contradiction, noise, or target irrelevance.
  if (state_value.contradiction_excess > 0.0)
    result = std::max(result,
                      0.35 + 0.60 * state_value.contradiction_excess);
  if (state_value.retrieval_noise_excess > 0.0)
    result = std::max(result,
                      0.15 + 0.35 * state_value.retrieval_noise_excess);
  if (state_value.relevance_deficit > 0.50 ||
      state_value.target_consistency_deficit > 0.50)
    result = std::max(result, 0.45);
  return clamp01(result);
}

LyapunovTrajectory LyapunovCritic::analyse(
    const std::vector<LyapunovSample>& samples,
    QueryMode mode,
    const LyapunovWeights& requested_weights,
    const LyapunovTargets& requested_targets) const {
  LyapunovTrajectory output;
  LyapunovCertificate& certificate = output.certificate;
  const LyapunovWeights weights = adjusted(requested_weights, mode);
  const LyapunovTargets targets = adjusted(requested_targets, mode);
  const std::vector<double> weights_list = weight_values(weights);
  certificate.weights_positive = positive_weights(weights);
  if (certificate.weights_positive) {
    const double sum =
        std::accumulate(weights_list.begin(), weights_list.end(), 0.0);
    const auto bounds =
        std::minmax_element(weights_list.begin(), weights_list.end());
    certificate.lower_quadratic_coefficient = *bounds.first / sum;
    certificate.upper_quadratic_coefficient = *bounds.second / sum;
    certificate.quadratic_bounds_valid = true;
  }
  if (samples.empty()) {
    certificate.violations.push_back("no Lyapunov samples were provided");
    return output;
  }

  certificate.state_bounded = true;
  certificate.energy_nonnegative = certificate.weights_positive;
  certificate.monotonic_nonincreasing = true;
  certificate.strict_or_sufficient_decrease = true;
  std::unordered_map<uint64_t, size_t> first_hash;
  std::vector<double> deltas;
  std::vector<double> ratios;
  size_t trailing_equilibrium = 0;

  for (size_t index = 0; index < samples.size(); ++index) {
    LyapunovObservation observation;
    observation.iteration = index;
    observation.bundle_hash = samples[index].bundle_hash;
    observation.state = state(samples[index].assessment, mode, requested_targets);
    observation.energy = energy(observation.state, mode, requested_weights);
    certificate.state_bounded =
        certificate.state_bounded && bounded(observation.state);
    certificate.energy_nonnegative = certificate.energy_nonnegative &&
        std::isfinite(observation.energy) && observation.energy >= -kEpsilon;

    const auto seen = first_hash.find(observation.bundle_hash);
    if (seen != first_hash.end() && index > seen->second + 1)
      certificate.limit_cycle_detected = true;
    else
      first_hash.emplace(observation.bundle_hash, index);

    if (index == 0) {
      certificate.initial_energy = observation.energy;
      observation.regime = observation.energy <= targets.equilibrium_energy
          ? LyapunovRegime::Equilibrium
          : LyapunovRegime::InsufficientHistory;
    } else {
      const double previous = output.observations.back().energy;
      observation.delta = observation.energy - previous;
      observation.relative_delta =
          observation.delta / std::max(kEpsilon, previous);
      observation.sufficient_decrease = observation.delta <=
          -targets.sufficient_decrease_rate *
              output.observations.back().state.norm_squared;
      deltas.push_back(observation.delta);
      if (previous > kEpsilon && previous > targets.equilibrium_energy)
        ratios.push_back(observation.energy / previous);
      if (observation.delta > targets.increase_epsilon) {
        observation.regime = LyapunovRegime::Diverging;
        certificate.monotonic_nonincreasing = false;
        certificate.strict_or_sufficient_decrease = false;
        ++certificate.diverging_transitions;
        certificate.maximum_energy_increase = std::max(
            certificate.maximum_energy_increase, observation.delta);
      } else if (observation.energy <= targets.equilibrium_energy) {
        observation.regime = LyapunovRegime::Equilibrium;
      } else if (observation.delta < -targets.descent_epsilon) {
        observation.regime = LyapunovRegime::Descending;
        ++certificate.descending_transitions;
      } else {
        observation.regime = LyapunovRegime::Marginal;
        ++certificate.marginal_transitions;
      }
    }
    if (observation.energy <= targets.equilibrium_energy)
      ++trailing_equilibrium;
    else
      trailing_equilibrium = 0;
    output.observations.push_back(observation);
  }

  if (deltas.size() >= 3) {
    size_t sign_changes = 0;
    for (size_t index = 1; index < deltas.size(); ++index) {
      const double left = deltas[index - 1];
      const double right = deltas[index];
      if (std::abs(left) > targets.stagnation_epsilon &&
          std::abs(right) > targets.stagnation_epsilon &&
          ((left < 0.0 && right > 0.0) ||
           (left > 0.0 && right < 0.0)))
        ++sign_changes;
    }
    certificate.oscillation_detected = sign_changes >= 2;
  }

  certificate.final_energy = output.observations.back().energy;
  certificate.goal_reached =
      certificate.final_energy <= targets.equilibrium_energy;
  certificate.equilibrium_dwell_satisfied =
      trailing_equilibrium >= targets.equilibrium_dwell_steps;
  certificate.practical_stability_observed =
      certificate.weights_positive && certificate.state_bounded &&
      certificate.energy_nonnegative && certificate.monotonic_nonincreasing &&
      certificate.goal_reached && certificate.equilibrium_dwell_satisfied &&
      !certificate.oscillation_detected && !certificate.limit_cycle_detected;
  certificate.convergence_observed =
      certificate.practical_stability_observed;
  if (!ratios.empty()) {
    certificate.mean_contraction_ratio =
        std::accumulate(ratios.begin(), ratios.end(), 0.0) /
        static_cast<double>(ratios.size());
    certificate.worst_contraction_ratio =
        *std::max_element(ratios.begin(), ratios.end());
  }

  if (certificate.limit_cycle_detected) {
    output.observations.back().regime = LyapunovRegime::LimitCycle;
    certificate.violations.push_back("bundle hash entered a limit cycle");
  } else if (certificate.oscillation_detected) {
    output.observations.back().regime = LyapunovRegime::Oscillating;
    certificate.violations.push_back("Lyapunov energy oscillated");
  }
  if (!certificate.weights_positive)
    certificate.violations.push_back(
        "Lyapunov weights must be finite and positive");
  if (!certificate.state_bounded)
    certificate.violations.push_back(
        "Lyapunov state escaped the bounded domain");
  if (!certificate.energy_nonnegative)
    certificate.violations.push_back("Lyapunov energy is invalid");
  if (!certificate.monotonic_nonincreasing)
    certificate.violations.push_back("Lyapunov energy increased");
  return output;
}

StabilityAssessment StabilityCriticV0::assess(
    const FiberBundle& bundle,
    QueryMode mode,
    const StabilityWeights& weights,
    const StabilityThresholds& thresholds) const {
  return LyapunovCritic().assess(bundle, mode, weights, thresholds);
}

const char* lyapunov_regime_name(LyapunovRegime regime) {
  switch (regime) {
    case LyapunovRegime::InsufficientHistory: return "insufficient_history";
    case LyapunovRegime::Equilibrium: return "equilibrium";
    case LyapunovRegime::Descending: return "descending";
    case LyapunovRegime::Marginal: return "marginal";
    case LyapunovRegime::Diverging: return "diverging";
    case LyapunovRegime::Oscillating: return "oscillating";
    case LyapunovRegime::LimitCycle: return "limit_cycle";
  }
  return "insufficient_history";
}

}  // namespace graphene
