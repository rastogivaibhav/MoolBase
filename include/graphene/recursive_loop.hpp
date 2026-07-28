#pragma once

#include "graphene/hypokosh_runtime.hpp"
#include "graphene/model_world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

enum class RepairAction : uint8_t {
  SeekIndependentEvidence = 0,
  ResolveContradiction = 1,
  DemoteUnsafeInference = 2,
  ExpandMinorityPath = 3,
  RequestHumanReview = 4
};

struct RepairProposal {
  RepairAction action{RepairAction::SeekIndependentEvidence};
  uint32_t target_node{0};
  std::string reason;
  bool mutates_truth{false};
};

struct RecursiveLoopOptions {
  HypoKoshRuntimeOptions runtime{};
  uint32_t max_cycles{3};
  double minimum_stability_gain{0.01};
  bool record_model_world_trace{false};
  std::string trace_namespace{"hypokosh-recursive"};
};

struct RecursiveIteration {
  uint32_t cycle{0};
  HypoKoshRuntimeResult runtime;
  std::vector<RepairProposal> repair_proposals;
  double stability_gain{0.0};
};

struct RecursiveLoopResult {
  std::vector<RecursiveIteration> iterations;
  GovernedAnswer answer;
  std::string stop_reason{"no_result"};
  bool durable_writes{false};
};

class RecursiveReasoningController {
 public:
  explicit RecursiveReasoningController(GrapheneDB& db);

  RecursiveLoopResult run(
      const std::vector<float>& query,
      uint64_t query_signature,
      const RecursiveLoopOptions& options = {},
      uint64_t snapshot_version = kInfVersion);

 private:
  GrapheneDB& db_;
};

}  // namespace graphene
