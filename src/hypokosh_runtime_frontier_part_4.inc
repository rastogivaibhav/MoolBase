  // Finalise any post-projection safety downgrade before emitting the durable
  // Terminal event so receipt state/cause cannot diverge from the returned
  // governed status.
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
      result.status = GovernedEpistemicStatus::ProvisionallyResolved;
      result.receipt.terminal_cause =
          "non_observed_evidence_prevented_silent_resolution";
      result.residual_uncertainty.push_back(
          "selected evidence contains a non-observed edge and cannot be silently resolved");
      break;
    }
  }

  const PriorEpistemicState& prior =
      options.prior_epistemic_state;
  const uint32_t current_operative =
      result.final_convergence.has_answer ? result.primary_node : 0;
  const uint32_t current_committed =
      result.final_convergence.has_answer &&
              trace_status_commits(result.status)
          ? result.primary_node
          : 0;

  auto already_has_transition =
      [&](EpistemicEventType type,
          uint32_t previous_node,
          uint32_t current_node) {
        for (const NativeEpistemicEvent& event :
             result.receipt.epistemic_events) {
          if (event.type == type &&
              event.previous_hypothesis_node == previous_node &&
              event.hypothesis_node == current_node) {
            return true;
          }
        }
        return false;
      };

  if (prior.available && prior.has_answer &&
      prior.operative_node != 0 &&
      prior.operative_node != current_operative &&
      !already_has_transition(
          EpistemicEventType::Revision,
          prior.operative_node,
          current_operative)) {
    NativeEpistemicEvent revision;
    revision.round_index = result.receipt.expansion_rounds;
    revision.source = EpistemicEventSource::HypoKosh;
    revision.type = EpistemicEventType::Revision;
    revision.previous_hypothesis_node = prior.operative_node;
    revision.hypothesis_node = current_operative;
    revision.competing_hypotheses =
        hypothesis_nodes(result.final_bundle);
    revision.evidence_edges = result.evidence_edges;
    revision.evidence_family_ids = selected_evidence_families(
        result.final_bundle, result.final_convergence);
    revision.epistemic_state =
        current_operative == 0 ? "revised_to_no_operative"
                               : "revised_across_evidence_update";
    revision.reason =
        "new persistent evidence changed the previously governed operative hypothesis";
    append_epistemic_event(&result.receipt, std::move(revision));
  }

  if (prior.available && prior.committed_node != 0 &&
      prior.committed_node != current_committed &&
      !already_has_transition(
          EpistemicEventType::Decommitment,
          prior.committed_node,
          0)) {
    NativeEpistemicEvent decommitment;
    decommitment.round_index = result.receipt.expansion_rounds;
    decommitment.source = EpistemicEventSource::HypoKosh;
    decommitment.type = EpistemicEventType::Decommitment;
    decommitment.previous_hypothesis_node = prior.committed_node;
    decommitment.hypothesis_node = 0;
    decommitment.competing_hypotheses =
        hypothesis_nodes(result.final_bundle);
    decommitment.evidence_edges = result.evidence_edges;
    decommitment.evidence_family_ids = selected_evidence_families(
        result.final_bundle, result.final_convergence);
    decommitment.epistemic_state = "decommitted";
    decommitment.reason =
        "new persistent evidence withdrew the previous governed commitment";
    append_epistemic_event(&result.receipt, std::move(decommitment));
  }

  const bool direct_commitment_change =
      prior.available && prior.committed_node != 0 &&
      current_committed != 0 &&
      current_committed != prior.committed_node;
  const bool recommit_after_gap =
      prior.available && prior.committed_node == 0 &&
      prior.last_committed_node != 0 &&
      current_committed != 0;
  const uint32_t recommit_from =
      direct_commitment_change
          ? prior.committed_node
          : prior.last_committed_node;
  if ((direct_commitment_change || recommit_after_gap) &&
      !already_has_transition(
          EpistemicEventType::Recommitment,
          recommit_from,
          current_committed)) {
    NativeEpistemicEvent recommitment;
    recommitment.round_index = result.receipt.expansion_rounds;
    recommitment.source = EpistemicEventSource::HypoKosh;
    recommitment.type = EpistemicEventType::Recommitment;
    recommitment.previous_hypothesis_node = recommit_from;
    recommitment.hypothesis_node = current_committed;
    recommitment.competing_hypotheses =
        hypothesis_nodes(result.final_bundle);
    recommitment.evidence_edges = result.evidence_edges;
    recommitment.evidence_family_ids = selected_evidence_families(
        result.final_bundle, result.final_convergence);
    recommitment.epistemic_state = "recommitted";
    recommitment.reason =
        "new persistent evidence earned a governed commitment after a previous commitment";
    append_epistemic_event(&result.receipt, std::move(recommitment));
  }

  if (prior.available &&
      prior.status != GovernedEpistemicStatus::Resolved &&
      result.status == GovernedEpistemicStatus::Resolved &&
      current_committed != 0 &&
      !already_has_transition(
          EpistemicEventType::Resolution, 0, current_committed)) {
    NativeEpistemicEvent resolution;
    resolution.round_index = result.receipt.expansion_rounds;
    resolution.source = EpistemicEventSource::HypoKosh;
    resolution.type = EpistemicEventType::Resolution;
    resolution.hypothesis_node = current_committed;
    resolution.competing_hypotheses =
        hypothesis_nodes(result.final_bundle);
    resolution.evidence_edges = result.evidence_edges;
    resolution.evidence_family_ids = selected_evidence_families(
        result.final_bundle, result.final_convergence);
    resolution.epistemic_state = "resolved";
    resolution.reason =
        "persistent evidence updates satisfied governed resolution and stability requirements";
    append_epistemic_event(&result.receipt, std::move(resolution));
  }

  std::sort(result.residual_uncertainty.begin(),
            result.residual_uncertainty.end());
  result.residual_uncertainty.erase(
      std::unique(result.residual_uncertainty.begin(),
                  result.residual_uncertainty.end()),
      result.residual_uncertainty.end());

  NativeEpistemicEvent terminal;
  terminal.source = EpistemicEventSource::GrapheneCore;
  terminal.type = EpistemicEventType::Terminal;
  terminal.hypothesis_node =
      result.final_convergence.has_answer ? result.primary_node : 0;
  terminal.competing_hypotheses =
      hypothesis_nodes(result.final_bundle);
  terminal.evidence_edges = result.evidence_edges;
  terminal.evidence_family_ids = selected_evidence_families(
      result.final_bundle, result.final_convergence);
  terminal.epistemic_state = governed_status_name(result.status);
  terminal.reason = result.receipt.terminal_cause.empty()
      ? "governed projection emitted the terminal epistemic state"
      : result.receipt.terminal_cause;
  append_epistemic_event(&result.receipt, std::move(terminal));

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
  result.receipt.opposition_executed = options.enable_dwm;
  result.receipt.bounded_recovery_executed =
      result.receipt.expansion_rounds > 0;
  result.receipt.governed_projection_executed = true;

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
    hypothesis.metadata["frontier_progress_rounds"] =
        std::to_string(result.receipt.frontier_progress_rounds);
    model_world_->add(std::move(hypothesis),
                      "record governed reasoning result");

    if (options.enable_dwm &&
        !result.final_opposition.challenged_claims.empty()) {
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
    result.receipt.model_world_updated = true;
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
    case GovernedEpistemicStatus::Open: return "open";
    case GovernedEpistemicStatus::Abstain: return "abstain";
    case GovernedEpistemicStatus::Speculative: return "speculative";
  }
  return "abstain";
}

const char* epistemic_event_source_name(EpistemicEventSource source) {
  switch (source) {
    case EpistemicEventSource::GrapheneCore: return "graphene_core";
    case EpistemicEventSource::HypoKosh: return "hypokosh";
    case EpistemicEventSource::DialecticalModelWorlds: return "dwm";
  }
  return "graphene_core";
}

const char* epistemic_event_type_name(EpistemicEventType type) {
  switch (type) {
    case EpistemicEventType::HypothesisSet: return "hypothesis_set";
    case EpistemicEventType::Decision: return "decision";
    case EpistemicEventType::Challenge: return "challenge";
    case EpistemicEventType::CorroborationSearch:
      return "corroboration_search";
    case EpistemicEventType::Reopen: return "reopen";
    case EpistemicEventType::Revision: return "revision";
    case EpistemicEventType::Decommitment: return "decommitment";
    case EpistemicEventType::Recommitment: return "recommitment";
    case EpistemicEventType::Resolution: return "resolution";
    case EpistemicEventType::Terminal: return "terminal";
  }
  return "decision";
}

}  // namespace graphene
