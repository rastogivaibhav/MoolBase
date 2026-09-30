
struct GovernedProjection {
  GovernedEpistemicStatus status{GovernedEpistemicStatus::Abstain};
  std::string cause;
};

GovernedProjection project_status(
    const ConvergedAnswer& convergence,
    const OppositionReport& opposition,
    const StabilityAssessment& stability,
    const EpistemicAdmissibility& admissibility,
    QueryMode mode,
    const StabilityThresholds& thresholds) {
  if (!convergence.has_answer) {
    if (admissibility.contradiction_blocks_resolution) {
      return {GovernedEpistemicStatus::Contested,
              "material_contradiction_without_earned_replacement"};
    }
    return {
        stability.missing_evidence_penalty > 0.0
            ? GovernedEpistemicStatus::EvidenceRequired
            : GovernedEpistemicStatus::Abstain,
        stability.missing_evidence_penalty > 0.0
            ? "no_answer_missing_evidence"
            : "no_answer_abstain"};
  }
  if (admissibility.semantic_verification ==
      SemanticVerificationStatus::Contradicted) {
    return {GovernedEpistemicStatus::Contested,
            "semantic_verification_contradicted"};
  }
  // The admissibility object is already scoped to the selected target and
  // computed with the configured contradiction threshold. Do not reintroduce
  // bundle-global contradiction here.
  (void)thresholds;
  if (admissibility.contradiction_blocks_resolution) {
    return {GovernedEpistemicStatus::Contested,
            "material_contradiction_blocks_resolution"};
  }
  if (opposition.opposition_score >= 0.50) {
    return {GovernedEpistemicStatus::Contested,
            "dialectic_opposition_blocks_resolution"};
  }
  if (!admissibility.evidence_admissible) {
    return {
        stability.completeness_score < 0.50 ||
                stability.provenance_score < 0.25
            ? GovernedEpistemicStatus::EvidenceRequired
            : GovernedEpistemicStatus::Abstain,
        stability.completeness_score < 0.50 ||
                stability.provenance_score < 0.25
            ? "evidence_incomplete_or_low_provenance"
            : "evidence_not_admissible"};
  }
  if (mode == QueryMode::Theoretical &&
      admissibility.semantic_verification ==
          SemanticVerificationStatus::Unverified) {
    return {GovernedEpistemicStatus::Speculative,
            "theoretical_claim_unverified"};
  }
  if (stability.stable &&
      admissibility.sufficient_independent_support &&
      admissibility.semantic_verification ==
          SemanticVerificationStatus::Verified &&
      opposition.opposition_score == 0.0) {
    return {GovernedEpistemicStatus::Resolved, "earned_resolution"};
  }
  return {GovernedEpistemicStatus::ProvisionallyResolved,
          "resolution_conditions_not_fully_earned"};
}

bool trace_status_commits(GovernedEpistemicStatus status) {
  return status == GovernedEpistemicStatus::Resolved ||
         status == GovernedEpistemicStatus::ProvisionallyResolved;
}

}  // namespace

CompleteHypoKoshRuntime::CompleteHypoKoshRuntime(
    const GrapheneDB& db,
    ModelWorld* model_world)
    : db_(db), model_world_(model_world) {}

HypoKoshRuntimeResult CompleteHypoKoshRuntime::reason(
    const std::vector<float>& query,
    uint64_t query_signature,
    const RuntimeOptions& requested_options,
    uint64_t snapshot_version) const {
  RuntimeOptions options = requested_options;
  if (!options.enable_hypokosh && options.enable_dwm) {
    throw std::invalid_argument(
        "DWM requires HypoKosh; disable enable_dwm before requesting G0");
  }
  options.max_recursive_cycles =
      std::min<uint32_t>(options.max_recursive_cycles, 3);
  options.unchanged_recovery_patience =
      std::min<uint32_t>(options.unchanged_recovery_patience, 3);
  const uint64_t resolved_snapshot =
      snapshot_version == kInfVersion ? db_.snapshot() : snapshot_version;

  HypoKoshRuntimeResult result;
  result.receipt.hypokosh_capability_enabled = options.enable_hypokosh;
  result.receipt.dwm_capability_enabled = options.enable_dwm;
  result.receipt.opposition_research_enabled =
      options.enable_dwm && options.enable_opposition_research;

  // Genuine G0: execute only the Graphene-owned evidence/provenance path.
  // HypoKosh convergence, stability/admissibility control, DWM opposition,
  // recovery and model-world promotion are intentionally not constructed.
  if (!options.enable_hypokosh) {
    GrapheneEvidenceExpander evidence_expander(db_);
    FiberBundleBuilder bundle_builder;
    BundleSet expansion = evidence_expander.expand(
        query, query_signature, options.dialectic, resolved_snapshot);
    if (options.path_verifier) {
      apply_path_verifier(&expansion, *options.path_verifier, query,
                          query_signature, options.dialectic.mode);
    }

    result.initial_bundle = bundle_builder.build(expansion);
    result.final_bundle = result.initial_bundle;
    result.status = GovernedEpistemicStatus::Abstain;
    result.primary_node = 0;
    result.confidence = 0.0;

    const std::vector<uint32_t> evidence_targets =
        hypothesis_nodes(result.final_bundle);
    result.evidence_edges = evidence_edges_for_nodes(
        result.final_bundle, evidence_targets);
    result.residual_uncertainty.push_back(
        "G0 exposes persistent evidence/provenance state without hypothesis selection");

    NativeEpistemicEvent terminal;
    terminal.source = EpistemicEventSource::GrapheneCore;
    terminal.type = EpistemicEventType::Terminal;
    terminal.hypothesis_node = 0;
    terminal.evidence_edges = result.evidence_edges;
    terminal.evidence_family_ids = evidence_families_for_nodes(
        result.final_bundle, evidence_targets);
    terminal.epistemic_state = "abstain";
    terminal.reason =
        "G0 evidence-state projection completed without HypoKosh or DWM execution";
    append_epistemic_event(&result.receipt, std::move(terminal));

    result.receipt.snapshot_version = resolved_snapshot;
    result.receipt.initial_bundle_hash = result.initial_bundle.immutable_hash;
    result.receipt.final_bundle_hash = result.final_bundle.immutable_hash;
    result.receipt.graphene_executed = true;
    result.receipt.path_verifier_executed = options.path_verifier != nullptr;
    result.receipt.fiber_bundle_built = true;
    result.receipt.fiber_bundle_authoritative = true;
    result.receipt.stability_critic_executed = false;
    result.receipt.epistemic_admissibility_executed = false;
    result.receipt.lyapunov_trajectory_executed = false;
    result.receipt.lyapunov_certificate_valid = false;
    result.receipt.lyapunov_goal_reached = false;
    result.receipt.escape_considered = false;
    result.receipt.convergence_executed = false;
    result.receipt.opposition_executed = false;
    result.receipt.governed_projection_executed = true;
    result.receipt.no_silent_promotion = true;
    result.receipt.terminal_cause = "g0_evidence_projection_no_hypothesis_selection";
    return result;
  }

  DialecticEngine dialectic(db_);
  FiberBundleBuilder bundle_builder;
  LyapunovCritic critic;
  EpistemicController controller;
  CorrectiveEscape escape;
  RecursiveSelfHealingController self_healing;

  BundleSet expansion = dialectic.expand(
      query, query_signature, options.dialectic, resolved_snapshot);
  if (options.path_verifier) {
    apply_path_verifier(&expansion, *options.path_verifier, query,
                        query_signature, options.dialectic.mode);
  }
  result.initial_bundle = bundle_builder.build(expansion);
  result.initial_stability = critic.assess(
      result.initial_bundle, options.dialectic.mode,
      options.stability_weights, options.stability_thresholds,
      options.lyapunov_weights, options.lyapunov_targets);
  result.initial_admissibility = controller.assess(
      result.initial_bundle, result.initial_stability,
      options.dialectic.mode, options.stability_thresholds);
  result.initial_escape = escape.plan(
      result.initial_bundle, result.initial_stability,
      options.dialectic.mode);
  result.initial_convergence = controller.converge(
      result.initial_bundle, result.initial_admissibility,
      result.initial_stability, options.dialectic);
  result.initial_opposition =
      options.enable_dwm
          ? controller.oppose(
                result.initial_bundle, result.initial_convergence,
                result.initial_admissibility, result.initial_stability,
                options.dialectic)
          : OppositionReport{};
  result.initial_self_healing = self_healing.plan(
      result.initial_stability, result.initial_opposition,
      result.initial_escape);

  NativeEpistemicEvent hypothesis_set;
  hypothesis_set.source = EpistemicEventSource::HypoKosh;
  hypothesis_set.type = EpistemicEventType::HypothesisSet;
  hypothesis_set.competing_hypotheses =
      hypothesis_nodes(result.initial_bundle);
  hypothesis_set.evidence_edges = evidence_edges_for_nodes(
      result.initial_bundle, hypothesis_set.competing_hypotheses);
  hypothesis_set.evidence_family_ids = evidence_families_for_nodes(
      result.initial_bundle, hypothesis_set.competing_hypotheses);
  hypothesis_set.epistemic_state = "competing";
  hypothesis_set.reason =
      "runtime-observed target fibers available for governed competition";
  append_epistemic_event(&result.receipt, std::move(hypothesis_set));

  NativeEpistemicEvent initial_decision;
  initial_decision.source = EpistemicEventSource::HypoKosh;
  initial_decision.type = EpistemicEventType::Decision;
  initial_decision.hypothesis_node =
      result.initial_convergence.has_answer
          ? result.initial_convergence.primary_node
          : 0;
  initial_decision.competing_hypotheses =
      hypothesis_nodes(result.initial_bundle);
  initial_decision.evidence_edges =
      all_evidence_edges(result.initial_convergence);
  initial_decision.evidence_family_ids = selected_evidence_families(
      result.initial_bundle, result.initial_convergence);
  initial_decision.epistemic_state =
      result.initial_convergence.has_answer ? "selected" : "abstain";
  initial_decision.reason =
      result.initial_convergence.has_answer
          ? "initial governed convergence selected a hypothesis"
          : "initial governed convergence emitted no answer";
  append_epistemic_event(&result.receipt, std::move(initial_decision));

  result.final_bundle = result.initial_bundle;
  result.final_stability = result.initial_stability;
  result.final_admissibility = result.initial_admissibility;
  result.final_convergence = result.initial_convergence;
  result.final_opposition = result.initial_opposition;
  result.final_self_healing = result.initial_self_healing;

  std::vector<LyapunovSample> samples;
  samples.push_back(
      {reasoning_state_hash(result.initial_bundle, options.dialectic),
       result.initial_stability});

  DialecticOptions current_options = options.dialectic;
  EscapePlan current_escape = result.initial_escape;
  uint32_t consecutive_unchanged_bundles = 0;
  GovernedEpistemicStatus current_trace_status = project_status(
      result.final_convergence, result.final_opposition,
      result.final_stability, result.final_admissibility,
      current_options.mode, options.stability_thresholds).status;
  for (uint32_t round = 0; round < options.max_recursive_cycles; ++round) {
    RecoveryRoundTrace trace;
    trace.round_index = round;
    trace.expansion_rounds_before = result.receipt.expansion_rounds;
    trace.requires_escape = result.final_stability.requires_escape;
    trace.searchable_escape = has_searchable_action(current_escape);
    trace.recovery_search_requested =
        trace.requires_escape && trace.searchable_escape;
    trace.dialectical_challenge_present =
        options.enable_dwm &&
        result.final_opposition.dialectical_challenge;
    trace.corroboration_search_requested =
        result.final_opposition.corroboration_search_required;
    const ExpansionOpportunity expansion_opportunity =
        assess_expansion_opportunity(
            db_, result.final_bundle, result.final_opposition,
            current_options, resolved_snapshot);
    trace.expansion_opportunity_available =
        expansion_opportunity.available;
    trace.expansion_opportunity_class =
        expansion_opportunity.classification;
    trace.opposition_search_requested =
        options.enable_dwm &&
        options.enable_opposition_research &&
        result.final_opposition.requests_reexpansion &&
        expansion_opportunity.available;
    trace.generic_expansion_allowed =
        current_escape.generic_expansion_allowed;
    trace.depth_repair_active = depth_repair_active(current_escape);
    trace.previous_max_hops = current_options.max_hops;
    trace.previous_semantic_candidates =
        current_options.semantic_candidates;
    trace.previous_max_paths = current_options.max_paths;
    trace.previous_max_paths_per_root =
        current_options.max_paths_per_root;
    trace.previous_max_visited_states =
        current_options.max_visited_states;
    trace.previous_minimum_confidence =
        current_options.minimum_confidence;
    trace.previous_reopen_nodes =
        current_options.reopen_nodes.size();
    trace.previous_bundle_hash =
        result.final_bundle.immutable_hash;
    trace.previous_visited_states =
        result.final_bundle.visited_states;
    trace.previous_truncated = result.final_bundle.truncated;

    trace.previous_has_answer = result.final_convergence.has_answer;
    trace.previous_primary_node =
        trace.previous_has_answer ? result.final_convergence.primary_node : 0;
    trace.previous_status = current_trace_status;
    trace.previous_committed_node =
        trace.previous_has_answer &&
                trace_status_commits(trace.previous_status)
            ? trace.previous_primary_node
            : 0;
    trace.previous_target_ranking =
        controller.inspect_targets(result.final_bundle);

    // Default the next snapshot to the current one so early-stop rounds have a
    // complete before/after record without implying unobserved change.
    trace.next_bundle_hash = trace.previous_bundle_hash;
    trace.next_visited_states = trace.previous_visited_states;
    trace.next_truncated = trace.previous_truncated;
    trace.next_has_answer = trace.previous_has_answer;
    trace.next_primary_node = trace.previous_primary_node;
    trace.next_status = trace.previous_status;
    trace.next_committed_node = trace.previous_committed_node;
    trace.next_target_ranking = trace.previous_target_ranking;

    const bool recovery_search_requested =
        trace.recovery_search_requested;
    const bool opposition_search_requested =
        trace.opposition_search_requested;
    if (trace.dialectical_challenge_present) {
      NativeEpistemicEvent challenge;
      challenge.round_index = round;
      challenge.source = EpistemicEventSource::DialecticalModelWorlds;
      challenge.type = EpistemicEventType::Challenge;
      challenge.hypothesis_node =
          result.final_convergence.has_answer
              ? result.final_convergence.primary_node
              : 0;
      challenge.competing_hypotheses =
          hypothesis_nodes(result.final_bundle);
      challenge.reopen_nodes =
          opposition_search_requested
              ? result.final_opposition.reopen_nodes
              : std::vector<uint32_t>{};
      challenge.evidence_edges = evidence_edges_for_nodes(
          result.final_bundle, result.final_opposition.reopen_nodes);
      challenge.evidence_family_ids = evidence_families_for_nodes(
          result.final_bundle, result.final_opposition.reopen_nodes);
      challenge.epistemic_state =
          opposition_search_requested
              ? "challenged_expandable"
              : "challenged_frontier_exhausted";
      challenge.reason =
          result.final_opposition.challenged_claims.empty()
              ? "material opposition challenges the current hypothesis"
              : result.final_opposition.challenged_claims.front();
      append_epistemic_event(&result.receipt, std::move(challenge));
    }
    if (trace.corroboration_search_requested) {
      NativeEpistemicEvent corroboration;
      corroboration.round_index = round;
      corroboration.source = EpistemicEventSource::HypoKosh;
      corroboration.type = EpistemicEventType::CorroborationSearch;
      corroboration.hypothesis_node =
          result.final_convergence.has_answer
              ? result.final_convergence.primary_node
              : 0;
      corroboration.competing_hypotheses =
          hypothesis_nodes(result.final_bundle);
      corroboration.evidence_edges =
          all_evidence_edges(result.final_convergence);
      corroboration.evidence_family_ids = selected_evidence_families(
          result.final_bundle, result.final_convergence);
      corroboration.epistemic_state = "evidence_insufficient";
      corroboration.reason =
          result.final_opposition.corroboration_questions.empty()
              ? "additional independent corroboration is required"
              : result.final_opposition.corroboration_questions.front();
      append_epistemic_event(&result.receipt, std::move(corroboration));
    }
    if (!recovery_search_requested && !opposition_search_requested) {
      trace.next_max_hops = current_options.max_hops;
      trace.next_semantic_candidates =
          current_options.semantic_candidates;
      trace.next_max_paths = current_options.max_paths;
      trace.next_max_paths_per_root =
          current_options.max_paths_per_root;
      trace.next_max_visited_states =
          current_options.max_visited_states;
      trace.next_minimum_confidence =
          current_options.minimum_confidence;
      trace.next_reopen_nodes =
          current_options.reopen_nodes.size();
      trace.stop_reason = "no_search_requested";
      result.receipt.recovery_stop_reason = trace.stop_reason;
      result.receipt.recovery_trace.push_back(std::move(trace));
      break;
    }

