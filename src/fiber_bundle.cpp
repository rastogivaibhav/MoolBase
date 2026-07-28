#include "graphene/fiber_bundle.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <unordered_map>

namespace graphene {
namespace {

constexpr double kEligibilityThreshold = 0.50;

uint64_t fnv1a_append(uint64_t value, const std::string& text) {
  uint64_t hash = value;
  for (unsigned char ch : text) {
    hash ^= ch;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string join(const std::vector<std::string>& values, char separator) {
  std::ostringstream output;
  for (size_t index = 0; index < values.size(); ++index) {
    if (index) output << separator;
    output << values[index];
  }
  return output.str();
}

std::vector<std::string> source_lineage(const DialecticPath& path) {
  std::set<std::string> values;
  for (const auto& evidence : path.evidence) {
    if (!evidence.source_id.empty()) values.insert(evidence.source_id);
  }
  return {values.begin(), values.end()};
}

std::vector<std::string> evidence_families(const DialecticPath& path) {
  std::set<std::string> values;
  for (const auto& evidence : path.evidence) {
    if (!evidence.evidence_family_id.empty()) {
      values.insert("family:" + evidence.evidence_family_id);
    } else if (!evidence.content_hash.empty()) {
      values.insert("content:" + evidence.content_hash);
    } else if (!evidence.derivation_id.empty()) {
      values.insert("derivation:" + evidence.derivation_id);
    } else if (!evidence.source_id.empty()) {
      values.insert("source:" + evidence.source_id);
    }
  }
  return {values.begin(), values.end()};
}

std::vector<std::string> derivations(const DialecticPath& path) {
  std::set<std::string> values;
  for (const auto& evidence : path.evidence) {
    if (!evidence.derivation_id.empty()) values.insert(evidence.derivation_id);
  }
  return {values.begin(), values.end()};
}

std::string route_signature(const FiberPath& path) {
  std::ostringstream output;
  output << path.target_node << ':' << path.anchor_node << ':';
  for (uint32_t node : path.nodes) output << 'n' << node << ',';
  output << '|';
  for (uint32_t edge : path.edges) output << 'e' << edge << ',';
  return output.str();
}

uint64_t path_hash(const FiberPath& path) {
  uint64_t hash = 1469598103934665603ULL;
  hash = fnv1a_append(hash, path.route_signature);
  hash = fnv1a_append(hash, path.evidence_lineage_signature);
  hash = fnv1a_append(hash, std::to_string(static_cast<int>(path.role)));
  return hash;
}

FiberPathRole classify(const DialecticPath& path) {
  if (path.role_hint == PathRoleHint::Noise ||
      path.query_relevance < kEligibilityThreshold ||
      path.target_consistency < kEligibilityThreshold) {
    return FiberPathRole::Noise;
  }
  if (path.role_hint == PathRoleHint::Opposition ||
      path.contains_contradiction ||
      path.semantic_verification == SemanticVerificationStatus::Contradicted) {
    return FiberPathRole::Opposition;
  }
  if (path.role_hint == PathRoleHint::Support ||
      path.role_hint == PathRoleHint::Auto) {
    return FiberPathRole::Support;
  }
  return FiberPathRole::Unknown;
}

bool overlaps(const std::vector<std::string>& left,
              const std::vector<std::string>& right) {
  size_t i = 0;
  size_t j = 0;
  while (i < left.size() && j < right.size()) {
    if (left[i] == right[j]) return true;
    if (left[i] < right[j]) ++i;
    else ++j;
  }
  return false;
}

struct DisjointSet {
  explicit DisjointSet(size_t size) : parent(size), rank(size, 0) {
    std::iota(parent.begin(), parent.end(), 0);
  }
  size_t find(size_t value) {
    if (parent[value] != value) parent[value] = find(parent[value]);
    return parent[value];
  }
  void unite(size_t left, size_t right) {
    left = find(left);
    right = find(right);
    if (left == right) return;
    if (rank[left] < rank[right]) std::swap(left, right);
    parent[right] = left;
    if (rank[left] == rank[right]) ++rank[left];
  }
  std::vector<size_t> parent;
  std::vector<uint8_t> rank;
};

double path_quality(const FiberPath& path) {
  return std::clamp(path.confidence * path.query_relevance *
                        path.target_consistency * path.completeness *
                        path.provenance_quality,
                    0.0, 1.0);
}

double jaccard_distance(const std::vector<uint32_t>& left,
                        const std::vector<uint32_t>& right) {
  std::set<uint32_t> a(left.begin(), left.end());
  std::set<uint32_t> b(right.begin(), right.end());
  size_t intersection = 0;
  for (uint32_t value : a) if (b.count(value)) ++intersection;
  const size_t union_size = a.size() + b.size() - intersection;
  return union_size == 0
             ? 0.0
             : 1.0 - static_cast<double>(intersection) /
                         static_cast<double>(union_size);
}

const FiberPath* find_path(const TargetFiber& fiber, uint64_t id) {
  const auto it = std::find_if(
      fiber.paths.begin(), fiber.paths.end(),
      [id](const FiberPath& path) { return path.id == id; });
  return it == fiber.paths.end() ? nullptr : &*it;
}

}  // namespace

const char* fiber_path_role_name(FiberPathRole role) {
  switch (role) {
    case FiberPathRole::Support: return "support";
    case FiberPathRole::Opposition: return "opposition";
    case FiberPathRole::Noise: return "noise";
    case FiberPathRole::Unknown: return "unknown";
  }
  return "unknown";
}

FiberBundle FiberBundleBuilder::build(const BundleSet& bundles) const {
  FiberBundle output;
  output.snapshot_version = bundles.snapshot_version;
  output.warnings = bundles.warnings;
  output.visited_states = bundles.visited_states;
  output.truncated = bundles.truncated;

  std::vector<RootBundle> roots = bundles.roots;
  std::sort(roots.begin(), roots.end(), [](const RootBundle& left,
                                           const RootBundle& right) {
    return left.root_node < right.root_node;
  });

  for (const auto& root : roots) {
    TargetFiber fiber;
    fiber.target_node = root.root_node;
    fiber.raw_path_count = root.paths.size();
    std::set<std::string> seen_exact;

    for (const auto& path : root.paths) {
      FiberPath converted;
      converted.target_node = root.root_node;
      converted.anchor_node = path.anchor_node;
      converted.nodes = path.nodes;
      converted.edges = path.edges;
      converted.evidence = path.evidence;
      converted.source_lineage = source_lineage(path);
      converted.evidence_family_lineage = evidence_families(path);
      converted.derivation_lineage = derivations(path);
      converted.confidence = std::clamp(path.score, 0.0, 1.0);
      converted.query_relevance = std::clamp(path.query_relevance, 0.0, 1.0);
      converted.target_consistency =
          std::clamp(path.target_consistency, 0.0, 1.0);
      converted.completeness = std::clamp(path.completeness, 0.0, 1.0);
      converted.temporal_consistency = path.temporal_consistent ? 1.0 : 0.0;
      converted.provenance_quality = path.edges.empty()
                                         ? (converted.evidence.empty() ? 0.0 : 1.0)
                                         : std::clamp(
                                               1.0 - static_cast<double>(
                                                         path.provenance_findings.size()) /
                                                         static_cast<double>(
                                                             path.edges.size()),
                                               0.0, 1.0);
      converted.contains_contradiction = path.contains_contradiction;
      converted.contains_hypothetical = path.contains_hypothetical;
      converted.semantic_verification = path.semantic_verification;
      converted.role = classify(path);
      converted.irrelevant = converted.role == FiberPathRole::Noise;
      converted.eligible_for_support =
          converted.role == FiberPathRole::Support &&
          converted.query_relevance >= kEligibilityThreshold &&
          converted.target_consistency >= kEligibilityThreshold &&
          converted.completeness >= kEligibilityThreshold &&
          converted.provenance_quality > 0.0;
      converted.eligible_for_opposition =
          converted.role == FiberPathRole::Opposition &&
          converted.query_relevance >= kEligibilityThreshold &&
          converted.target_consistency >= kEligibilityThreshold &&
          converted.provenance_quality > 0.0;
      converted.route_signature = route_signature(converted);
      converted.evidence_lineage_signature =
          join(converted.evidence_family_lineage, '\x1f');
      converted.causal_ancestry_signature =
          join(converted.derivation_lineage, '\x1f');
      converted.id = path_hash(converted);

      std::ostringstream exact;
      exact << converted.route_signature << '|'
            << converted.evidence_lineage_signature << '|'
            << static_cast<int>(converted.role);
      if (!seen_exact.insert(exact.str()).second) continue;
      fiber.paths.push_back(std::move(converted));
    }

    std::sort(fiber.paths.begin(), fiber.paths.end(),
              [](const FiberPath& left, const FiberPath& right) {
      if (left.id != right.id) return left.id < right.id;
      return left.edges < right.edges;
    });
    fiber.unique_route_count = fiber.paths.size();
    for (const auto& path : fiber.paths) {
      if (path.role == FiberPathRole::Noise) ++fiber.noise_path_count;
      else ++fiber.relevant_path_count;
    }
    fiber.retrieval_noise_ratio = fiber.paths.empty()
                                      ? 0.0
                                      : static_cast<double>(fiber.noise_path_count) /
                                            static_cast<double>(fiber.paths.size());

    DisjointSet groups(fiber.paths.size());
    for (size_t left = 0; left < fiber.paths.size(); ++left) {
      if (fiber.paths[left].role == FiberPathRole::Noise) continue;
      for (size_t right = left + 1; right < fiber.paths.size(); ++right) {
        if (fiber.paths[right].role == FiberPathRole::Noise) continue;
        if (overlaps(fiber.paths[left].evidence_family_lineage,
                     fiber.paths[right].evidence_family_lineage) ||
            overlaps(fiber.paths[left].derivation_lineage,
                     fiber.paths[right].derivation_lineage)) {
          groups.unite(left, right);
        }
      }
    }

    std::map<size_t, std::vector<size_t>> components;
    for (size_t index = 0; index < fiber.paths.size(); ++index) {
      if (fiber.paths[index].role == FiberPathRole::Noise) continue;
      components[groups.find(index)].push_back(index);
    }

    std::vector<const FiberPath*> support_representatives;
    double contradiction_mass = 0.0;
    double completeness_sum = 0.0;
    size_t supported_with_evidence = 0;

    for (const auto& [component, indices] : components) {
      (void)component;
      EvidenceCorrelationGroup group;
      std::set<std::string> families;
      std::set<std::string> derivation_ids;
      const FiberPath* representative = nullptr;
      for (size_t index : indices) {
        const FiberPath& path = fiber.paths[index];
        group.path_ids.push_back(path.id);
        families.insert(path.evidence_family_lineage.begin(),
                        path.evidence_family_lineage.end());
        derivation_ids.insert(path.derivation_lineage.begin(),
                              path.derivation_lineage.end());
        if (!representative || path_quality(path) > path_quality(*representative) ||
            (path_quality(path) == path_quality(*representative) &&
             path.id < representative->id)) {
          representative = &path;
        }
      }
      group.evidence_family_ids.assign(families.begin(), families.end());
      group.derivation_ids.assign(derivation_ids.begin(), derivation_ids.end());
      if (representative) {
        group.representative_path_id = representative->id;
        group.role = representative->role;
        uint64_t group_hash = 1469598103934665603ULL;
        group_hash = fnv1a_append(group_hash,
                                  join(group.evidence_family_ids, '\x1f'));
        group_hash = fnv1a_append(group_hash,
                                  join(group.derivation_ids, '\x1f'));
        group_hash = fnv1a_append(group_hash,
                                  std::to_string(static_cast<int>(group.role)));
        group.id = group_hash;
        group.independent_support =
            representative->eligible_for_support &&
            !group.evidence_family_ids.empty();
        if (group.independent_support) {
          ++fiber.independent_evidence_family_count;
          support_representatives.push_back(representative);
          completeness_sum += representative->completeness;
          if (!representative->evidence_family_lineage.empty()) {
            ++supported_with_evidence;
          }
        }
        if (representative->eligible_for_opposition) {
          contradiction_mass = std::max(
              contradiction_mass, path_quality(*representative));
        }
      }
      std::sort(group.path_ids.begin(), group.path_ids.end());
      fiber.correlation_groups.push_back(std::move(group));
    }

    std::sort(fiber.correlation_groups.begin(),
              fiber.correlation_groups.end(),
              [](const EvidenceCorrelationGroup& left,
                 const EvidenceCorrelationGroup& right) {
      return left.id < right.id;
    });

    fiber.independent_path_count =
        fiber.independent_evidence_family_count;
    fiber.independent_support_score =
        1.0 - std::exp(-0.7 * static_cast<double>(
                                  fiber.independent_evidence_family_count));
    fiber.degeneracy = fiber.independent_support_score;
    fiber.contradiction_mass = contradiction_mass;
    fiber.completeness_score = support_representatives.empty()
                                   ? 0.0
                                   : completeness_sum /
                                         static_cast<double>(
                                             support_representatives.size());
    fiber.evidence_coverage = support_representatives.empty()
                                  ? 0.0
                                  : static_cast<double>(supported_with_evidence) /
                                        static_cast<double>(
                                            support_representatives.size());

    if (support_representatives.size() > 1) {
      double diversity_sum = 0.0;
      size_t pairs = 0;
      for (size_t left = 0; left < support_representatives.size(); ++left) {
        for (size_t right = left + 1;
             right < support_representatives.size(); ++right) {
          diversity_sum += jaccard_distance(
              support_representatives[left]->edges,
              support_representatives[right]->edges);
          ++pairs;
        }
      }
      fiber.relevant_route_diversity =
          pairs == 0 ? 0.0
                     : std::clamp(diversity_sum /
                                      static_cast<double>(pairs),
                                  0.0, 1.0);
    }
    output.fibers.push_back(std::move(fiber));
  }

  output.immutable_hash = hash(output);
  return output;
}

uint64_t FiberBundleBuilder::hash(const FiberBundle& bundle) {
  uint64_t hash = 1469598103934665603ULL;
  hash = fnv1a_append(hash, std::to_string(bundle.schema_version));
  hash = fnv1a_append(hash, std::to_string(bundle.snapshot_version));
  hash = fnv1a_append(hash, bundle.truncated ? "1" : "0");
  for (const auto& fiber : bundle.fibers) {
    hash = fnv1a_append(hash, "t" + std::to_string(fiber.target_node));
    hash = fnv1a_append(
        hash, "i" +
                  std::to_string(fiber.independent_evidence_family_count));
    hash = fnv1a_append(hash,
                        "n" + std::to_string(fiber.noise_path_count));
    for (const auto& path : fiber.paths) {
      hash = fnv1a_append(hash, "p" + std::to_string(path.id));
      hash = fnv1a_append(hash,
                          "r" + std::to_string(static_cast<int>(path.role)));
    }
    for (const auto& group : fiber.correlation_groups) {
      hash = fnv1a_append(hash, "g" + std::to_string(group.id));
    }
  }
  for (const auto& warning : bundle.warnings) {
    hash = fnv1a_append(hash, "w" + warning);
  }
  return hash;
}

}  // namespace graphene
