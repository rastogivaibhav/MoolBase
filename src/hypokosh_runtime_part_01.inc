        options.lyapunov_targets);
    if (!new_information || partial_trajectory.certificate.limit_cycle_detected ||
        partial_trajectory.certificate.oscillation_detected) {
      break;
    }
  }

  result.lyapunov = critic.analyse(
      lyapunov_samples, options.dialectic.mode, options.lyapunov_weights,
      options.lyapunov_targets);

  result.status = project_status(result.final_convergence, result.final_opposition,
                                 result.final_stability, options.dialectic.mode);
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
  result.evidence_edges = all_evidence_edges(result.final_convergence);
  result.residual_uncertainty = result.final_convergence.residual_uncertainty;
  result.residual_uncertainty.insert(result.residual_uncertainty.end(),
                                     result.final_stability.reasons.begin(),
                                     result.final_stability.reasons.end());
  result.residual_uncertainty.insert(
      result.residual_uncertainty.end(),
      result.lyapunov.certificate.violations.begin(),
      result.lyapunov.certificate.violations.end());
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
  result.receipt.lyapunov_trajectory_executed = true;
  result.receipt.lyapunov_certificate_valid =
      result.lyapunov.certificate.weights_positive &&
      result.lyapunov.certificate.state_bounded &&
      result.lyapunov.certificate.energy_nonnegative;
  result.receipt.lyapunov_goal_reached =
      result.lyapunov.certificate.goal_reached;
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
    hypothesis.metadata["lyapunov_energy"] =
        std::to_string(result.lyapunov.certificate.final_energy);
    hypothesis.metadata["lyapunov_regime"] =
        result.lyapunov.observations.empty()
            ? "insufficient_history"
            : lyapunov_regime_name(result.lyapunov.observations.back().regime);
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
