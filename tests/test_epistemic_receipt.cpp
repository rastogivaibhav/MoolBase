#include "graphene/epistemic_receipt.hpp"

#include <cassert>
#include <iostream>

using namespace graphene;

int main() {
  HypoKoshRuntimeResult result;
  result.receipt.snapshot_version = 77;
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

  assert(receipt.schema_version == 1);
  assert(receipt.snapshot_version == 77);
  assert(receipt.bundle_hash == 99123);
  assert(receipt.content_hash != 0);
  assert(receipt.content_hash == repeated.content_hash);
  assert(receipt.primary_node == 42);
  assert(receipt.evidence_edges == std::vector<uint32_t>({4, 9}));
  assert(receipt.selected_evidence.selected_path_ids ==
         std::vector<uint64_t>({100, 200}));
  assert(receipt.selected_evidence.source_lineage ==
         std::vector<std::string>({"source-a", "source-b", "source-c"}));
  assert(receipt.selected_evidence.independent_evidence_family_count == 2);

  result.residual_uncertainty.push_back("new contradiction");
  const CompactEpistemicReceipt changed =
      build_compact_epistemic_receipt(result);
  assert(changed.content_hash != receipt.content_hash);

  std::cout << "compact_epistemic_receipt_contract_passed=true\n";
  return 0;
}
