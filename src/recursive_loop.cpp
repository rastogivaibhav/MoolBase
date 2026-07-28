#include "graphene/recursive_loop.hpp"

#include <algorithm>

namespace graphene {
namespace {

void add_proposal(std::vector<RepairProposal>* proposals,
                  RepairAction action,
                  uint32_t target,
                  std::string reason) {
  const auto it = std::find_if(
      proposals->begin(), proposals->end(), [&](const RepairProposal& value) {
        return value.action == action && value.target_node == target;
      });
  if (it == proposals->end()) {
    proposals->push_back({action, target, std::move(reason), false});
  }
}

std::vector<RepairProposal> propose_repairs(
    const HypoKoshRuntimeResult& runtime) {
  std::vector<RepairProposal> output;
  for (const auto& fiber : runtime.final_bundle.fibers) {
    if (fiber.independent_path_count < 2) {
      add_proposal(&output, RepairAction::SeekIndependentEvidence,
                   fiber.target_node,
                   "independent evidence count is below two");
    }
    if (fiber.contradiction_ratio > 0.0) {
      add_proposal(&output, RepairAction::ResolveContradiction,
                   fiber.target_node,
                   "contradictory paths remain unresolved");
    }
    if (fiber.path_diversity < 0.25) {
      add_proposal(&output, RepairAction::ExpandMinorityPath,
                   fiber.target_node,
                   "path diversity is too low");
    }
  }
  if (runtime.final_convergence.false_promotion_risk > 0.0) {
    add_proposal(&output, RepairAction::DemoteUnsafeInference,
                 runtime.final_convergence.primary_node,
                 "selected path carries false-promotion risk");
  }
  if (runtime.final_stability.requires_abstention && output.empty()) {
    add_proposal(&output, RepairAction::RequestHumanReview, 0,
                 "no safe local repair can establish evidence");
  }
  return output;
}

Status record_iteration(ModelWorldStore* store,
                        const std::string& trace_namespace,
                        uint32_t cycle,
                        const HypoKoshRuntimeResult& runtime) {
  if (!store) return Status::ok();
  const std::string prefix = trace_namespace + ":" + std::to_string(cycle);

  ModelWorldNodeInput hypothesis;
  hypothesis.object_id = prefix + ":hypothesis";
  hypothesis.type = ModelWorldNodeType::Hypothesis;
  hypothesis.status = ModelWorldStatus::Hypothetical;
  hypothesis.content = runtime.answer.has_answer
                           ? "candidate target " +
                                 std::to_string(runtime.answer.primary_node)
                           : "no defensible candidate target";
  hypothesis.vector = {};
  hypothesis.signature = 0;
  hypothesis.source_id = "";
  hypothesis.metadata["reasoning_status"] =
      runtime.answer.epistemic_status;
  ModelWorldPutResult hypothesis_result;
  Status status = store->put(hypothesis, &hypothesis_result);
  if (!status) return status;

  ModelWorldNodeInput opposition;
  opposition.object_id = prefix + ":opposition";
  opposition.type = ModelWorldNodeType::Opposition;
  opposition.status = ModelWorldStatus::Inferred;
  opposition.content =
      runtime.final_opposition.challenged_claims.empty()
          ? "no material opposition"
          : runtime.final_opposition.challenged_claims.front();
  opposition.vector = {};
  opposition.parent_nodes = {hypothesis_result.node.node_id};
  opposition.metadata["opposition_score"] =
      std::to_string(runtime.final_opposition.opposition_score);
  ModelWorldPutResult opposition_result;
  status = store->put(opposition, &opposition_result);
  if (!status) return status;

  ModelWorldNodeInput outcome;
  outcome.object_id = prefix + ":outcome";
  outcome.type = ModelWorldNodeType::Outcome;
  outcome.status = ModelWorldStatus::Observed;
  outcome.content = "recursive reasoning cycle completed with status " +
                    runtime.answer.epistemic_status;
  outcome.vector = {};
  outcome.source_id = "graphenedb-recursive-runtime";
  outcome.parent_nodes = {hypothesis_result.node.node_id,
                          opposition_result.node.node_id};
  outcome.metadata["stability_score"] =
      std::to_string(runtime.final_stability.total_score);
  return store->put(outcome);
}

}  // namespace

RecursiveReasoningController::RecursiveReasoningController(GrapheneDB& db)
    : db_(db) {}

RecursiveLoopResult RecursiveReasoningController::run(
    const std::vector<float>& query,
    uint64_t query_signature,
    const RecursiveLoopOptions& requested_options,
    uint64_t snapshot_version) {
  RecursiveLoopOptions options = requested_options;
  options.max_cycles = std::clamp<uint32_t>(options.max_cycles, 1, 8);
  options.minimum_stability_gain =
      std::clamp(options.minimum_stability_gain, 0.0, 1.0);

  RecursiveLoopResult output;
  ModelWorldStore store(db_);
  ModelWorldStore* store_ptr = options.record_model_world_trace ? &store : nullptr;
  double previous_stability = -1.0;
  uint64_t previous_bundle_hash = 0;

  for (uint32_t cycle = 0; cycle < options.max_cycles; ++cycle) {
    RecursiveIteration iteration;
    iteration.cycle = cycle;
    iteration.runtime = HypoKoshRuntime(db_).reason(
        query, query_signature, options.runtime, snapshot_version);
    iteration.repair_proposals = propose_repairs(iteration.runtime);
    iteration.stability_gain = previous_stability < 0.0
                                   ? iteration.runtime.final_stability.total_score
                                   : iteration.runtime.final_stability.total_score -
                                         previous_stability;

    const Status record_status = record_iteration(
        store_ptr, options.trace_namespace, cycle, iteration.runtime);
    if (!record_status) {
      output.stop_reason = "model_world_record_failed: " +
                           record_status.message;
      output.iterations.push_back(std::move(iteration));
      output.answer = output.iterations.back().runtime.answer;
      return output;
    }
    output.iterations.push_back(std::move(iteration));
    const auto& current = output.iterations.back();
    output.answer = current.runtime.answer;

    if (current.runtime.final_stability.stable &&
        !current.runtime.final_opposition.reopen_required) {
      output.stop_reason = "stable";
      break;
    }
    if (current.repair_proposals.empty()) {
      output.stop_reason = "no_safe_repair";
      break;
    }
    if (previous_bundle_hash != 0 &&
        previous_bundle_hash == current.runtime.final_bundle.immutable_hash) {
      output.stop_reason = "no_epistemic_gain";
      break;
    }
    if (previous_stability >= 0.0 &&
        current.stability_gain < options.minimum_stability_gain) {
      output.stop_reason = "stability_gain_below_threshold";
      break;
    }
    previous_stability = current.runtime.final_stability.total_score;
    previous_bundle_hash = current.runtime.final_bundle.immutable_hash;
  }

  if (output.stop_reason == "no_result") {
    output.stop_reason = output.iterations.size() >= options.max_cycles
                             ? "max_cycles"
                             : "completed";
  }
  output.durable_writes = options.record_model_world_trace;
  return output;
}

}  // namespace graphene
