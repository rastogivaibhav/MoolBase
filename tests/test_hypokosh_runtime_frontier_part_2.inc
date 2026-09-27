  require(db.put_node(node("Connection pool exhausted", {0.2f, 0.8f, 0.0f}, signature), &dependency_a), "put dep a");
  require(db.put_node(node("Database wait queue increased", {0.1f, 0.9f, 0.0f}, signature), &dependency_b), "put dep b");
  require(db.put_node(node("Checkout failures increased", {0.0f, 1.0f, 0.0f}, signature, false, true), &symptom), "put symptom");
  require(db.put_node(node("Traffic spike hypothesis", {-1.0f, 0.0f, 0.0f}, signature, true), &alternative), "put alternative");

  require(db.put_edge({root_node, dependency_a, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.96,
                       {{"source_id", "release-log"}}}), "edge root-dep-a");
  require(db.put_edge({dependency_a, symptom, EdgeOrigin::Discovered, EdgeRole::Causal, 0.95,
                       {{"source_id", "heap-profile"}}}), "edge dep-a-symptom");
  require(db.put_edge({root_node, dependency_b, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.92,
                       {{"source_id", "config-diff"}}}), "edge root-dep-b");
  require(db.put_edge({dependency_b, symptom, EdgeOrigin::Observed, EdgeRole::Supports, 0.90,
                       {{"source_id", "database-metrics"}}}), "edge dep-b-symptom");
  require(db.put_edge({alternative, symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.55,
                       {{"source_id", "traffic-dashboard"}}}), "edge alternative");

  RuntimeOptions runtime_options;
  runtime_options.dialectic.mode = QueryMode::Empirical;
  runtime_options.dialectic.semantic_candidates = 4;
  runtime_options.dialectic.max_hops = 4;
  runtime_options.dialectic.max_paths = 32;
  runtime_options.dialectic.max_paths_per_root = 8;
  runtime_options.dialectic.minimum_confidence = 0.35;
  runtime_options.dialectic.reexpansion_threshold = 0.20;
  runtime_options.max_recursive_cycles = 2;

  ModelWorld runtime_world;
  CompleteHypoKoshRuntime runtime(db, &runtime_world);
  const HypoKoshRuntimeResult result =
      runtime.reason({0.0f, 1.0f, 0.0f}, signature, runtime_options);

  assert(result.receipt.hypokosh_capability_enabled);
  assert(result.receipt.graphene_executed);
  assert(result.receipt.fiber_bundle_built);
  assert(result.receipt.stability_critic_executed);
  assert(result.receipt.lyapunov_trajectory_executed);
  assert(result.receipt.lyapunov_certificate_valid);
  assert(!result.lyapunov.observations.empty());
  assert(result.lyapunov.certificate.weights_positive);
  assert(result.lyapunov.certificate.state_bounded);
  assert(result.lyapunov.certificate.energy_nonnegative);
  assert(result.lyapunov.certificate.quadratic_bounds_valid);
  assert(result.lyapunov.certificate.final_energy >= 0.0);
  assert(result.lyapunov.certificate.final_energy <= 1.0);
  assert(result.receipt.escape_considered);
  assert(result.receipt.convergence_executed);
  assert(result.receipt.opposition_executed);
  assert(result.receipt.governed_projection_executed);
  assert(!result.receipt.epistemic_events.empty());
  assert(result.receipt.epistemic_events.front().sequence == 1);
  assert(result.receipt.epistemic_events.front().type ==
         EpistemicEventType::HypothesisSet);
  assert(result.receipt.epistemic_events.front().source ==
         EpistemicEventSource::HypoKosh);
  assert(result.receipt.epistemic_events.back().type ==
         EpistemicEventType::Terminal);
  assert(result.receipt.epistemic_events.back().source ==
         EpistemicEventSource::GrapheneCore);
  for (size_t index = 0;
       index < result.receipt.epistemic_events.size(); ++index) {
    const NativeEpistemicEvent& event =
        result.receipt.epistemic_events[index];
    assert(event.sequence == index + 1);
    assert(event.source != EpistemicEventSource::DialecticalModelWorlds);
  }
  assert(result.receipt.initial_bundle_hash != 0);
  assert(result.receipt.final_bundle_hash != 0);
  assert(result.primary_node == root_node);
  assert(result.status != GovernedEpistemicStatus::Abstain);
  assert(result.status != GovernedEpistemicStatus::EvidenceRequired);
  assert(!result.evidence_edges.empty());
  assert(!result.final_self_healing.repairs.empty());
  assert(result.final_self_healing.rerun_required || !result.final_stability.requires_escape);
  const CompactEpistemicReceipt compact_result =
      build_compact_epistemic_receipt(result);
  assert(compact_result.hypokosh_capability_enabled);
  assert(compact_result.dwm_capability_enabled);
  assert(compact_result.graphene_executed);
  assert(compact_result.convergence_executed);
  assert(compact_result.opposition_executed);
  assert(!compact_result.opposition_research_enabled);
  assert(compact_result.governed_projection_executed);
  assert(!compact_result.terminal_cause.empty());

  // RC3: DWM may affect terminal governance even when opposition research is
  // disabled and therefore no Challenge/Reopen event is emitted. The compact
  // receipt must still preserve that DWM/opposition executed.
  if (result.status == GovernedEpistemicStatus::Contested &&
      result.receipt.terminal_cause ==
          "dialectic_opposition_blocks_resolution") {
    bool saw_dwm_transition = false;
    for (const NativeEpistemicEvent& event :
         compact_result.epistemic_events) {
      saw_dwm_transition =
          saw_dwm_transition ||
          event.source == EpistemicEventSource::DialecticalModelWorlds;
    }
    assert(!saw_dwm_transition);
    assert(compact_result.dwm_capability_enabled);
    assert(compact_result.opposition_executed);
    assert(compact_result.terminal_cause ==
           "dialectic_opposition_blocks_resolution");
  }

  assert(!runtime_world.nodes().empty());
  assert(runtime_world.audit().empty());
  const fs::path model_world_path = directory / "model-world.snapshot";
  require(runtime_world.save(model_world_path), "save model world");
  ModelWorld reloaded_world;
  require(reloaded_world.load(model_world_path), "load model world");
  assert(reloaded_world.nodes().size() == runtime_world.nodes().size());
  assert(reloaded_world.events().size() == runtime_world.events().size());
  assert(reloaded_world.event_log_hash() == runtime_world.event_log_hash());
  assert(reloaded_world.audit().empty());

  // RC4: when opposition research is enabled, an actual DWM challenge must
  // be visible as Challenge/Reopen events as well as durable capability flags.
  RuntimeOptions g2_research_options = runtime_options;
  g2_research_options.enable_dwm = true;
  g2_research_options.enable_opposition_research = true;
  g2_research_options.update_model_world = false;
  const HypoKoshRuntimeResult g2_research_result =
      runtime.reason({0.0f, 1.0f, 0.0f}, signature, g2_research_options);
  assert(g2_research_result.receipt.dwm_capability_enabled);
  assert(g2_research_result.receipt.opposition_research_enabled);
  assert(g2_research_result.receipt.opposition_executed);
  bool saw_challenge = false;
  bool saw_reopen = false;
  for (const NativeEpistemicEvent& event :
       g2_research_result.receipt.epistemic_events) {
    saw_challenge =
        saw_challenge || event.type == EpistemicEventType::Challenge;
    saw_reopen =
        saw_reopen || event.type == EpistemicEventType::Reopen;
  }
  assert(saw_challenge);
  assert(saw_reopen);
  const CompactEpistemicReceipt g2_research_compact =
      build_compact_epistemic_receipt(g2_research_result);
  assert(g2_research_compact.dwm_capability_enabled);
  assert(g2_research_compact.opposition_research_enabled);
  assert(g2_research_compact.opposition_executed);
  assert(g2_research_compact.bounded_recovery_executed);

  // Production capability boundary: disabling DWM must prevent opposition
  // execution even when opposition research is explicitly requested.
  RuntimeOptions g1_options = runtime_options;
  g1_options.enable_dwm = false;
  g1_options.enable_opposition_research = true;
  g1_options.update_model_world = false;
  const HypoKoshRuntimeResult g1_result =
      runtime.reason({0.0f, 1.0f, 0.0f}, signature, g1_options);
  assert(g1_result.receipt.hypokosh_capability_enabled);
  assert(!g1_result.receipt.dwm_capability_enabled);
  assert(!g1_result.receipt.opposition_research_enabled);
  assert(!g1_result.receipt.opposition_executed);
  for (const NativeEpistemicEvent& event :
       g1_result.receipt.epistemic_events) {
    assert(event.source != EpistemicEventSource::DialecticalModelWorlds);
  }
  const CompactEpistemicReceipt g1_compact =
      build_compact_epistemic_receipt(g1_result);
  assert(g1_compact.hypokosh_capability_enabled);
  assert(!g1_compact.dwm_capability_enabled);
  assert(!g1_compact.opposition_executed);
  assert(g1_compact.graphene_executed);
  assert(g1_compact.convergence_executed);

  // Genuine G0 must execute Graphene evidence/provenance only. Invalid
  // DWM-without-HypoKosh remains rejected, while valid G0 must not construct
  // or execute convergence/opposition semantics.
  RuntimeOptions invalid_g0 = runtime_options;
  invalid_g0.enable_hypokosh = false;
  bool invalid_dependency_rejected = false;
  try {
    (void)runtime.reason({0.0f, 1.0f, 0.0f}, signature, invalid_g0);
  } catch (const std::invalid_argument&) {
    invalid_dependency_rejected = true;
  }
  assert(invalid_dependency_rejected);

  RuntimeOptions g0_options = runtime_options;
  g0_options.enable_hypokosh = false;
  g0_options.enable_dwm = false;
  g0_options.enable_opposition_research = false;
  g0_options.update_model_world = false;
  ModelWorld g0_world;
  CompleteHypoKoshRuntime g0_runtime(db, &g0_world);
  const HypoKoshRuntimeResult g0_result =
      g0_runtime.reason({0.0f, 1.0f, 0.0f}, signature, g0_options);
  assert(g0_result.receipt.graphene_executed);
  assert(!g0_result.receipt.hypokosh_capability_enabled);
  assert(!g0_result.receipt.dwm_capability_enabled);
  assert(!g0_result.receipt.stability_critic_executed);
  assert(!g0_result.receipt.epistemic_admissibility_executed);
  assert(!g0_result.receipt.convergence_executed);
  assert(!g0_result.receipt.opposition_executed);
  assert(!g0_result.receipt.bounded_recovery_executed);
  assert(g0_result.receipt.governed_projection_executed);
  assert(g0_result.receipt.terminal_cause ==
         "g0_evidence_projection_no_hypothesis_selection");
  assert(!g0_result.final_bundle.fibers.empty());
  assert(!g0_result.evidence_edges.empty());
  const CompactEpistemicReceipt g0_compact =
      build_compact_epistemic_receipt(g0_result);
  assert(!g0_compact.hypokosh_capability_enabled);
  assert(!g0_compact.dwm_capability_enabled);
  assert(g0_compact.graphene_executed);
  assert(!g0_compact.convergence_executed);
  assert(!g0_compact.opposition_executed);
  assert(g0_compact.terminal_cause ==
         "g0_evidence_projection_no_hypothesis_selection");
  assert(g0_world.nodes().empty());
  for (const NativeEpistemicEvent& event :
       g0_result.receipt.epistemic_events) {
    assert(event.source == EpistemicEventSource::GrapheneCore);
    assert(event.type != EpistemicEventType::Challenge);
    assert(event.type != EpistemicEventType::Reopen);
    assert(event.type != EpistemicEventType::Revision);
  }

  const HypoKoshRuntimeResult repeated =
      runtime.reason({0.0f, 1.0f, 0.0f}, signature, runtime_options);
  assert(repeated.primary_node == result.primary_node);
  assert(repeated.status == result.status);
  assert(repeated.receipt.initial_bundle_hash == result.receipt.initial_bundle_hash);
  assert(repeated.receipt.final_bundle_hash == result.receipt.final_bundle_hash);
  assert(repeated.lyapunov.certificate.final_energy ==
         result.lyapunov.certificate.final_energy);
  assert(repeated.lyapunov.certificate.violations ==
         result.lyapunov.certificate.violations);

  // A four-edge chain requires all three permitted recovery cycles. The
  // completed FiberBundle can remain unchanged while intermediate nodes are
  // visited, so the runtime must not confuse frontier progress with a limit
  // cycle or no-new-information stop.
  const uint64_t deep_signature = signature_for(10, 11);
  uint32_t deep_root = 0;
  uint32_t bridge_one = 0;
  uint32_t bridge_two = 0;
  uint32_t bridge_three = 0;
  uint32_t deep_symptom = 0;
  require(db.put_node(node("Deep causal root", {1.0f, 0.0f, 0.0f}, deep_signature, true), &deep_root), "put deep root");
  require(db.put_node(node("Deep bridge one", {0.8f, 0.2f, 0.0f}, deep_signature), &bridge_one), "put bridge one");
  require(db.put_node(node("Deep bridge two", {0.4f, 0.6f, 0.0f}, deep_signature), &bridge_two), "put bridge two");
  require(db.put_node(node("Deep bridge three", {0.1f, 0.3f, 0.6f}, deep_signature), &bridge_three), "put bridge three");
  require(db.put_node(node("Deep observed symptom", {0.0f, 0.0f, 1.0f}, deep_signature, false, true), &deep_symptom), "put deep symptom");
  require(db.put_edge({deep_root, bridge_one, EdgeOrigin::Observed, EdgeRole::Causal, 0.99,
                       {{"source_id", "deep-root-source"}}}), "edge deep root-one");
  require(db.put_edge({bridge_one, bridge_two, EdgeOrigin::Discovered, EdgeRole::Mechanistic, 0.99,
                       {{"source_id", "deep-bridge-one"}}}), "edge deep one-two");
  require(db.put_edge({bridge_two, bridge_three, EdgeOrigin::Discovered, EdgeRole::Mechanistic, 0.99,
                       {{"source_id", "deep-bridge-two"}}}), "edge deep two-three");
  require(db.put_edge({bridge_three, deep_symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.99,
                       {{"source_id", "deep-symptom-source"}}}), "edge deep three-symptom");

  RuntimeOptions deep_options;
  deep_options.dialectic.mode = QueryMode::Empirical;
  deep_options.dialectic.semantic_candidates = 1;
  deep_options.dialectic.max_hops = 1;
  deep_options.dialectic.max_paths = 8;
  deep_options.dialectic.max_paths_per_root = 4;
  deep_options.dialectic.max_visited_states = 64;
  deep_options.dialectic.minimum_confidence = 0.35;
  deep_options.max_recursive_cycles = 3;
  deep_options.unchanged_recovery_patience = 2;
  deep_options.enable_opposition_research = false;
  deep_options.update_model_world = false;

  const HypoKoshRuntimeResult deep_result =
      runtime.reason({0.0f, 0.0f, 1.0f}, deep_signature, deep_options);
  std::cerr << "deep_recovery_trace"
            << " expansion_rounds=" << deep_result.receipt.expansion_rounds
            << " frontier_progress_rounds="
            << deep_result.receipt.frontier_progress_rounds
            << " unchanged_bundle_rounds="
            << deep_result.receipt.unchanged_bundle_rounds
            << " stop_reason="
            << deep_result.receipt.recovery_stop_reason << "\n";
  for (const RecoveryRoundTrace& trace :
       deep_result.receipt.recovery_trace) {
    std::cerr
        << "deep_recovery_round"
        << " round=" << trace.round_index
        << " expansions_before=" << trace.expansion_rounds_before
        << " requires_escape=" << trace.requires_escape
        << " searchable_escape=" << trace.searchable_escape
        << " recovery_requested=" << trace.recovery_search_requested
        << " opposition_requested=" << trace.opposition_search_requested
        << " generic_allowed=" << trace.generic_expansion_allowed
        << " depth_repair=" << trace.depth_repair_active
        << " options_changed=" << trace.options_changed
        << " max_hops=" << trace.previous_max_hops
        << "->" << trace.next_max_hops
        << " semantic_candidates="
        << trace.previous_semantic_candidates
        << "->" << trace.next_semantic_candidates
        << " max_paths=" << trace.previous_max_paths
        << "->" << trace.next_max_paths
        << " max_paths_per_root="
        << trace.previous_max_paths_per_root
        << "->" << trace.next_max_paths_per_root
        << " visit_budget=" << trace.previous_max_visited_states
        << "->" << trace.next_max_visited_states
        << " min_confidence="
        << trace.previous_minimum_confidence
        << "->" << trace.next_minimum_confidence
        << " reopen_nodes=" << trace.previous_reopen_nodes
        << "->" << trace.next_reopen_nodes
        << " bundle_hash=" << trace.previous_bundle_hash
        << "->" << trace.next_bundle_hash
        << " visited=" << trace.previous_visited_states
        << "->" << trace.next_visited_states
        << " truncated=" << trace.previous_truncated
        << "->" << trace.next_truncated
        << " bundle_changed=" << trace.bundle_changed
        << " frontier_progress=" << trace.frontier_progress
        << " unchanged_rounds="
        << trace.consecutive_unchanged_bundles
        << " lyapunov_limit_cycle="
        << trace.lyapunov_limit_cycle
        << " lyapunov_oscillation="
        << trace.lyapunov_oscillation
        << " stop_reason=" << trace.stop_reason
        << "\n";
  }
  assert(deep_result.primary_node == deep_root);
  assert(deep_result.receipt.recovery_trace.size() == 3);
  assert(deep_result.receipt.recovery_trace[0].depth_repair_active);
  assert(deep_result.receipt.recovery_trace[0].previous_max_hops == 1);
  assert(deep_result.receipt.recovery_trace[0].next_max_hops == 2);
  assert(deep_result.receipt.recovery_trace[0].previous_semantic_candidates == 1);
  assert(deep_result.receipt.recovery_trace[0].next_semantic_candidates == 1);
  assert(deep_result.receipt.recovery_trace[1].previous_max_hops == 2);
  assert(deep_result.receipt.recovery_trace[1].next_max_hops == 3);
  assert(deep_result.receipt.recovery_trace[1].next_semantic_candidates == 1);
  assert(deep_result.receipt.recovery_trace[2].previous_max_hops == 3);
  assert(deep_result.receipt.recovery_trace[2].next_max_hops == 4);
  assert(deep_result.receipt.recovery_trace[2].next_semantic_candidates == 1);
  assert(deep_result.receipt.expansion_rounds == 3);
  assert(deep_result.receipt.frontier_progress_rounds >= 2);
  assert(!deep_result.receipt.stopped_for_no_progress);
  assert(!deep_result.lyapunov.certificate.limit_cycle_detected);

  require(db.close(), "close runtime database");
  fs::remove_all(directory);
  std::cout << "complete_hypokosh_runtime_contract_passed=true\n";
  return 0;
}
