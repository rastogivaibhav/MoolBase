#pragma once

#include "graphene/dialectic.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

struct FiberPath {
  uint64_t id{0};
  uint32_t target_node{0};
  uint32_t anchor_node{0};
  std::vector<uint32_t> nodes;
  std::vector<uint32_t> edges;
  std::vector<EvidenceRef> evidence;
  std::vector<std::string> source_lineage;
  double confidence{0.0};
  double temporal_consistency{0.0};
  double provenance_quality{0.0};
  bool contains_contradiction{false};
  bool contains_hypothetical{false};
};

struct TargetFiber {
  uint32_t target_node{0};
  std::vector<FiberPath> paths;
  size_t raw_path_count{0};
  size_t independent_path_count{0};
  double degeneracy{0.0};
};

struct FiberBundle {
  uint64_t snapshot_version{0};
  std::vector<TargetFiber> fibers;
  std::vector<std::string> warnings;
  size_t visited_states{0};
  bool truncated{false};
  uint64_t immutable_hash{0};
};

class FiberBundleBuilder {
 public:
  FiberBundle build(const BundleSet& bundles) const;
  static uint64_t hash(const FiberBundle& bundle);
};

}  // namespace graphene
