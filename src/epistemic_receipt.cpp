#include "graphene/epistemic_receipt.hpp"

#include <algorithm>

namespace graphene {
namespace {

uint64_t append_hash(uint64_t hash, uint64_t value) {
  for (int shift = 0; shift < 8; ++shift) {
    hash ^= static_cast<unsigned char>((value >> (shift * 8)) & 0xffU);
    hash *= 1099511628211ULL;
  }
  return hash;
}

void append_text(uint64_t* hash, const std::string& value) {
  for (unsigned char ch : value) {
    *hash ^= ch;
    *hash *= 1099511628211ULL;
  }
  *hash ^= 0xffU;
  *hash *= 1099511628211ULL;
}

template <typename T>
void sort_unique(std::vector<T>* values) {
  std::sort(values->begin(), values->end());
  values->erase(std::unique(values->begin(), values->end()), values->end());
}

}  // namespace

CompactEpistemicReceipt build_compact_epistemic_receipt(
    const HypoKoshRuntimeResult& result) {
  CompactEpistemicReceipt receipt;
  receipt.snapshot_version = result.receipt.snapshot_version;
  receipt.bundle_hash = result.final_bundle.immutable_hash;
  receipt.primary_node = result.primary_node;
  receipt.status = result.status;
  receipt.confidence = result.confidence;
  receipt.lyapunov_energy = result.final_stability.lyapunov_energy;
  receipt.semantic_verification =
      result.final_admissibility.semantic_verification;
  receipt.evidence_edges = result.evidence_edges;
  receipt.epistemic_events = result.receipt.epistemic_events;
  receipt.residual_uncertainty = result.residual_uncertainty;
  sort_unique(&receipt.evidence_edges);
  sort_unique(&receipt.residual_uncertainty);
  for (auto& event : receipt.epistemic_events) {
    sort_unique(&event.competing_hypotheses);
    sort_unique(&event.reopen_nodes);
    sort_unique(&event.evidence_edges);
    sort_unique(&event.evidence_family_ids);
  }

  const auto fiber_it = std::find_if(
      result.final_bundle.fibers.begin(), result.final_bundle.fibers.end(),
      [&](const TargetFiber& fiber) {
        return fiber.target_node == result.primary_node;
      });
  if (fiber_it != result.final_bundle.fibers.end()) {
    receipt.selected_evidence.target_node = fiber_it->target_node;
    receipt.selected_evidence.independent_evidence_family_count =
        fiber_it->independent_evidence_family_count;
    receipt.selected_evidence.contradiction_mass =
        fiber_it->contradiction_mass;
    receipt.selected_evidence.completeness_score =
        fiber_it->completeness_score;
    for (const auto& reference : result.final_convergence.selected_paths) {
      if (reference.root_node != fiber_it->target_node ||
          reference.path_index >= fiber_it->paths.size()) {
        continue;
      }
      const FiberPath& path = fiber_it->paths[reference.path_index];
      receipt.selected_evidence.selected_path_ids.push_back(path.id);
      receipt.selected_evidence.source_lineage.insert(
          receipt.selected_evidence.source_lineage.end(),
          path.source_lineage.begin(), path.source_lineage.end());
      receipt.selected_evidence.evidence_family_lineage.insert(
          receipt.selected_evidence.evidence_family_lineage.end(),
          path.evidence_family_lineage.begin(),
          path.evidence_family_lineage.end());
      receipt.selected_evidence.derivation_lineage.insert(
          receipt.selected_evidence.derivation_lineage.end(),
          path.derivation_lineage.begin(), path.derivation_lineage.end());
      if (!path.verifier_version.empty()) {
        receipt.selected_evidence.verifier_versions.push_back(
            path.verifier_version);
      }
    }
  }

  sort_unique(&receipt.selected_evidence.selected_path_ids);
  sort_unique(&receipt.selected_evidence.source_lineage);
  sort_unique(&receipt.selected_evidence.evidence_family_lineage);
  sort_unique(&receipt.selected_evidence.derivation_lineage);
  sort_unique(&receipt.selected_evidence.verifier_versions);

  uint64_t hash = 1469598103934665603ULL;
  hash = append_hash(hash, receipt.schema_version);
  hash = append_hash(hash, receipt.snapshot_version);
  hash = append_hash(hash, receipt.bundle_hash);
  hash = append_hash(hash, receipt.primary_node);
  hash = append_hash(hash, static_cast<uint64_t>(receipt.status));
  hash = append_hash(
      hash, static_cast<uint64_t>(receipt.confidence * 1000000000.0));
  hash = append_hash(
      hash, static_cast<uint64_t>(receipt.lyapunov_energy * 1000000000.0));
  hash = append_hash(
      hash, static_cast<uint64_t>(receipt.semantic_verification));
  for (uint32_t edge : receipt.evidence_edges) hash = append_hash(hash, edge);
  for (uint64_t path_id : receipt.selected_evidence.selected_path_ids) {
    hash = append_hash(hash, path_id);
  }
  hash = append_hash(
      hash, receipt.selected_evidence.independent_evidence_family_count);
  for (const auto& value : receipt.selected_evidence.source_lineage) {
    append_text(&hash, value);
  }
  for (const auto& value : receipt.selected_evidence.evidence_family_lineage) {
    append_text(&hash, value);
  }
  for (const auto& value : receipt.selected_evidence.derivation_lineage) {
    append_text(&hash, value);
  }
  for (const auto& value : receipt.selected_evidence.verifier_versions) {
    append_text(&hash, value);
  }
  for (const auto& event : receipt.epistemic_events) {
    hash = append_hash(hash, event.sequence);
    hash = append_hash(hash, event.round_index);
    hash = append_hash(hash, static_cast<uint64_t>(event.source));
    hash = append_hash(hash, static_cast<uint64_t>(event.type));
    hash = append_hash(hash, event.previous_hypothesis_node);
    hash = append_hash(hash, event.hypothesis_node);
    for (uint32_t node : event.competing_hypotheses) {
      hash = append_hash(hash, node);
    }
    for (uint32_t node : event.reopen_nodes) {
      hash = append_hash(hash, node);
    }
    for (uint32_t edge : event.evidence_edges) {
      hash = append_hash(hash, edge);
    }
    for (const auto& family : event.evidence_family_ids) {
      append_text(&hash, family);
    }
    append_text(&hash, event.epistemic_state);
    append_text(&hash, event.reason);
  }
  for (const auto& value : receipt.residual_uncertainty) {
    append_text(&hash, value);
  }
  receipt.content_hash = hash;
  return receipt;
}

}  // namespace graphene
