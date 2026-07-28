#include "graphene/fiber_bundle.hpp"
#include "graphene/stability_critic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace graphene;

namespace {

struct Example {
  std::string id;
  std::string type;
  size_t hops{0};
  size_t source_count{0};
  size_t evidence_count{0};
  size_t distractor_count{0};
};

std::vector<std::string> split_tab(const std::string& line) {
  std::vector<std::string> fields;
  std::stringstream input(line);
  std::string field;
  while (std::getline(input, field, '\t')) fields.push_back(field);
  return fields;
}

uint32_t stable_seed(const std::string& text) {
  uint32_t hash = 2166136261u;
  for (unsigned char ch : text) {
    hash ^= ch;
    hash *= 16777619u;
  }
  return hash & 0x0fffffffU;
}

DialecticPath make_path(const Example& example,
                        uint32_t edge_offset,
                        size_t edge_count,
                        size_t source_count,
                        double confidence,
                        bool contradiction,
                        bool temporal_consistent,
                        size_t provenance_findings,
                        const std::string& source_namespace) {
  (void)example;
  DialecticPath path;
  path.root_node = 1;
  path.anchor_node = 100;
  path.nodes.push_back(1);
  for (size_t index = 0; index < edge_count; ++index) {
    path.nodes.push_back(static_cast<uint32_t>(101 + index));
    path.edges.push_back(edge_offset + static_cast<uint32_t>(index));
  }
  path.score = confidence;
  path.contains_contradiction = contradiction;
  path.temporal_consistent = temporal_consistent;
  const size_t bounded_sources = std::max<size_t>(1, source_count);
  for (size_t index = 0; index < bounded_sources; ++index) {
    path.evidence.push_back({source_namespace + ":" + std::to_string(index),
                             "2wiki-supporting-fact", ""});
  }
  for (size_t index = 0; index < provenance_findings; ++index) {
    const uint32_t edge = path.edges.empty() ? 0 : path.edges[index % path.edges.size()];
    path.provenance_findings.push_back(
        {edge, "missing_gold_hop", "controlled 2Wiki evidence corruption"});
  }
  return path;
}

FiberBundle build_bundle(const Example& example,
                         const std::string& condition) {
  const uint32_t base = 1000U + stable_seed(example.id) * 16U;
  BundleSet set;
  set.snapshot_version = 1;
  RootBundle root;
  root.root_node = 1;

  const size_t hops = std::max<size_t>(2, example.hops);
  const size_t sources = std::max<size_t>(1, example.source_count);
  const auto gold = make_path(example, base, hops, sources, 0.95, false, true,
                              0, "gold:" + example.id);

  if (condition == "gold") {
    root.paths.push_back(gold);
  } else if (condition == "missing_hop") {
    const size_t retained_hops = std::max<size_t>(1, hops - 1);
    const size_t retained_sources = std::max<size_t>(1, sources - 1);
    root.paths.push_back(make_path(example, base, retained_hops,
                                   retained_sources, 0.90, false, true, 1,
                                   "gold:" + example.id));
    set.truncated = true;
    set.warnings.push_back("CONTROLLED_MISSING_GOLD_HOP");
  } else if (condition == "contradiction") {
    root.paths.push_back(gold);
    root.paths.push_back(make_path(example, base + 4000U, hops, sources,
                                   0.85, true, true, 0,
                                   "contradiction:" + example.id));
  } else if (condition == "same_source_duplicate") {
    root.paths.push_back(gold);
    // Different edge sequence but exactly the same evidence source IDs. A
    // source-independent critic must not reward this as corroboration.
    root.paths.push_back(make_path(example, base + 8000U, hops, sources,
                                   0.90, false, true, 0,
                                   "gold:" + example.id));
  } else if (condition == "distractor") {
    root.paths.push_back(gold);
    const size_t distractor_hops = std::max<size_t>(2, std::min<size_t>(hops, 4));
    root.paths.push_back(make_path(example, base + 12000U,
                                   distractor_hops, 1, 0.55, false, true, 0,
                                   "irrelevant-context:" + example.id));
  } else if (condition == "wrong_complete") {
    // Structurally perfect and fully sourced, but deliberately unrelated to
    // the gold evidence. This demonstrates the critic's truth-oracle boundary.
    root.paths.push_back(make_path(example, base + 16000U, hops, sources,
                                   0.95, false, true, 0,
                                   "wrong-answer:" + example.id));
  } else {
    throw std::invalid_argument("unknown condition: " + condition);
  }

  set.roots.push_back(std::move(root));
  return FiberBundleBuilder().build(set);
}

std::vector<Example> load_examples(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open input TSV: " + path);
  std::vector<Example> examples;
  std::string line;
  bool first = true;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    if (first) {
      first = false;
      if (line.rfind("id\t", 0) == 0) continue;
    }
    const auto fields = split_tab(line);
    if (fields.size() != 6) {
      throw std::runtime_error("expected six TSV fields");
    }
    Example example;
    example.id = fields[0];
    example.type = fields[1];
    example.hops = std::stoull(fields[2]);
    example.source_count = std::stoull(fields[3]);
    example.evidence_count = std::stoull(fields[4]);
    example.distractor_count = std::stoull(fields[5]);
    examples.push_back(std::move(example));
  }
  return examples;
}

struct Result {
  FiberBundle bundle;
  StabilityAssessment assessment;
};

Result evaluate(const Example& example,
                const std::string& condition,
                const LyapunovCritic& critic) {
  Result result;
  result.bundle = build_bundle(example, condition);
  result.assessment = critic.assess(result.bundle, QueryMode::Empirical);
  return result;
}

void write_bool(std::ostream& out, bool value) { out << (value ? 1 : 0); }

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: bench_2wiki_lyapunov <2wiki.tsv> <results.csv>\n";
    return 2;
  }

  try {
    const auto examples = load_examples(argv[1]);
    if (examples.empty()) throw std::runtime_error("no 2Wiki examples loaded");
    std::ofstream output(argv[2]);
    if (!output) throw std::runtime_error("cannot open result CSV");
    output << std::setprecision(17);
    output << "id,type,hops,source_count,evidence_count,distractor_count,"
              "gold_energy,missing_energy,contradiction_energy,duplicate_energy,"
              "distractor_energy,wrong_energy,gold_score,missing_score,"
              "contradiction_score,duplicate_score,distractor_score,wrong_score,"
              "gold_stable,missing_stable,contradiction_stable,duplicate_stable,"
              "distractor_stable,wrong_stable,gold_independent,duplicate_independent,"
              "gold_pattern_lock,duplicate_pattern_lock,distractor_pattern_lock,"
              "repair_monotonic,repair_goal_reached,repair_convergence\n";

    LyapunovCritic critic;
    for (const auto& example : examples) {
      const auto gold = evaluate(example, "gold", critic);
      const auto missing = evaluate(example, "missing_hop", critic);
      const auto contradiction = evaluate(example, "contradiction", critic);
      const auto duplicate = evaluate(example, "same_source_duplicate", critic);
      const auto distractor = evaluate(example, "distractor", critic);
      const auto wrong = evaluate(example, "wrong_complete", critic);

      const LyapunovTrajectory repair = critic.analyse(
          {{missing.bundle.immutable_hash, missing.assessment},
           {gold.bundle.immutable_hash, gold.assessment},
           {gold.bundle.immutable_hash, gold.assessment}},
          QueryMode::Empirical);

      const auto independent = [](const FiberBundle& bundle) -> size_t {
        return bundle.fibers.empty() ? 0 :
               bundle.fibers.front().independent_path_count;
      };

      output << example.id << ',' << example.type << ',' << example.hops << ','
             << example.source_count << ',' << example.evidence_count << ','
             << example.distractor_count << ','
             << gold.assessment.lyapunov_energy << ','
             << missing.assessment.lyapunov_energy << ','
             << contradiction.assessment.lyapunov_energy << ','
             << duplicate.assessment.lyapunov_energy << ','
             << distractor.assessment.lyapunov_energy << ','
             << wrong.assessment.lyapunov_energy << ','
             << gold.assessment.total_score << ','
             << missing.assessment.total_score << ','
             << contradiction.assessment.total_score << ','
             << duplicate.assessment.total_score << ','
             << distractor.assessment.total_score << ','
             << wrong.assessment.total_score << ',';
      write_bool(output, gold.assessment.stable); output << ',';
      write_bool(output, missing.assessment.stable); output << ',';
      write_bool(output, contradiction.assessment.stable); output << ',';
      write_bool(output, duplicate.assessment.stable); output << ',';
      write_bool(output, distractor.assessment.stable); output << ',';
      write_bool(output, wrong.assessment.stable); output << ',';
      output << independent(gold.bundle) << ',' << independent(duplicate.bundle) << ','
             << gold.assessment.pattern_lock_score << ','
             << duplicate.assessment.pattern_lock_score << ','
             << distractor.assessment.pattern_lock_score << ',';
      write_bool(output, repair.certificate.monotonic_nonincreasing); output << ',';
      write_bool(output, repair.certificate.goal_reached); output << ',';
      write_bool(output, repair.certificate.convergence_observed); output << '\n';
    }

    std::cout << "2wiki_examples=" << examples.size() << "\n";
    std::cout << "critic=graphene::LyapunovCritic\n";
    std::cout << "mode=empirical\n";
    std::cout << "results=" << argv[2] << "\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "benchmark_error=" << error.what() << "\n";
    return 1;
  }
}
