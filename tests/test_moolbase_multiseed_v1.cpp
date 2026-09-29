#include "graphene/epistemic_control.hpp"
#include "graphene/fiber_bundle.hpp"
#include "graphene/stability_critic.hpp"

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <utility>

using namespace graphene;

namespace {

DialecticPath make_path(uint32_t root,
                        uint32_t edge,
                        std::string source,
                        std::string family,
                        double score,
                        bool contradiction,
                        double relevance) {
  DialecticPath path;
  path.root_node = root;
  path.anchor_node = 999;
  path.nodes = {root, 999};
  path.edges = {edge};
  path.score = score;
  path.query_relevance = relevance;
  path.target_consistency = relevance;
  path.completeness = 1.0;
  path.temporal_consistent = true;
  path.contains_contradiction = contradiction;
  path.evidence.push_back(
      {std::move(source), "span", "", std::move(family), "", ""});
  return path;
}

ConvergedAnswer converge(const FiberBundle& bundle) {
  LyapunovCritic critic;
  EpistemicController controller;
  const auto stability = critic.assess(bundle, QueryMode::Empirical);
  const auto admissibility =
      controller.assess(bundle, stability, QueryMode::Empirical);
  return controller.converge(bundle, admissibility, stability);
}

bool episode(uint64_t seed, size_t index) {
  std::mt19937_64 rng(seed);
  // Advance exactly as if this were the indexed episode in a sequential run.
  for (size_t skipped = 0; skipped < index; ++skipped) {
    (void)(rng() % 3);
    const size_t paths = 1 + static_cast<size_t>(rng() % 8);
    for (size_t i = 0; i < paths; ++i) {
      (void)(rng() % 4);
      (void)(rng() % 5);
      (void)(rng() % 1001);
      (void)(rng() % 7);
      (void)(rng() % 1001);
    }
  }

  const uint32_t target = 1 + static_cast<uint32_t>(rng() % 3);
  RootBundle root;
  root.root_node = target;
  const size_t paths = 1 + static_cast<size_t>(rng() % 8);
  for (size_t i = 0; i < paths; ++i) {
    const std::string family =
        "mut-family-" + std::to_string(rng() % 4);
    const std::string source =
        "mut-source-" + std::to_string(rng() % 5);
    const double score =
        static_cast<double>(rng() % 1001) / 1000.0;
    const bool contradiction = (rng() % 7) == 0;
    const double relevance =
        static_cast<double>(rng() % 1001) / 1000.0;
    root.paths.push_back(make_path(
        target, 200 + static_cast<uint32_t>(i), source, family, score,
        contradiction, relevance));
  }

  BundleSet raw;
  raw.snapshot_version = 20000 + index;
  raw.roots = {root};
  FiberBundleBuilder builder;
  const FiberBundle first = builder.build(raw);
  const FiberBundle repeated = builder.build(raw);
  const auto first_answer = converge(first);
  const auto repeated_answer = converge(repeated);
  return first.immutable_hash == repeated.immutable_hash &&
         first_answer.primary_node == repeated_answer.primary_node &&
         first_answer.has_answer == repeated_answer.has_answer &&
         std::abs(first_answer.confidence - repeated_answer.confidence) <
             1e-12;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) {
    std::cerr << "usage: graphenedb_moolbase_multiseed_v1 "
              << "<seed> <episodes> <report.json>\n";
    return 2;
  }
  const uint64_t seed = std::stoull(argv[1]);
  const size_t episodes = static_cast<size_t>(std::stoull(argv[2]));
  size_t failures = 0;
  for (size_t index = 0; index < episodes; ++index) {
    if (!episode(seed, index)) ++failures;
  }
  std::ofstream out(argv[3]);
  if (!out) return 2;
  out << "{\n"
      << "  \"schema\": \"moolbase-multiseed-v1\",\n"
      << "  \"seed\": " << seed << ",\n"
      << "  \"episodes\": " << episodes << ",\n"
      << "  \"failures\": " << failures << ",\n"
      << "  \"crashes\": 0,\n"
      << "  \"nondeterminism\": " << failures << "\n"
      << "}\n";
  std::cout << "multiseed_seed=" << seed << "\n"
            << "multiseed_episodes=" << episodes << "\n"
            << "multiseed_failures=" << failures << "\n";
  return failures == 0 ? 0 : 1;
}
