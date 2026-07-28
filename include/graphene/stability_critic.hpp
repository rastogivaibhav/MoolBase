#pragma once

#include "graphene/fiber_bundle.hpp"

#include <string>
#include <vector>

namespace graphene {

struct StabilityWeights {
  double temporal{0.20};
  double path_diversity{0.15};
  double degeneracy{0.15};
  double provenance{0.20};
  double contradiction{0.15};
  double pattern_lock{0.10};
  double missing_evidence{0.05};
};

struct StabilityThresholds {
  double stable_score{0.60};
  double escape_score{0.55};
  double opposition_score{0.15};
  double abstention_score{0.25};
  double severe_contradiction{0.60};
  double high_pattern_lock{0.70};
};

struct StabilityAssessment {
  double temporal_consistency{0.0};
  double path_diversity{0.0};
  double degeneracy_score{0.0};
  double provenance_score{0.0};
  double contradiction_score{0.0};
  double pattern_lock_score{0.0};
  double missing_evidence_penalty{0.0};
  double total_score{0.0};

  bool stable{false};
  bool requires_escape{false};
  bool requires_opposition{false};
  bool requires_abstention{false};
  std::vector<std::string> reasons;
};

class StabilityCriticV0 {
 public:
  StabilityAssessment assess(
      const FiberBundle& bundle,
      QueryMode mode = QueryMode::Balanced,
      const StabilityWeights& weights = {},
      const StabilityThresholds& thresholds = {}) const;
};

}  // namespace graphene
