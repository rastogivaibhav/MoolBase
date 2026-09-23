
GovernedEpistemicStatus project_status(
    const ConvergedAnswer& convergence,
    const OppositionReport& opposition,
    const StabilityAssessment& stability,
    const EpistemicAdmissibility& admissibility,
    QueryMode mode) {
  if (!convergence.has_answer) {
    return stability.missing_evidence_penalty > 0.0
               ? GovernedEpistemicStatus::EvidenceRequired
               : GovernedEpistemicStatus::Abstain;
  }
  if (admissibility.semantic_verification ==
      SemanticVerificationStatus::Contradicted) {
    return GovernedEpistemicStatus::Contested;
  }
  if (admissibility.contradiction_blocks_resolution ||
      opposition.opposition_score >= 0.50) {
    return GovernedEpistemicStatus::Contested;
  }
  if (!admissibility.evidence_admissible) {
    return stability.completeness_score < 0.50 ||
                   stability.provenance_score < 0.25
               ? GovernedEpistemicStatus::EvidenceRequired
               : GovernedEpistemicStatus::Abstain;
  }
  if (mode == QueryMode::Theoretical &&
      admissibility.semantic_verification ==
          SemanticVerificationStatus::Unverified) {
    return GovernedEpistemicStatus::Speculative;
  }
  if (stability.stable &&
      admissibility.sufficient_independent_support &&
      admissibility.semantic_verification ==
          SemanticVerificationStatus::Verified &&
      opposition.opposition_score == 0.0) {
    return GovernedEpistemicStatus::Resolved;
  }
  return GovernedEpistemicStatus::ProvisionallyResolved;
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
  options.max_recursive_cycles =
      std::min<uint32_t>(options.max_recursive_cycles, 3);
  options.unchanged_recovery_patience =
      std::min<uint32_t>(options.unchanged_recovery_patience, 3);
  const uint64_t resolved_snapshot =
      snapshot_version == kInfVersion ? db_.snapshot() : snapshot_version;

  HypoKoshRuntimeResult result;
  result.receipt.opposition_research_enabled =
      options.enable_opposition_research;
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
      options.dialectic.mode);
  result.initial_escape = escape.plan(
      result.initial_bundle, result.initial_stability,
      options.dialectic.mode);
  result.initial_convergence = controller.converge(
      result.initial_bundle, result.initial_admissibility,
      result.initial_stability, options.dialectic);
  result.initial_opposition = controller.oppose(
      result.initial_bundle, result.initial_convergence,
      result.initial_admissibility, result.initial_stability,
      options.dialectic);
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
  for (uint32_t round = 0; round < options.max_recursive_cycles; ++round) {
    RecoveryRoundTrace trace;
    trace.round_index = round;
    trace.expansion_rounds_before = result.receipt.expansion_rounds;
    trace.requires_escape = result.final_stability.requires_escape;
    trace.searchable_escape = has_searchable_action(current_escape);
    trace.recovery_search_requested =
        trace.requires_escape && trace.searchable_escape;
    trace.opposition_search_requested =
        options.enable_opposition_research &&
        result.final_opposition.requests_reexpansion;
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

    const bool recovery_search_requested =
        trace.recovery_search_requested;
    const bool opposition_search_requested =
        trace.opposition_search_requested;
    if (opposition_search_requested) {
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
          result.final_opposition.reopen_nodes;
      challenge.evidence_edges = evidence_edges_for_nodes(
          result.final_bundle, challenge.reopen_nodes);
      challenge.evidence_family_ids = evidence_families_for_nodes(
          result.final_bundle, challenge.reopen_nodes);
      challenge.epistemic_state = "challenged";
      challenge.reason =
          result.final_opposition.challenged_claims.empty()
              ? "opposition requested bounded re-expansion"
              : result.final_opposition.challenged_claims.front();
      append_epistemic_event(&result.receipt, std::move(challenge));
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

