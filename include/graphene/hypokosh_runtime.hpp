#pragma once

#include "graphene/escape.hpp"
#include "graphene/opposition.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

enum class RuntimeStopReason : uint8_t {
  Stable = 0,
  MaxRounds = 1,
  NoNewEvidence = 2,
  HumanEvidenceRequired = 3,
  Abstained = 4,
  IntegrityFailure = 5
};

struct GovernedAnswer {
  bool has_answer{false};
  uint32_t primary_node{0};
  double confidence{0.0};
  std::string epistemic_status{"abstain"};
  std::vector<uint32_t> evidence_edges;
  std::vector<EvidenceRef> evidence;
  std::vector<std::string> residual_uncertainty;
  std::vector<std::string> promotion_warnings;
};

struct HypoKoshRuntimeOptions {
  DialecticOptions expansion{};
  FiberBundleBuildOptions bundle{};
  StabilityWeights stability_weights{};
  StabilityThresholds stability_thresholds{};
  EscapeBudget escape_budget{};
  RuntimeConvergenceOptions convergence{};
  RuntimeOppositionOptions opposition{};
  uint32_t max_rounds{3};
};

struct HypoKoshRuntimeResult {
  FiberBundle initial_bundle;
  StabilityAssessment initial_stability;
  RuntimeConvergedAnswer initial_convergence;
  RuntimeOppositionReport initial_opposition;
  EscapePlan initial_escape;

  FiberBundle final_bundle;
  StabilityAssessment final_stability;
  RuntimeConvergedAnswer final_convergence;
  RuntimeOppositionReport final_opposition;
  EscapePlan final_escape;

  GovernedAnswer answer;
  uint32_t rounds{0};
  RuntimeStopReason stop_reason{RuntimeStopReason::Abstained};
  bool durable_writes{false};
};

class HypoKoshRuntime {
 public:
  explicit HypoKoshRuntime(const GrapheneDB& db);

  HypoKoshRuntimeResult reason(
      const std::vector<float>& query,
      uint64_t query_signature,
      const HypoKoshRuntimeOptions& options = {},
      uint64_t snapshot_version = kInfVersion) const;

 private:
  const GrapheneDB& db_;
};

}  // namespace graphene
