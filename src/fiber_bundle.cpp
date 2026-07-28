#include "graphene/fiber_bundle.hpp"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <unordered_set>

namespace graphene {
namespace {

uint64_t fnv1a_bytes(uint64_t hash, const void* data, size_t size) {
  const auto* bytes = static_cast<const unsigned char*>(data);
  for (size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

template <typename T>
uint64_t hash_value(uint64_t hash, const T& value) {
  return fnv1a_bytes(hash, &value, sizeof(T));
}

uint64_t hash_string(uint64_t hash, const std::string& value) {
  hash = hash_value(hash, value.size());
  return fnv1a_bytes(hash, value.data(), value.size());
}

std::string evidence_key(const EvidenceRef& evidence) {
  return evidence.source_id + "\x1f" + evidence.span + "\x1f" +
         evidence.observed_at;
}

std::vector<std::string> source_lineage(const DialecticPath& path) {
  std::set<std::string> sources;
  for (const auto& evidence : path.evidence) {
    if (!evidence.source_id.empty()) sources.insert(evidence.source_id);
  }
  // A path with no evidence lineage is not treated as independent evidence.
  return {sources.begin(), sources.end()};
}

double jaccard_overlap(const std::vector<std::string>& left,
                       const std::vector<std::string>& right) {
  if (left.empty() || right.empty()) return 1.0;
  std::set<std::string> a(left.begin(), left.end());
  std::set<std::string> b(right.begin(), right.end());
  size_t intersection = 0;
  for (const auto& value : a) {
    if (b.count(value) != 0) ++intersection;
  }
  const size_t union_size = a.size() + b.size() - intersection;
  return union_size == 0
             ? 1.0
             : static_cast<double>(intersection) /
                   static_cast<double>(union_size);
}

double edge_distance(const FiberPath& left, const FiberPath& right) {
  std::set<uint32_t> a(left.edges.begin(), left.edges.end());
  std::set<uint32_t> b(right.edges.begin(), right.edges.end());
  if (a.empty() && b.empty()) return 0.0;
  size_t intersection = 0;
  for (uint32_t edge : a) {
    if (b.count(edge) != 0) ++intersection;
  }
  const size_t union_size = a.size() + b.size() - intersection;
  return union_size == 0
             ? 0.0
             : 1.0 - static_cast<double>(intersection) /
                         static_cast<double>(union_size);
}

bool path_less(const FiberPath& left, const FiberPath& right) {
  if (left.root_node != right.root_node) return left.root_node < right.root_node;
  if (left.edges != right.edges) return left.edges < right.edges;
  if (left.nodes != right.nodes) return left.nodes < right.nodes;
  return left.anchor_node < right.anchor_node;
}

}  // namespace

FiberBundleBuilder::FiberBundleBuilder(const GrapheneDB& db) : db_(db) {}

FiberBundle FiberBundleBuilder::from_bundle_set(
    const BundleSet& legacy,
    uint64_t query_id,
    const FiberBundleBuildOptions& requested_options) const {
  FiberBundleBuildOptions options = requested_options;
  options.max_source_overlap_for_independence = std::clamp(
      options.max_source_overlap_for_independence, 0.0, 1.0);

  FiberBundle output;
  output.query_id = query_id;
  output.snapshot_version = legacy.snapshot_version;
  output.semantic_candidates = legacy.semantic_candidates;
  output.visited_states = legacy.visited_states;
  output.truncated = legacy.truncated;
  output.warnings = legacy.warnings;

  for (const RootBundle& root : legacy.roots) {
    TargetFiber fiber;
    fiber.target_node = root.root_node;

    std::set<std::vector<uint32_t>> exact_paths;
    for (const DialecticPath& path : root.paths) {
      // Duplicate semantic paths are collapsed before any degeneracy metric.
      if (!exact_paths.insert(path.edges).second) continue;

      FiberPath converted;
      converted.root_node = path.root_node;
      converted.anchor_node = path.anchor_node;
      converted.nodes = path.nodes;
      converted.edges = path.edges;
      converted.score = std::clamp(path.score, 0.0, 1.0);
      converted.temporal_consistency = path.temporal_consistent ? 1.0 : 0.0;
      converted.contains_contradiction = path.contains_contradiction;
      converted.contains_hypothetical = path.contains_hypothetical;
      converted.evidence = path.evidence;
      converted.provenance_findings = path.provenance_findings;
      converted.source_lineage = source_lineage(path);

      size_t inferred = 0;
      size_t analogical = 0;
      for (uint32_t edge_id : converted.edges) {
        const auto edge = db_.get_edge(edge_id, legacy.snapshot_version);
        if (!edge) continue;
        inferred += edge->origin == EdgeOrigin::Inferred ? 1 : 0;
        analogical += edge->role == EdgeRole::Analogical ? 1 : 0;
      }
      converted.contains_inferred = inferred > 0;
      converted.contains_analogical = analogical > 0;

      const size_t edge_count = converted.edges.size();
      converted.provenance_quality =
          edge_count == 0
              ? 0.0
              : std::clamp(
                    static_cast<double>(converted.evidence.size()) /
                            static_cast<double>(edge_count) -
                        0.25 * static_cast<double>(converted.provenance_findings.size()) /
                            static_cast<double>(edge_count),
                    0.0, 1.0);
      fiber.paths.push_back(std::move(converted));
    }

    std::sort(fiber.paths.begin(), fiber.paths.end(), path_less);
    for (size_t index = 0; index < fiber.paths.size(); ++index) {
      // Stable path IDs are derived from immutable path content.
      FiberBundle singleton;
      singleton.query_id = query_id;
      singleton.snapshot_version = legacy.snapshot_version;
      TargetFiber temp;
      temp.target_node = root.root_node;
      temp.paths.push_back(fiber.paths[index]);
      singleton.fibers.push_back(std::move(temp));
      fiber.paths[index].id = compute_hash(singleton);
    }

    fiber.raw_path_count = fiber.paths.size();
    std::vector<size_t> independent_representatives;
    for (size_t index = 0; index < fiber.paths.size(); ++index) {
      bool independent = !fiber.paths[index].source_lineage.empty();
      for (size_t representative : independent_representatives) {
        if (jaccard_overlap(fiber.paths[index].source_lineage,
                            fiber.paths[representative].source_lineage) >
            options.max_source_overlap_for_independence) {
          independent = false;
          break;
        }
      }
      if (independent) independent_representatives.push_back(index);
    }
    fiber.independent_path_count = independent_representatives.size();
    fiber.degeneracy = static_cast<double>(fiber.independent_path_count);

    double diversity_sum = 0.0;
    size_t diversity_pairs = 0;
    for (size_t i = 0; i < fiber.paths.size(); ++i) {
      for (size_t j = i + 1; j < fiber.paths.size(); ++j) {
        diversity_sum += edge_distance(fiber.paths[i], fiber.paths[j]);
        ++diversity_pairs;
      }
    }
    fiber.path_diversity = diversity_pairs == 0
                               ? 0.0
                               : diversity_sum /
                                     static_cast<double>(diversity_pairs);

    size_t contradictory_paths = 0;
    size_t evidence_complete_paths = 0;
    for (auto& path : fiber.paths) {
      contradictory_paths += path.contains_contradiction ? 1 : 0;
      evidence_complete_paths += path.provenance_quality >= 0.999 ? 1 : 0;
      if (fiber.independent_path_count == 0) {
        path.independence_score = 0.0;
      } else {
        bool unique = !path.source_lineage.empty();
        for (const auto& other : fiber.paths) {
          if (&path == &other || other.source_lineage.empty()) continue;
          if (jaccard_overlap(path.source_lineage, other.source_lineage) >
              options.max_source_overlap_for_independence) {
            unique = false;
            break;
          }
        }
        path.independence_score = unique ? 1.0 : 0.0;
      }
    }
    fiber.contradiction_ratio =
        fiber.paths.empty()
            ? 0.0
            : static_cast<double>(contradictory_paths) /
                  static_cast<double>(fiber.paths.size());
    fiber.evidence_coverage =
        fiber.paths.empty()
            ? 0.0
            : static_cast<double>(evidence_complete_paths) /
                  static_cast<double>(fiber.paths.size());

    if (fiber.paths.empty()) {
      output.missing_evidence.push_back(
          {fiber.target_node, "no valid source-grounded path reached target"});
    } else if (fiber.evidence_coverage < 1.0) {
      output.missing_evidence.push_back(
          {fiber.target_node, "one or more path edges lack complete provenance"});
    }
    output.fibers.push_back(std::move(fiber));
  }

  std::sort(output.fibers.begin(), output.fibers.end(),
            [](const TargetFiber& left, const TargetFiber& right) {
              return left.target_node < right.target_node;
            });
  output.immutable_hash = compute_hash(output);
  return output;
}

uint64_t FiberBundleBuilder::compute_hash(const FiberBundle& bundle) {
  uint64_t hash = 1469598103934665603ULL;
  hash = hash_value(hash, bundle.query_id);
  hash = hash_value(hash, bundle.snapshot_version);
  hash = hash_value(hash, bundle.truncated);
  for (uint32_t candidate : bundle.semantic_candidates) {
    hash = hash_value(hash, candidate);
  }
  for (const auto& fiber : bundle.fibers) {
    hash = hash_value(hash, fiber.target_node);
    for (const auto& path : fiber.paths) {
      hash = hash_value(hash, path.root_node);
      hash = hash_value(hash, path.anchor_node);
      for (uint32_t node : path.nodes) hash = hash_value(hash, node);
      for (uint32_t edge : path.edges) hash = hash_value(hash, edge);
      hash = hash_value(hash, path.score);
      hash = hash_value(hash, path.temporal_consistency);
      hash = hash_value(hash, path.provenance_quality);
      hash = hash_value(hash, path.independence_score);
      hash = hash_value(hash, path.contains_contradiction);
      hash = hash_value(hash, path.contains_hypothetical);
      hash = hash_value(hash, path.contains_inferred);
      hash = hash_value(hash, path.contains_analogical);
      for (const auto& source : path.source_lineage) {
        hash = hash_string(hash, source);
      }
      for (const auto& evidence : path.evidence) {
        hash = hash_string(hash, evidence_key(evidence));
      }
      for (const auto& finding : path.provenance_findings) {
        hash = hash_value(hash, finding.edge_id);
        hash = hash_string(hash, finding.code);
        hash = hash_string(hash, finding.detail);
      }
    }
  }
  for (const auto& missing : bundle.missing_evidence) {
    hash = hash_value(hash, missing.target_node);
    hash = hash_string(hash, missing.requirement);
  }
  return hash;
}

}  // namespace graphene
