#include "graphene/epistemic.hpp"

#include <cassert>
#include <iostream>

using namespace graphene;

int main() {
  Edge explicit_lineage;
  explicit_lineage.id = 7;
  explicit_lineage.origin = EdgeOrigin::Observed;
  explicit_lineage.role = EdgeRole::Causal;
  explicit_lineage.metadata = {
      {"source_id", "incident-report"},
      {"evidence_family_id", "incident-42"},
      {"derivation_id", "extract-run-9"},
      {"evidence_content_hash", "sha256:abc"},
      {"span", "paragraph 4"},
      {"observed_at", "2026-07-28T12:00:00Z"}};
  const EdgeProvenance explicit_result =
      assess_edge_provenance(explicit_lineage);
  assert(explicit_result.findings.empty());
  assert(explicit_result.evidence.size() == 1);
  assert(explicit_result.evidence.front().source_id == "incident-report");
  assert(explicit_result.evidence.front().evidence_family_id == "incident-42");
  assert(explicit_result.evidence.front().derivation_id == "extract-run-9");
  assert(explicit_result.evidence.front().content_hash == "sha256:abc");

  Edge inferred;
  inferred.id = 8;
  inferred.origin = EdgeOrigin::Inferred;
  inferred.role = EdgeRole::Compressed;
  inferred.metadata = {
      {"source_id", "derived-view"},
      {"derived_from", "edges:1,2"}};
  const EdgeProvenance inferred_result = assess_edge_provenance(inferred);
  assert(inferred_result.evidence.size() == 1);
  assert(inferred_result.evidence.front().derivation_id == "edges:1,2");
  assert(inferred_result.findings.empty());

  Edge unsupported;
  unsupported.id = 9;
  unsupported.origin = EdgeOrigin::Observed;
  unsupported.role = EdgeRole::Supports;
  const EdgeProvenance unsupported_result =
      assess_edge_provenance(unsupported);
  assert(unsupported_result.evidence.empty());
  assert(unsupported_result.findings.size() == 1);
  assert(unsupported_result.findings.front().code == "MISSING_EVIDENCE");

  std::cout << "epistemic_lineage_contract_passed=true\n";
  return 0;
}
