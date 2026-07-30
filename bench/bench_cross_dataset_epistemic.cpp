#include "graphene/fiber_bundle.hpp"
#include "graphene/stability_critic.hpp"

#include <algorithm>
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
  std::string dataset;
  std::string id;
  std::string category;
  std::string subtype;
  size_t hops{0};
  size_t source_count{0};
  size_t evidence_count{0};
  size_t distractor_count{0};
  bool temporal{false};
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
                        FiberPathRole role,
                        bool temporal_consistent,
                        bool verified,
                        const std::string& source_namespace) {
  DialecticPath path;
  path.root_node = 1;
  path.nodes.push_back(path.root_node);
  for (size_t index = 0; index < std::max<size_t>(1, edge_count); ++index) {
    path.nodes.push_back(static_cast<uint32_t>(101 + index));
    path.edges.push_back(edge_offset + static_cast<uint32_t>(index));
  }
  path.anchor_node = path.nodes.back();
  path.score = confidence;
  path.temporal_consistent = temporal_consistent;
  path.query_relevance = 1.0;
  path.target_consistency = 1.0;
  path.completeness = std::clamp(
      static_cast<double>(edge_count) /
          static_cast<double>(std::max<size_t>(1, example.hops)),
      0.0, 1.0);

  if (role == FiberPathRole::Opposition) {
    path.role_hint = PathRoleHint::Opposition;
    path.contains_contradiction = true;
    path.semantic_verification = SemanticVerificationStatus::Contradicted;
  } else if (role == FiberPathRole::Noise) {
    path.role_hint = PathRoleHint::Noise;
    path.query_relevance = 0.0;
    path.target_consistency = 0.0;
  } else {
    path.role_hint = PathRoleHint::Support;
    path.semantic_verification = verified
        ? SemanticVerificationStatus::Verified
        : SemanticVerificationStatus::Unverified;
  }

  for (size_t index = 0; index < source_count; ++index) {
    path.evidence.push_back({source_namespace + ":source:" + std::to_string(index),
                             example.dataset + ":evidence", "",
                             source_namespace + ":family:" + std::to_string(index),
                             source_namespace + ":derivation:" + std::to_string(index),
                             source_namespace + ":hash:" + std::to_string(index)});
  }
  return path;
}

FiberBundle make_support_bundle(const Example& example,
                                const std::string& condition) {
  BundleSet set;
  set.snapshot_version = 1;
  RootBundle root;
  root.root_node = 1;
  const uint32_t base = 1000U + stable_seed(example.dataset + ":" + example.id) * 32U;
  const size_t hops = std::max<size_t>(1, example.hops);
  const size_t sources = std::max<size_t>(1, example.source_count);

  DialecticPath gold = make_path(example, base, hops, sources, 0.95,
                                 FiberPathRole::Support, true, true,
                                 "gold:" + example.id);
  if (condition == "gold") {
    root.paths.push_back(std::move(gold));
  } else if (condition == "missing") {
    DialecticPath missing = make_path(
        example, base, std::max<size_t>(1, hops - 1),
        std::max<size_t>(1, sources - 1), 0.90,
        FiberPathRole::Support, true, true, "gold:" + example.id);
    missing.provenance_findings.push_back(
        {missing.edges.front(), "MISSING_EVIDENCE", "controlled critical-fact removal"});
    root.paths.push_back(std::move(missing));
    set.truncated = true;
  } else if (condition == "duplicate") {
    root.paths.push_back(gold);
    DialecticPath duplicate = make_path(
        example, base + 8000U, hops, sources, 0.90,
        FiberPathRole::Support, true, true, "gold:" + example.id);
    root.paths.push_back(std::move(duplicate));
  } else if (condition == "distractor") {
    root.paths.push_back(gold);
    DialecticPath distractor = make_path(
        example, base + 12000U, std::min<size_t>(hops + 1, 4), 1, 0.55,
        FiberPathRole::Noise, true, false, "noise:" + example.id);
    root.paths.push_back(std::move(distractor));
  } else if (condition == "contradiction") {
    root.paths.push_back(gold);
    DialecticPath opposition = make_path(
        example, base + 16000U, hops, sources, 0.90,
        FiberPathRole::Opposition, true, false, "opposition:" + example.id);
    root.paths.push_back(std::move(opposition));
  } else if (condition == "independent") {
    root.paths.push_back(gold);
    DialecticPath independent = make_path(
        example, base + 20000U, hops, sources, 0.90,
        FiberPathRole::Support, true, true, "independent:" + example.id);
    root.paths.push_back(std::move(independent));
  } else if (condition == "temporal_invalid") {
    root.paths.push_back(make_path(
        example, base, hops, sources, 0.95, FiberPathRole::Support,
        false, true, "gold:" + example.id));
  } else {
    throw std::invalid_argument("unknown support condition: " + condition);
  }
  set.roots.push_back(std::move(root));
  return FiberBundleBuilder().build(set);
}

FiberBundle make_refute_bundle(const Example& example, bool with_opposition) {
  BundleSet set;
  RootBundle root;
  root.root_node = 1;
  const uint32_t base = 5000U + stable_seed(example.dataset + ":" + example.id) * 32U;
  const size_t hops = std::max<size_t>(1, example.hops);
  const size_t sources = std::max<size_t>(1, example.source_count);
  root.paths.push_back(make_path(example, base, hops, sources, 0.70,
                                 FiberPathRole::Support, true, false,
                                 "candidate:" + example.id));
  if (with_opposition) {
    root.paths.push_back(make_path(example, base + 9000U, hops, sources, 0.95,
                                   FiberPathRole::Opposition, true, false,
                                   "refuting:" + example.id));
  }
  set.roots.push_back(std::move(root));
  return FiberBundleBuilder().build(set);
}

FiberBundle make_nei_bundle(const Example& example) {
  BundleSet set;
  set.truncated = true;
  RootBundle root;
  root.root_node = 1;
  const uint32_t base = 9000U + stable_seed(example.dataset + ":" + example.id) * 32U;
  DialecticPath path = make_path(example, base, 1, 0, 0.50,
                                 FiberPathRole::Support, true, false,
                                 "nei:" + example.id);
  path.evidence.clear();
  path.provenance_findings.push_back(
      {path.edges.front(), "MISSING_EVIDENCE", "benchmark label is insufficient evidence"});
  root.paths.push_back(std::move(path));
  set.roots.push_back(std::move(root));
  return FiberBundleBuilder().build(set);
}

std::vector<Example> load_examples(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open normalized benchmark TSV: " + path);
  std::vector<Example> examples;
  std::string line;
  bool first = true;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    if (first) {
      first = false;
      if (line.rfind("dataset\t", 0) == 0) continue;
    }
    const auto fields = split_tab(line);
    if (fields.size() != 9) throw std::runtime_error("expected nine TSV fields");
    Example example;
    example.dataset = fields[0];
    example.id = fields[1];
    example.category = fields[2];
    example.subtype = fields[3];
    example.hops = std::stoull(fields[4]);
    example.source_count = std::stoull(fields[5]);
    example.evidence_count = std::stoull(fields[6]);
    example.distractor_count = std::stoull(fields[7]);
    example.temporal = fields[8] == "1";
    examples.push_back(std::move(example));
  }
  return examples;
}

size_t independent_count(const FiberBundle& bundle) {
  return bundle.fibers.empty() ? 0 :
         bundle.fibers.front().independent_evidence_family_count;
}

void boolean(std::ostream& out, bool value) { out << (value ? 1 : 0); }

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: bench_cross_dataset_epistemic <normalized.tsv> <results.csv>\n";
    return 2;
  }
  try {
    const auto examples = load_examples(argv[1]);
    if (examples.empty()) throw std::runtime_error("no normalized examples loaded");
    std::ofstream output(argv[2]);
    if (!output) throw std::runtime_error("cannot open results CSV");
    output << std::setprecision(17);
    output << "dataset,id,category,subtype,hops,sources,evidence,distractors,temporal,"
              "gold_energy,missing_energy,duplicate_energy,distractor_energy,contradiction_energy,"
              "independent_energy,temporal_invalid_energy,gold_score,independent_score,"
              "gold_independent,duplicate_independent,independent_independent,"
              "contradiction_blocks,refute_blocks,nei_not_stable,nei_repair_required,"
              "gold_stable,missing_stable,distractor_stable,temporal_invalid_stable\n";

    LyapunovCritic critic;
    for (const auto& example : examples) {
      double gold_energy = -1.0, missing_energy = -1.0, duplicate_energy = -1.0;
      double distractor_energy = -1.0, contradiction_energy = -1.0;
      double independent_energy = -1.0, temporal_invalid_energy = -1.0;
      double gold_score = -1.0, independent_score = -1.0;
      size_t gold_independent = 0, duplicate_independent = 0, independent_independent = 0;
      bool contradiction_blocks = false, refute_blocks = false;
      bool nei_not_stable = false, nei_repair_required = false;
      bool gold_stable = false, missing_stable = false;
      bool distractor_stable = false, temporal_invalid_stable = false;

      if (example.category == "SUPPORT") {
        const FiberBundle gold_bundle = make_support_bundle(example, "gold");
        const FiberBundle missing_bundle = make_support_bundle(example, "missing");
        const FiberBundle duplicate_bundle = make_support_bundle(example, "duplicate");
        const FiberBundle distractor_bundle = make_support_bundle(example, "distractor");
        const FiberBundle contradiction_bundle = make_support_bundle(example, "contradiction");
        const FiberBundle independent_bundle = make_support_bundle(example, "independent");
        const FiberBundle temporal_bundle = make_support_bundle(example, "temporal_invalid");
        const auto gold = critic.assess(gold_bundle, QueryMode::Empirical);
        const auto missing = critic.assess(missing_bundle, QueryMode::Empirical);
        const auto duplicate = critic.assess(duplicate_bundle, QueryMode::Empirical);
        const auto distractor = critic.assess(distractor_bundle, QueryMode::Empirical);
        const auto contradiction = critic.assess(contradiction_bundle, QueryMode::Empirical);
        const auto independent = critic.assess(independent_bundle, QueryMode::Empirical);
        const auto temporal = critic.assess(temporal_bundle, QueryMode::Empirical);
        gold_energy = gold.lyapunov_energy;
        missing_energy = missing.lyapunov_energy;
        duplicate_energy = duplicate.lyapunov_energy;
        distractor_energy = distractor.lyapunov_energy;
        contradiction_energy = contradiction.lyapunov_energy;
        independent_energy = independent.lyapunov_energy;
        temporal_invalid_energy = temporal.lyapunov_energy;
        gold_score = gold.total_score;
        independent_score = independent.total_score;
        gold_independent = independent_count(gold_bundle);
        duplicate_independent = independent_count(duplicate_bundle);
        independent_independent = independent_count(independent_bundle);
        contradiction_blocks = contradiction.contradiction_blocks_resolution;
        gold_stable = gold.stable;
        missing_stable = missing.stable;
        distractor_stable = distractor.stable;
        temporal_invalid_stable = temporal.stable;
      } else if (example.category == "REFUTE") {
        const auto candidate = critic.assess(make_refute_bundle(example, false), QueryMode::Empirical);
        const auto refuted = critic.assess(make_refute_bundle(example, true), QueryMode::Empirical);
        gold_energy = candidate.lyapunov_energy;
        contradiction_energy = refuted.lyapunov_energy;
        gold_score = candidate.total_score;
        contradiction_blocks = refuted.contradiction_blocks_resolution;
        refute_blocks = refuted.contradiction_blocks_resolution && !refuted.stable;
      } else if (example.category == "NEI") {
        const auto nei = critic.assess(make_nei_bundle(example), QueryMode::Empirical);
        gold_energy = nei.lyapunov_energy;
        gold_score = nei.total_score;
        nei_not_stable = !nei.stable;
        nei_repair_required = nei.requires_escape || nei.requires_abstention;
      } else {
        throw std::runtime_error("unknown category: " + example.category);
      }

      output << example.dataset << ',' << example.id << ',' << example.category << ','
             << example.subtype << ',' << example.hops << ',' << example.source_count << ','
             << example.evidence_count << ',' << example.distractor_count << ','
             << (example.temporal ? 1 : 0) << ',' << gold_energy << ',' << missing_energy << ','
             << duplicate_energy << ',' << distractor_energy << ',' << contradiction_energy << ','
             << independent_energy << ',' << temporal_invalid_energy << ',' << gold_score << ','
             << independent_score << ',' << gold_independent << ',' << duplicate_independent << ','
             << independent_independent << ',';
      boolean(output, contradiction_blocks); output << ',';
      boolean(output, refute_blocks); output << ',';
      boolean(output, nei_not_stable); output << ',';
      boolean(output, nei_repair_required); output << ',';
      boolean(output, gold_stable); output << ',';
      boolean(output, missing_stable); output << ',';
      boolean(output, distractor_stable); output << ',';
      boolean(output, temporal_invalid_stable); output << '\n';
    }
    std::cout << "cross_dataset_examples=" << examples.size() << "\n";
    std::cout << "critic=graphene::LyapunovCritic\n";
    std::cout << "results=" << argv[2] << "\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "benchmark_error=" << error.what() << "\n";
    return 1;
  }
}
