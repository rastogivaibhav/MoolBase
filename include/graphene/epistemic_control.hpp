#pragma once

#include "graphene/stability_critic.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace graphene {

struct EpistemicAdmissibility {
  double relevance{0.0};
  double target_consistency{0.0};
  double completeness{0.0};
  double provenance{0.0};
  double independent_support{0.0};
  double retrieval_noise{0.0};
  double unresolved_contradiction{0.0};

  bool evidence_admissible{false};
  bool contradiction_blocks_resolution{false};
  bool semantic_tie{false};
  bool sufficient_independent_support{false};
  bool requires_external_verification{true};
  SemanticVerificationStatus semantic_verification{
      SemanticVerificationStatus::Unverified};
  std::vector<std::string> reasons;
};

// Read-only target-level coordinates used by diagnostic receipts and benchmark
// telemetry. These values are projected from the exact same ranking inputs used
// by production convergence; exposing them must not influence selection.
struct TargetEpistemicTrace {
  uint32_t target_node{0};
  double support_strength{0.0};
  double opposition_strength{0.0};
  double belief_strength{0.0};
  SemanticVerificationStatus semantic_verification{
      SemanticVerificationStatus::Unverified};
  size_t independent_support_family_count{0};
  double best_support_score{0.0};
};

class EpistemicController {
 public:
  std::vector<TargetEpistemicTrace> inspect_targets(
      const FiberBundle& bundle) const;

  EpistemicAdmissibility assess(
      const FiberBundle& bundle,
      const StabilityAssessment& stability,
      QueryMode mode = QueryMode::Balanced) const;

  EpistemicAdmissibility assess(
      const FiberBundle& bundle,
      const StabilityAssessment& stability,
      QueryMode mode,
      const StabilityThresholds& thresholds) const;

  ConvergedAnswer converge(
      const FiberBundle& bundle,
      const EpistemicAdmissibility& admissibility,
      const StabilityAssessment& stability,
      const DialecticOptions& options = {}) const;

  OppositionReport oppose(
      const FiberBundle& bundle,
      const ConvergedAnswer& answer,
      const EpistemicAdmissibility& admissibility,
      const StabilityAssessment& stability,
      const DialecticOptions& options = {}) const;
};

}  // namespace graphene
