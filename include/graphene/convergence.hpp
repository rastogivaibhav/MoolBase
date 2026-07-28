#pragma once

#include "graphene/stability_critic.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

struct RuntimePathReference {
  uint32_t target_node{0};
  uint64_t path_id{0};
  size_t path_index{0};
  std::string reason;
};

struct RuntimeConvergedAnswer {
  bool has_answer{false};
  uint32_t primary_node{0};
  uint64_t primary_path_id{0};
  double confidence{0.0};
  double false_promotion_risk{0.0};
  std::vector<RuntimePathReference> selected_paths;
  std::vector<RuntimePathReference> discarded_paths;
  std::vector<uint32_t> evidence_edges;
  std::vector<EvidenceRef> evidence;
  std::vector<std::string> residual_uncertainty;
  uint64_t source_bundle_hash{0};
};

struct RuntimeConvergenceOptions {
  size_t max_selected_paths{4};
  double minimum_confidence{0.45};
  bool require_observed_or_discovered_for_empirical{true};
};

class ConvergenceEngine {
 public:
  RuntimeConvergedAnswer converge(
      const FiberBundle& bundle,
      const StabilityAssessment& stability,
      QueryMode mode = QueryMode::Balanced,
      const RuntimeConvergenceOptions& options = {}) const;
};

}  // namespace graphene
