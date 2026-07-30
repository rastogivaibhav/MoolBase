          "put symptom");

  // The initial direct explanation is intentionally weaker than the hidden
  // correct explanation, but it is reachable at the initial one-hop budget.
  require(db.put_edge(
              {wrong_root, symptom, EdgeOrigin::Observed, EdgeRole::Causal,
               scenario.initial_is_correct ? 0.98 : 0.68,
               {{"source_id", "initial-source"}}}),
          "put initial explanation");

  if (scenario.initial_is_correct) {
    return {wrong_root, wrong_root, symptom, query, signature};
  }

  if (scenario.independent_candidate) {
    uint32_t independent_anchor = 0;
    require(db.put_node(
                make_node("independent candidate",
                          near_query(dimension, 0.992f, 6), signature),
                &independent_anchor),
            "put independent candidate");
    require(db.put_edge(
                {correct_root, independent_anchor, EdgeOrigin::Discovered,
                 EdgeRole::Causal, 0.99,
                 {{"source_id", "independent-source"}}}),
            "put independent candidate edge");
  } else if (scenario.minority_path) {
    // Inserted after the wrong path. The initial max_paths=1 truncates before it.
    require(db.put_edge(
                {correct_root, symptom, EdgeOrigin::Discovered,
                 EdgeRole::Causal, 0.99,
                 {{"source_id", "minority-source"}}}),
            "put minority edge");
  } else {
    uint32_t downstream = symptom;
    for (uint32_t hop = 1; hop < scenario.chain_hops; ++hop) {
      uint32_t bridge = 0;
      require(db.put_node(
                  make_node("hidden bridge " + std::to_string(hop),
                            unit_vector(dimension, 10 + (hop % 5)), signature),
                  &bridge),
              "put bridge");
      require(db.put_edge(
                  {bridge, downstream, EdgeOrigin::Discovered,
                   EdgeRole::Mechanistic, 0.995,
                   {{"source_id", "bridge-source-" + std::to_string(hop)}}}),
              "put bridge edge");
      downstream = bridge;
    }
    require(db.put_edge(
                {correct_root, downstream, EdgeOrigin::Observed,
                 EdgeRole::Causal, 0.995,
                 {{"source_id", "root-source"}}}),
            "put root chain edge");
  }

  return {correct_root, wrong_root, symptom, query, signature};
}

CycleObservation observe(const BundleSet& bundle,
                         const ConvergedAnswer& answer,
                         const OppositionReport& opposition,
                         uint32_t expected_root,
                         uint32_t cycle) {
  CycleObservation value;
  value.cycle = cycle;
  value.primary = answer.primary_node;
  value.has_answer = answer.has_answer;
  value.correct = answer.has_answer && answer.primary_node == expected_root;
  value.opposition_requested = opposition.requests_reexpansion;
  value.roots = bundle.roots.size();
  value.paths = path_count(bundle);
  value.visited_states = bundle.visited_states;
  value.truncated = bundle.truncated;
  value.bundle_signature = bundle_signature(bundle);
  value.confidence = answer.confidence;
  return value;
}

RunResult run_case(const Scenario& scenario, uint32_t seed, Policy policy) {
  const fs::path directory = fs::temp_directory_path() /
      ("graphenedb_dialectic_intervention_" + scenario.family + "_" +
       std::to_string(seed) + "_" + policy_name(policy));
  fs::remove_all(directory);
  GrapheneDB db;
  DBOptions db_options;
  db_options.dimension = 16;
  db_options.fsync_on_commit = false;
  db_options.vector_index_kind = VectorIndexKind::Flat;
  require(db.open(directory, db_options), "open intervention db");
  const BuiltCase built = build_case(db, scenario, seed);

  DialecticEngine engine(db);
  DialecticOptions options = initial_options(scenario);
  BundleSet bundle = engine.expand(built.query, built.signature, options);
  ConvergedAnswer answer = engine.converge(bundle, options);
  OppositionReport opposition = engine.oppose(bundle, answer, options);

  RunResult result;
  result.family = scenario.family;
  result.policy = policy_name(policy);
  result.seed = seed;
  result.expected_root = built.expected_root;
  result.initial_correct = answer.has_answer && answer.primary_node == built.expected_root;
  result.observations.push_back(observe(bundle, answer, opposition,
                                        built.expected_root, 0));
  result.total_visited_states += bundle.visited_states;
  if (result.initial_correct) result.first_correct_cycle = 0;

  uint64_t previous_signature = bundle_signature(bundle);
  size_t previous_visited = bundle.visited_states;
  uint32_t unchanged_rounds = 0;
  const uint32_t max_cycles = policy == Policy::NoCycle ? 0 : 3;
  for (uint32_t cycle = 1; cycle <= max_cycles; ++cycle) {
    if (!opposition.requests_reexpansion) break;
    options = policy == Policy::BroadForced
                  ? broad_options(options)
                  : targeted_options(options, scenario.escape);
    bundle = engine.expand(built.query, built.signature, options);
    answer = engine.converge(bundle, options);
    opposition = engine.oppose(bundle, answer, options);
    const uint64_t signature = bundle_signature(bundle);
    const bool new_information = signature != previous_signature;
    const bool frontier_progress = bundle.visited_states > previous_visited;
    unchanged_rounds = new_information ? 0 : unchanged_rounds + 1;
    result.observations.push_back(observe(bundle, answer, opposition,
                                          built.expected_root, cycle));
    result.total_visited_states += bundle.visited_states;
    ++result.cycles_executed;
    if (new_information &&
        (answer.primary_node == built.expected_root ||
         bundle.roots.size() > result.observations.front().roots)) {
      result.useful_new_information = true;
    }
    if (answer.has_answer && answer.primary_node == built.expected_root &&
        result.first_correct_cycle == 999) {
      result.first_correct_cycle = cycle;
    }
    if (policy == Policy::TargetedCurrentStop && !new_information) break;
    if (policy == Policy::TargetedFrontierAware && !new_information &&
        !frontier_progress && unchanged_rounds > 2) break;
    previous_signature = signature;
