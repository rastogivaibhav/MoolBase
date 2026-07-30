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

  Edge extracted;
  extracted.id = 8;
  extracted.origin = EdgeOrigin::Observed;
  extracted.role = EdgeRole::Supports;
  extracted.metadata = {
      {"graphene_source_id", "document-17"},
      {"graphene_source_uri", "file:///document-17.json"},
      {"graphene_evidence_id", "edge-fact-99"},
      {"graphene_evidence_uri", "file:///document-17.json#fact-99"},
      {"graphene_evidence_text", "supporting sentence"}};
  const EdgeProvenance extracted_result = assess_edge_provenance(extracted);
  assert(extracted_result.findings.empty());
  assert(extracted_result.evidence.size() == 1);
  assert(extracted_result.evidence.front().source_id == "document-17");
  assert(extracted_result.evidence.front().span == "supporting sentence");

  Edge inferred;
  inferred.id = 9;
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
  unsupported.id = 10;
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
