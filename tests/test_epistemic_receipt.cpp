#include "graphene/epistemic_receipt.hpp"

#include <cassert>
#include <iostream>

using namespace graphene;

int main() {
  HypoKoshRuntimeResult result;
  result.receipt.snapshot_version = 77;
  result.receipt.hypokosh_capability_enabled = true;
  result.receipt.dwm_capability_enabled = true;
  result.receipt.opposition_research_enabled = false;
  result.receipt.graphene_executed = true;
  result.receipt.path_verifier_executed = false;
  result.receipt.stability_critic_executed = true;
  result.receipt.epistemic_admissibility_executed = true;
  result.receipt.convergence_executed = true;
  result.receipt.opposition_executed = true;
  result.receipt.bounded_recovery_executed = false;
  result.receipt.governed_projection_executed = true;
  result.receipt.model_world_updated = false;
  result.receipt.terminal_cause = "dialectic_opposition_blocks_resolution";
  result.status = GovernedEpistemicStatus::ProvisionallyResolved;
  result.primary_node = 42;
  result.confidence = 0.88;
  result.evidence_edges = {9, 4, 9};
  result.residual_uncertainty = {
      "semantic verification pending", "independent test pending"};
  result.final_bundle.immutable_hash = 99123;
  result.final_stability.lyapunov_energy = 0.12;
  result.final_admissibility.semantic_verification =
      SemanticVerificationStatus::Verified;
  result.final_convergence.primary_node = 42;
  result.final_convergence.confidence = 0.88;
  result.final_convergence.selected_paths = {
      {42, 1, "selected"}, {42, 0, "selected"}};

  NativeEpistemicEvent decision;
  decision.sequence = 1;
  decision.source = EpistemicEventSource::HypoKosh;
  decision.type = EpistemicEventType::Decision;
  decision.hypothesis_node = 42;
  decision.competing_hypotheses = {43, 42, 43};
  decision.evidence_edges = {9, 4, 9};
  decision.evidence_family_ids = {"family-b", "family-a", "family-a"};
  decision.epistemic_state = "selected";
  decision.reason = "test decision";
  result.receipt.epistemic_events.push_back(decision);

  NativeEpistemicEvent terminal;
  terminal.sequence = 2;
  terminal.source = EpistemicEventSource::GrapheneCore;
  terminal.type = EpistemicEventType::Terminal;
  terminal.hypothesis_node = 42;
  terminal.evidence_edges = {4, 9};
  terminal.evidence_family_ids = {"family-a", "family-b"};
  terminal.epistemic_state = "provisionally_resolved";
  terminal.reason = "test terminal";
  result.receipt.epistemic_events.push_back(terminal);

  TargetFiber fiber;
  fiber.target_node = 42;
  fiber.independent_evidence_family_count = 2;
  fiber.contradiction_mass = 0.05;
  fiber.completeness_score = 0.92;

  FiberPath first;
  first.id = 200;
  first.target_node = 42;
  first.source_lineage = {"source-b", "source-a"};
  first.evidence_family_lineage = {"family-a"};
  first.derivation_lineage = {"derivation-a"};
  first.verifier_version = "verifier-v1";

  FiberPath second;
  second.id = 100;
  second.target_node = 42;
  second.source_lineage = {"source-c", "source-a"};
  second.evidence_family_lineage = {"family-b"};
  second.derivation_lineage = {"derivation-b"};
  second.verifier_version = "verifier-v1";

  fiber.paths = {first, second};
  result.final_bundle.fibers.push_back(fiber);

  const CompactEpistemicReceipt receipt =
      build_compact_epistemic_receipt(result);
  const CompactEpistemicReceipt repeated =
      build_compact_epistemic_receipt(result);

  assert(receipt.schema_version == 3);
  assert(receipt.snapshot_version == 77);
  assert(receipt.bundle_hash == 99123);
  assert(receipt.content_hash != 0);
  assert(receipt.content_hash == repeated.content_hash);
  assert(receipt.primary_node == 42);
  assert(receipt.hypokosh_capability_enabled);
  assert(receipt.dwm_capability_enabled);
  assert(!receipt.opposition_research_enabled);
  assert(receipt.graphene_executed);
  assert(receipt.stability_critic_executed);
  assert(receipt.epistemic_admissibility_executed);
  assert(receipt.convergence_executed);
  assert(receipt.opposition_executed);
  assert(!receipt.bounded_recovery_executed);
  assert(receipt.governed_projection_executed);
  assert(!receipt.model_world_updated);
  assert(receipt.terminal_cause ==
         "dialectic_opposition_blocks_resolution");
  assert(receipt.evidence_edges == std::vector<uint32_t>({4, 9}));
  assert(receipt.selected_evidence.selected_path_ids ==
         std::vector<uint64_t>({100, 200}));
  assert(receipt.selected_evidence.source_lineage ==
         std::vector<std::string>({"source-a", "source-b", "source-c"}));
  assert(receipt.selected_evidence.independent_evidence_family_count == 2);
  assert(receipt.epistemic_events.size() == 2);
  assert(receipt.epistemic_events.front().sequence == 1);
  assert(receipt.epistemic_events.front().hypothesis_node == 42);
  assert(receipt.epistemic_events.front().competing_hypotheses ==
         std::vector<uint32_t>({42, 43}));
  assert(receipt.epistemic_events.front().evidence_edges ==
         std::vector<uint32_t>({4, 9}));
  assert(receipt.epistemic_events.front().evidence_family_ids ==
         std::vector<std::string>({"family-a", "family-b"}));

  HypoKoshRuntimeResult revised = result;
  revised.receipt.epistemic_events.front().epistemic_state = "challenged";
  const CompactEpistemicReceipt revised_receipt =
      build_compact_epistemic_receipt(revised);
  assert(revised_receipt.content_hash != receipt.content_hash);

  result.residual_uncertainty.push_back("new contradiction");
  const CompactEpistemicReceipt changed =
      build_compact_epistemic_receipt(result);
  assert(changed.content_hash != receipt.content_hash);

  HypoKoshRuntimeResult capability_change = result;
  capability_change.residual_uncertainty.pop_back();
  capability_change.receipt.dwm_capability_enabled = false;
  capability_change.receipt.opposition_executed = false;
  const CompactEpistemicReceipt capability_receipt =
      build_compact_epistemic_receipt(capability_change);
  assert(capability_receipt.content_hash != receipt.content_hash);

  HypoKoshRuntimeResult cause_change = result;
  cause_change.residual_uncertainty.pop_back();
  cause_change.receipt.terminal_cause =
      "material_contradiction_blocks_resolution";
  const CompactEpistemicReceipt cause_receipt =
      build_compact_epistemic_receipt(cause_change);
  assert(cause_receipt.content_hash != receipt.content_hash);

  std::cout << "compact_epistemic_receipt_contract_passed=true\n";
  return 0;
}
