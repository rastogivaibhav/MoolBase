      hash = append_hash(hash, path.contains_contradiction ? 1 : 0);
    }
  }
  return hash;
}

DialecticOptions initial_options(const Scenario& scenario) {
  DialecticOptions options;
  options.mode = QueryMode::Balanced;
  options.semantic_candidates = 1;
  options.max_hops = 1;
  options.max_paths = scenario.minority_path ? 1 : 8;
  options.max_paths_per_root = scenario.minority_path ? 1 : 4;
  options.max_visited_states = 64;
  options.max_selected_paths = 4;
  options.max_opposition_rounds = 0;
  options.minimum_confidence = 0.35;
  options.reexpansion_threshold = 0.20;
  return options;
}

DialecticOptions targeted_options(DialecticOptions options, EscapeKind escape) {
  switch (escape) {
    case EscapeKind::MissingHop:
      options.max_hops = std::min<uint32_t>(16, options.max_hops + 1);
      break;
    case EscapeKind::SeekIndependent:
      options.semantic_candidates =
          std::min<size_t>(64, options.semantic_candidates + 4);
      options.max_paths =
          std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
      break;
    case EscapeKind::ExpandMinority:
      options.max_paths_per_root =
          std::min<size_t>(64, options.max_paths_per_root + 4);
      options.max_paths =
          std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
      break;
    case EscapeKind::None:
      break;
  }
  options.max_visited_states =
      std::min<size_t>(200000,
                       std::max<size_t>(2, options.max_visited_states * 2));
  return options;
}

DialecticOptions broad_options(DialecticOptions options) {
  options.max_hops = std::min<uint32_t>(16, options.max_hops + 1);
  options.max_paths =
      std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
  options.max_paths_per_root =
      std::min<size_t>(64, std::max<size_t>(2, options.max_paths_per_root * 2));
  options.max_visited_states =
      std::min<size_t>(200000,
                       std::max<size_t>(2, options.max_visited_states * 2));
  options.semantic_candidates =
      std::min<size_t>(64, options.semantic_candidates + 4);
  return options;
}

std::string policy_name(Policy policy) {
  switch (policy) {
    case Policy::NoCycle: return "no_cycle";
    case Policy::TargetedCurrentStop: return "targeted_current_stop";
    case Policy::TargetedFrontierAware: return "targeted_frontier_aware";
    case Policy::TargetedForced: return "targeted_forced_3";
    case Policy::BroadForced: return "broad_forced_3";
  }
  return "unknown";
}

struct BuiltCase {
  uint32_t expected_root{0};
  uint32_t initial_root{0};
  uint32_t symptom{0};
  std::vector<float> query;
  uint64_t signature{0};
};

BuiltCase build_case(GrapheneDB& db, const Scenario& scenario, uint32_t seed) {
  constexpr size_t dimension = 16;
  const uint64_t signature = signature_for(seed % 31, (seed + 7) % 31);
  const std::vector<float> query = unit_vector(dimension, 0);

  // Noise roots are inserted first so equal-confidence ties favour them.
  if (scenario.noise_trap) {
    for (uint32_t index = 0; index < 4; ++index) {
      uint32_t noise_root = 0;
      uint32_t noise_anchor = 0;
      require(db.put_node(
                  make_node("noise root " + std::to_string(index),
                            unit_vector(dimension, 8 + index), signature, true),
                  &noise_root),
              "put noise root");
      require(db.put_node(
                  make_node("noise anchor " + std::to_string(index),
                            near_query(dimension, 0.995f - 0.0005f * index,
                                       4 + index),
                            signature),
                  &noise_anchor),
              "put noise anchor");
      require(db.put_edge(
                  {noise_root, noise_anchor, EdgeOrigin::Observed,
                   EdgeRole::Causal, 1.0,
                   {{"source_id", "noise-source-" + std::to_string(index)}}}),
              "put noise edge");
    }
  }

  // High-similarity dead candidates force broad retrieval to spend its candidate
  // budget before it reaches low-similarity bridge nodes.
  const uint32_t dead_candidates = scenario.chain_hops >= 3 ? 8 : 0;
  for (uint32_t index = 0; index < dead_candidates; ++index) {
    uint32_t ignored = 0;
    require(db.put_node(
                make_node("dead candidate " + std::to_string(index),
                          near_query(dimension, 0.97f - 0.001f * index,
                                     4 + (index % 8)),
                          signature),
                &ignored),
            "put dead candidate");
  }

  uint32_t correct_root = 0;
  require(db.put_node(
              make_node("correct root", unit_vector(dimension, 2), signature,
                        true),
              &correct_root),
          "put correct root");

  uint32_t wrong_root = 0;
  require(db.put_node(
              make_node("initial root", unit_vector(dimension, 3), signature,
                        true),
              &wrong_root),
          "put initial root");

  uint32_t symptom = 0;
  require(db.put_node(
              make_node("query symptom", query, signature, false, true),
              &symptom),
