#include "graphene/fiber_bundle.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

namespace graphene {
namespace {

uint64_t fnv1a_append(uint64_t value, const std::string& text) {
  uint64_t hash = value;
  for (unsigned char ch : text) {
    hash ^= ch;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::vector<std::string> source_lineage(const DialecticPath& path) {
  std::set<std::string> sources;
  for (const auto& evidence : path.evidence) {
    if (!evidence.source_id.empty()) sources.insert(evidence.source_id);
  }
  return {sources.begin(), sources.end()};
}

std::string lineage_key(const FiberPath& path) {
  std::ostringstream output;
  for (const auto& source : path.source_lineage) output << source << '\x1f';
  output << '|';
  for (uint32_t edge : path.edges) output << edge << ',';
  return output.str();
}

uint64_t path_hash(const FiberPath& path) {
  uint64_t hash = 1469598103934665603ULL;
  hash = fnv1a_append(hash, std::to_string(path.target_node));
  hash = fnv1a_append(hash, ":");
  hash = fnv1a_append(hash, std::to_string(path.anchor_node));
  for (uint32_t node : path.nodes) hash = fnv1a_append(hash, "n" + std::to_string(node));
  for (uint32_t edge : path.edges) hash = fnv1a_append(hash, "e" + std::to_string(edge));
  for (const auto& source : path.source_lineage) hash = fnv1a_append(hash, "s" + source);
  return hash;
}

}  // namespace

FiberBundle FiberBundleBuilder::build(const BundleSet& bundles) const {
  FiberBundle output;
  output.snapshot_version = bundles.snapshot_version;
  output.warnings = bundles.warnings;
  output.visited_states = bundles.visited_states;
  output.truncated = bundles.truncated;

  std::vector<RootBundle> roots = bundles.roots;
  std::sort(roots.begin(), roots.end(), [](const RootBundle& left, const RootBundle& right) {
    return left.root_node < right.root_node;
  });

  for (const auto& root : roots) {
    TargetFiber fiber;
    fiber.target_node = root.root_node;
    fiber.raw_path_count = root.paths.size();
    std::set<std::string> seen_exact;
    std::set<std::string> independent_lineages;

    for (const auto& path : root.paths) {
      FiberPath converted;
      converted.target_node = root.root_node;
      converted.anchor_node = path.anchor_node;
      converted.nodes = path.nodes;
      converted.edges = path.edges;
      converted.evidence = path.evidence;
      converted.source_lineage = source_lineage(path);
      converted.confidence = std::clamp(path.score, 0.0, 1.0);
      converted.temporal_consistency = path.temporal_consistent ? 1.0 : 0.0;
      converted.provenance_quality = path.edges.empty()
                                         ? 1.0
                                         : std::clamp(1.0 - static_cast<double>(path.provenance_findings.size()) /
                                                               static_cast<double>(path.edges.size()),
                                                      0.0, 1.0);
      converted.contains_contradiction = path.contains_contradiction;
      converted.contains_hypothetical = path.contains_hypothetical;
      converted.id = path_hash(converted);

      std::ostringstream exact;
      exact << converted.target_node << '|';
      for (uint32_t edge : converted.edges) exact << edge << ',';
      exact << '|';
      for (const auto& source : converted.source_lineage) exact << source << ',';
      if (!seen_exact.insert(exact.str()).second) continue;

      if (!converted.source_lineage.empty()) independent_lineages.insert(lineage_key(converted));
      fiber.paths.push_back(std::move(converted));
    }

    std::sort(fiber.paths.begin(), fiber.paths.end(), [](const FiberPath& left, const FiberPath& right) {
      if (left.id != right.id) return left.id < right.id;
      return left.edges < right.edges;
    });
    fiber.independent_path_count = independent_lineages.size();
    fiber.degeneracy = 1.0 - std::exp(-0.7 * static_cast<double>(fiber.independent_path_count));
    output.fibers.push_back(std::move(fiber));
  }

  output.immutable_hash = hash(output);
  return output;
}

uint64_t FiberBundleBuilder::hash(const FiberBundle& bundle) {
  uint64_t hash = 1469598103934665603ULL;
  hash = fnv1a_append(hash, std::to_string(bundle.snapshot_version));
  hash = fnv1a_append(hash, bundle.truncated ? "1" : "0");
  for (const auto& fiber : bundle.fibers) {
    hash = fnv1a_append(hash, "t" + std::to_string(fiber.target_node));
    hash = fnv1a_append(hash, "i" + std::to_string(fiber.independent_path_count));
    for (const auto& path : fiber.paths) hash = fnv1a_append(hash, "p" + std::to_string(path.id));
  }
  for (const auto& warning : bundle.warnings) hash = fnv1a_append(hash, "w" + warning);
  return hash;
}

}  // namespace graphene
