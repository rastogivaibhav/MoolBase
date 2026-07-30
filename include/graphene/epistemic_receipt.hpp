#pragma once

#include "graphene/hypokosh_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

struct CompactEvidenceSummary {
  uint32_t target_node{0};
  std::vector<uint64_t> selected_path_ids;
  std::vector<std::string> source_lineage;
  std::vector<std::string> evidence_family_lineage;
  std::vector<std::string> derivation_lineage;
  std::vector<std::string> verifier_versions;
  size_t independent_evidence_family_count{0};
  double contradiction_mass{0.0};
  double completeness_score{0.0};
};

// Durable, content-addressed answer receipt. The full FiberBundle remains an
// ephemeral query workspace unless an explicit audit policy retains it.
struct CompactEpistemicReceipt {
  uint32_t schema_version{1};
  uint64_t snapshot_version{0};
  uint64_t bundle_hash{0};
  uint64_t content_hash{0};
  uint32_t primary_node{0};
  GovernedEpistemicStatus status{GovernedEpistemicStatus::Abstain};
  double confidence{0.0};
  double lyapunov_energy{0.0};
  SemanticVerificationStatus semantic_verification{
      SemanticVerificationStatus::Unverified};
  std::vector<uint32_t> evidence_edges;
  CompactEvidenceSummary selected_evidence;
  std::vector<std::string> residual_uncertainty;
};

CompactEpistemicReceipt build_compact_epistemic_receipt(
    const HypoKoshRuntimeResult& result);

}  // namespace graphene
