    previous_visited = bundle.visited_states;
  }

  result.final_correct = result.observations.back().correct;
  result.corrected = !result.initial_correct && result.final_correct;
  result.degraded = result.initial_correct && !result.final_correct;

  // Retrospective oracle: earliest cycle with a correct answer; when no cycle is
  // correct, cycle zero is the least-cost choice.
  result.oracle_cycle = 0;
  for (const auto& observation : result.observations) {
    if (observation.correct) {
      result.oracle_cycle = observation.cycle;
      break;
    }
  }
  if (result.final_correct && result.cycles_executed > result.oracle_cycle) {
    result.stop_regret_cycles = result.cycles_executed - result.oracle_cycle;
  }

  require(db.close(), "close intervention db");
  fs::remove_all(directory);
  return result;
}

struct Aggregate {
  size_t cases{0};
  size_t initial_correct{0};
  size_t final_correct{0};
  size_t corrected{0};
  size_t degraded{0};
  size_t useful{0};
  size_t cycles{0};
  size_t visited{0};
  size_t stop_regret{0};
};

void add(Aggregate& aggregate, const RunResult& result) {
  ++aggregate.cases;
  aggregate.initial_correct += result.initial_correct;
  aggregate.final_correct += result.final_correct;
  aggregate.corrected += result.corrected;
  aggregate.degraded += result.degraded;
  aggregate.useful += result.useful_new_information;
  aggregate.cycles += result.cycles_executed;
  aggregate.visited += result.total_visited_states;
  aggregate.stop_regret += result.stop_regret_cycles;
}

double ratio(size_t numerator, size_t denominator) {
  return denominator == 0 ? 0.0
                          : static_cast<double>(numerator) /
                                static_cast<double>(denominator);
}

}  // namespace

int main(int argc, char** argv) {
  const uint32_t repetitions =
      argc > 1 ? static_cast<uint32_t>(std::stoul(argv[1])) : 20;
  const std::string output_csv =
      argc > 2 ? argv[2] : "dialectic_intervention_results.csv";
  if (repetitions == 0 || repetitions > 100) {
    std::cerr << "usage: bench_dialectic_intervention [repetitions 1..100] [csv]\n";
    return 2;
  }

  const std::vector<Scenario> scenarios = {
      {"missing_hop_2", EscapeKind::MissingHop, 2, false, false, false, false},
      {"missing_hop_3", EscapeKind::MissingHop, 3, false, false, false, false},
      {"missing_hop_4", EscapeKind::MissingHop, 4, false, false, false, false},
      {"minority_path", EscapeKind::ExpandMinority, 1, true, false, false, false},
      {"independent_candidate", EscapeKind::SeekIndependent, 1, false, true, false, false},
      {"noise_trap", EscapeKind::MissingHop, 2, false, false, true, false},
      {"already_correct", EscapeKind::MissingHop, 1, false, false, false, true},
  };
  const std::vector<Policy> policies = {
      Policy::NoCycle,
      Policy::TargetedCurrentStop,
      Policy::TargetedFrontierAware,
      Policy::TargetedForced,
      Policy::BroadForced,
  };

  std::ofstream csv(output_csv, std::ios::trunc);
  csv << "family,policy,seed,initial_correct,final_correct,corrected,degraded,"
         "useful_new_information,cycles,total_visited,first_correct_cycle,"
         "oracle_cycle,stop_regret_cycles,final_primary,final_confidence,"
         "final_roots,final_paths\n";

  std::map<std::pair<std::string, std::string>, Aggregate> aggregates;
  std::vector<RunResult> all_results;
  const auto start = Clock::now();
  for (const auto& scenario : scenarios) {
    for (uint32_t repetition = 0; repetition < repetitions; ++repetition) {
      const uint32_t seed = 1000 + repetition;
      for (Policy policy : policies) {
        RunResult result = run_case(scenario, seed, policy);
        const auto& final = result.observations.back();
        csv << result.family << ',' << result.policy << ',' << result.seed << ','
            << result.initial_correct << ',' << result.final_correct << ','
            << result.corrected << ',' << result.degraded << ','
            << result.useful_new_information << ',' << result.cycles_executed
            << ',' << result.total_visited_states << ','
            << (result.first_correct_cycle == 999 ? -1
                                                   : static_cast<int>(result.first_correct_cycle))
            << ',' << result.oracle_cycle << ',' << result.stop_regret_cycles
            << ',' << final.primary << ',' << std::setprecision(12)
            << final.confidence << ',' << final.roots << ',' << final.paths
            << '\n';
        add(aggregates[{result.family, result.policy}], result);
        all_results.push_back(std::move(result));
      }
    }
  }
  csv.close();
  const double elapsed_ms =
      std::chrono::duration<double, std::milli>(Clock::now() - start).count();

  std::cout << "dialectic_intervention_benchmark=true\n"
            << "implementation_scope=dialectic_engine_controlled_stop_policy_ablation\n"
            << "repetitions_per_family=" << repetitions << "\n"
            << "total_runs=" << all_results.size() << "\n"
            << "elapsed_ms=" << elapsed_ms << "\n";
  std::cout << "family,policy,initial_accuracy,final_accuracy,correction_rate,"
               "harm_rate,useful_information_rate,mean_cycles,mean_visited,"
               "mean_stop_regret_cycles\n";
  for (const auto& [key, aggregate] : aggregates) {
    std::cout << key.first << ',' << key.second << ','
              << ratio(aggregate.initial_correct, aggregate.cases) << ','
              << ratio(aggregate.final_correct, aggregate.cases) << ','
              << ratio(aggregate.corrected,
                       aggregate.cases - aggregate.initial_correct)
              << ',' << ratio(aggregate.degraded, aggregate.initial_correct)
              << ',' << ratio(aggregate.useful, aggregate.cases) << ','
              << ratio(aggregate.cycles, aggregate.cases) << ','
              << ratio(aggregate.visited, aggregate.cases) << ','
              << ratio(aggregate.stop_regret, aggregate.cases) << '\n';
  }

  return 0;
}
