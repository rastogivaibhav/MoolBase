#include "graphene/stability_critic.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace graphene;

namespace {

StabilityAssessment assessment(double temporal,
                               double diversity,
                               double degeneracy,
                               double provenance,
                               double contradiction,
                               double pattern_lock,
                               double missing_evidence) {
  StabilityAssessment value;
  value.temporal_consistency = temporal;
  value.path_diversity = diversity;
  value.degeneracy_score = degeneracy;
  value.provenance_score = provenance;
  value.contradiction_score = contradiction;
  value.pattern_lock_score = pattern_lock;
  value.missing_evidence_penalty = missing_evidence;
  return value;
}

}  // namespace

int main() {
  LyapunovCritic critic;

  const StabilityAssessment equilibrium =
      assessment(1.0, 0.80, 0.90, 1.0, 0.0, 0.20, 0.0);
  const LyapunovState equilibrium_state = critic.state(equilibrium);
  assert(std::abs(equilibrium_state.norm_squared) < 1e-12);
  assert(std::abs(critic.energy(equilibrium_state)) < 1e-12);

  const StabilityAssessment poor =
      assessment(0.30, 0.0, 0.0, 0.20, 0.80, 0.95, 0.90);
  const LyapunovState poor_state = critic.state(poor);
  assert(poor_state.norm_squared > 1.0);
  assert(critic.energy(poor_state) > 0.50);

  const StabilityAssessment middle =
      assessment(0.65, 0.10, 0.30, 0.50, 0.40, 0.70, 0.40);
  const StabilityAssessment good =
      assessment(0.98, 0.55, 0.75, 0.95, 0.05, 0.35, 0.02);

  const LyapunovTrajectory descending = critic.analyse({
      {101, poor}, {102, middle}, {103, good}, {104, good}});
  assert(descending.observations.size() == 4);
  assert(descending.certificate.weights_positive);
  assert(descending.certificate.state_bounded);
  assert(descending.certificate.energy_nonnegative);
  assert(descending.certificate.quadratic_bounds_valid);
  assert(descending.certificate.lower_quadratic_coefficient > 0.0);
  assert(descending.certificate.upper_quadratic_coefficient >=
         descending.certificate.lower_quadratic_coefficient);
  assert(descending.certificate.mean_contraction_ratio < 1.0);
  assert(descending.certificate.monotonic_nonincreasing);
  assert(descending.certificate.goal_reached);
  assert(descending.certificate.equilibrium_dwell_satisfied);
  assert(descending.certificate.practical_stability_observed);
  assert(descending.certificate.convergence_observed);
  assert(descending.certificate.descending_transitions >= 1);
  assert(descending.observations.back().regime == LyapunovRegime::Equilibrium);

  const LyapunovTrajectory diverging = critic.analyse({
      {201, middle}, {202, poor}});
  assert(!diverging.certificate.monotonic_nonincreasing);
  assert(diverging.certificate.diverging_transitions == 1);
  assert(diverging.certificate.maximum_energy_increase > 0.0);
  assert(diverging.observations.back().regime == LyapunovRegime::Diverging);

  const LyapunovTrajectory oscillating = critic.analyse({
      {301, poor}, {302, middle}, {303, poor}, {304, middle}, {305, poor}});
  assert(oscillating.certificate.oscillation_detected);
  assert(oscillating.observations.back().regime == LyapunovRegime::Oscillating);

  const LyapunovTrajectory cycle = critic.analyse({
      {401, poor}, {402, middle}, {401, poor}});
  assert(cycle.certificate.limit_cycle_detected);
  assert(cycle.observations.back().regime == LyapunovRegime::LimitCycle);

  LyapunovWeights invalid_weights;
  invalid_weights.provenance = -1.0;
  const LyapunovTrajectory invalid = critic.analyse({{501, poor}},
                                                     QueryMode::Balanced,
                                                     invalid_weights);
  assert(!invalid.certificate.weights_positive);
  assert(!invalid.certificate.energy_nonnegative);
  assert(!invalid.certificate.violations.empty());

  const StabilityAssessment moderate_provenance =
      assessment(0.93, 0.45, 0.65, 0.70, 0.10, 0.40, 0.05);
  const double balanced_energy = critic.energy(
      critic.state(moderate_provenance, QueryMode::Balanced),
      QueryMode::Balanced);
  const double empirical_energy = critic.energy(
      critic.state(moderate_provenance, QueryMode::Empirical),
      QueryMode::Empirical);
  assert(empirical_energy > balanced_energy);

  const auto repeated = critic.analyse({
      {101, poor}, {102, middle}, {103, good}, {104, good}});
  assert(repeated.certificate.final_energy ==
         descending.certificate.final_energy);
  assert(repeated.certificate.violations ==
         descending.certificate.violations);

  std::cout << "complete_lyapunov_critic_contract_passed=true\n";
  return 0;
}
