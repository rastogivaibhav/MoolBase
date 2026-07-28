#pragma once

#include "graphene/dialectic.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

// An immutable, source-aware reasoning path. This is deliberately separate
// from the legacy Path/DialecticPath types so the thesis contract is explicit.
struct FiberPath {
  uint64_t id{0};
  uint32_t root_node{0};
  uint32_t anchor_node{0};
  std::vector<uint32_t> nodes;
  std::vector<uint32_t> edges;
  double score{0.0};
  double temporal_consistency{1.0};
  double provenance_quality{0.0};
  double independence_score{0.0};
  bool contains_contradiction{false};
  bool contains_hypothetical{false};
  bool contains_inferred{false};
  bool contains_analogical{false};
  std::vector<EvidenceRef> evidence;
  std::vector<ProvenanceFinding> provenance_findings;
  std::vector<std::string> source_lineage;
};

struct TargetFiber {
  uint32_t target_node{0};
  std::vector<FiberPath> paths;
  size_t raw_path_count{0};
  size_t independent_path_count{0};
  double degeneracy{0.0};
  double path_diversity{0.0};
  double contradiction_ratio{0.0};
  double evidence_coverage{0.0};
};

struct MissingEvidence {
  uint32_t target_node{0};
  std::string requirement;
};

struct FiberBundle {
  uint64_t query_id{0};
  uint64_t snapshot_version{0};
  std::vector<uint32_t> semantic_candidates;
  std::vector<TargetFiber> fibers;
  std::vector<MissingEvidence> missing_evidence;
  size_t visited_states{0};
  bool truncated{false};
  std::vector<std::string> warnings;
  uint64_t immutable_hash{0};
};

struct FiberBundleBuildOptions {
  // Two paths cannot count as independent if this proportion of their source
  // lineage overlaps. A value of 1.0 only rejects identical lineages.
  double max_source_overlap_for_independence{0.50};
};

class FiberBundleBuilder {
 public:
  explicit FiberBundleBuilder(const GrapheneDB& db);

  FiberBundle from_bundle_set(
      const BundleSet& legacy,
      uint64_t query_id,
      const FiberBundleBuildOptions& options = {}) const;

  static uint64_t compute_hash(const FiberBundle& bundle);

 private:
  const GrapheneDB& db_;
};

}  // namespace graphene
