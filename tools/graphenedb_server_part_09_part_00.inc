         << (hypotheses.durable_writes ? "true" : "false")
         << ",\"active_policy\":";
  if (active) append_retrieval_policy(output, active->policy);
  else output << "null";
  output << ",\"discriminating_tests\":";
  append_string_array(output, hypotheses.discriminating_tests);
  output << ",\"warnings\":";
  append_string_array(output, hypotheses.warnings);
  output << ",\"proposals\":[";
  for (size_t index = 0; index < hypotheses.proposals.size(); ++index) {
    if (index != 0) output << ',';
    const HypothesisProposal& proposal = hypotheses.proposals[index];
    output << "{\"root_node\":" << proposal.root_node
           << ",\"statement\":\"" << json_escape(proposal.statement)
           << "\",\"origin\":\"hypothetical\",\"plausibility\":"
           << proposal.plausibility << ",\"evidence_edges\":";
    append_u32_array(output, proposal.evidence_edges);
    output << ",\"discriminating_tests\":";
    append_string_array(output, proposal.discriminating_tests);
    output << ",\"eligible_for_truth_promotion\":"
           << (proposal.eligible_for_truth_promotion ? "true" : "false")
           << '}';
  }
  output << "]}";
  return output.str();
}

static std::string hypokosh_runtime_json(const HypoKoshRuntimeResult& result) {
  std::ostringstream output;
  output << "{\"status\":\"" << governed_status_name(result.status)
         << "\",\"primary_node\":" << result.primary_node
         << ",\"confidence\":" << result.confidence
         << ",\"evidence_edges\":";
  append_u32_array(output, result.evidence_edges);
  output << ",\"residual_uncertainty\":";
  append_string_array(output, result.residual_uncertainty);
  output << ",\"stability\":{" 
         << "\"total\":" << result.final_stability.total_score
         << ",\"temporal_consistency\":" << result.final_stability.temporal_consistency
         << ",\"path_diversity\":" << result.final_stability.path_diversity
         << ",\"degeneracy\":" << result.final_stability.degeneracy_score
         << ",\"provenance\":" << result.final_stability.provenance_score
         << ",\"contradiction\":" << result.final_stability.contradiction_score
         << ",\"pattern_lock\":" << result.final_stability.pattern_lock_score
         << ",\"missing_evidence\":" << result.final_stability.missing_evidence_penalty
         << ",\"lyapunov_energy\":" << result.final_stability.lyapunov_energy
         << ",\"lyapunov_state_norm\":" << result.final_stability.lyapunov_state_norm
         << ",\"lyapunov_goal_reached\":"
         << (result.final_stability.lyapunov_goal_reached ? "true" : "false")
         << ",\"stable\":" << (result.final_stability.stable ? "true" : "false")
         << "},\"lyapunov\":{" 
         << "\"initial_energy\":" << result.lyapunov.certificate.initial_energy
         << ",\"final_energy\":" << result.lyapunov.certificate.final_energy
         << ",\"lower_quadratic_coefficient\":"
         << result.lyapunov.certificate.lower_quadratic_coefficient
         << ",\"upper_quadratic_coefficient\":"
         << result.lyapunov.certificate.upper_quadratic_coefficient
         << ",\"mean_contraction_ratio\":"
         << result.lyapunov.certificate.mean_contraction_ratio
         << ",\"worst_contraction_ratio\":"
         << result.lyapunov.certificate.worst_contraction_ratio
         << ",\"maximum_energy_increase\":"
         << result.lyapunov.certificate.maximum_energy_increase
         << ",\"weights_positive\":"
         << (result.lyapunov.certificate.weights_positive ? "true" : "false")
         << ",\"state_bounded\":"
         << (result.lyapunov.certificate.state_bounded ? "true" : "false")
         << ",\"energy_nonnegative\":"
         << (result.lyapunov.certificate.energy_nonnegative ? "true" : "false")
         << ",\"quadratic_bounds_valid\":"
         << (result.lyapunov.certificate.quadratic_bounds_valid ? "true" : "false")
         << ",\"monotonic_nonincreasing\":"
         << (result.lyapunov.certificate.monotonic_nonincreasing ? "true" : "false")
         << ",\"sufficient_decrease\":"
         << (result.lyapunov.certificate.strict_or_sufficient_decrease ? "true" : "false")
         << ",\"goal_reached\":"
         << (result.lyapunov.certificate.goal_reached ? "true" : "false")
         << ",\"equilibrium_dwell_satisfied\":"
         << (result.lyapunov.certificate.equilibrium_dwell_satisfied ? "true" : "false")
         << ",\"practical_stability_observed\":"
         << (result.lyapunov.certificate.practical_stability_observed ? "true" : "false")
         << ",\"convergence_observed\":"
         << (result.lyapunov.certificate.convergence_observed ? "true" : "false")
         << ",\"oscillation_detected\":"
         << (result.lyapunov.certificate.oscillation_detected ? "true" : "false")
         << ",\"limit_cycle_detected\":"
         << (result.lyapunov.certificate.limit_cycle_detected ? "true" : "false")
         << ",\"violations\":";
  append_string_array(output, result.lyapunov.certificate.violations);
  output << ",\"observations\":[";
  for (size_t index = 0; index < result.lyapunov.observations.size(); ++index) {
    if (index != 0) output << ',';
    const LyapunovObservation& observation = result.lyapunov.observations[index];
    output << "{\"iteration\":" << observation.iteration
           << ",\"bundle_hash\":" << observation.bundle_hash
           << ",\"energy\":" << observation.energy
           << ",\"delta\":" << observation.delta
           << ",\"relative_delta\":" << observation.relative_delta
           << ",\"sufficient_decrease\":"
           << (observation.sufficient_decrease ? "true" : "false")
           << ",\"regime\":\"" << lyapunov_regime_name(observation.regime)
           << "\"}";
  }
  output << "]},\"opposition\":{" 
         << "\"score\":" << result.final_opposition.opposition_score
         << ",\"reexpansion_requested\":"
         << (result.final_opposition.requests_reexpansion ? "true" : "false")
         << ",\"challenged_claims\":";
  append_string_array(output, result.final_opposition.challenged_claims);
  output << "},\"self_healing\":{" 
