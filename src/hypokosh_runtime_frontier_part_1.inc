#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>
#include <set>

namespace graphene {
namespace {

bool has_action(const EscapePlan& plan, EscapeAction action) {
  return std::any_of(plan.tasks.begin(), plan.tasks.end(),
                     [&](const EscapeTask& task) {
    return task.action == action;
  });
}

bool has_searchable_action(const EscapePlan& plan) {
  return has_action(plan, EscapeAction::GenerateMissingEvidenceQuery) ||
         has_action(plan, EscapeAction::SearchTemporalNeighbour) ||
         has_action(plan, EscapeAction::SeekIndependentEvidence) ||
         has_action(plan, EscapeAction::SearchContradiction) ||
         has_action(plan, EscapeAction::ExpandMinorityPath) ||
         has_action(plan, EscapeAction::PruneRetrievalNoise) ||
         has_action(plan, EscapeAction::ExploreAnalogy);
}

bool depth_repair_active(const EscapePlan& plan) {
  return has_action(plan, EscapeAction::GenerateMissingEvidenceQuery) ||
         has_action(plan, EscapeAction::SearchTemporalNeighbour);
}

DialecticOptions targeted_options(DialecticOptions options,
                                  const EscapePlan& plan) {
  // Defect-specific precedence: when the active defect is an incomplete
  // causal frontier, repair depth first. Do not simultaneously broaden
  // semantic candidate discovery or path budgets; doing so changes the
  // retrieval problem and can surface deep roots before the bounded frontier
  // has actually traversed the missing hops.
  if (depth_repair_active(plan)) {
    options.max_hops = std::min<uint32_t>(16, options.max_hops + 1);
    options.max_visited_states =
        std::min<size_t>(200000,
                         std::max<size_t>(2, options.max_visited_states * 2));
    return options;
  }
  if (has_action(plan, EscapeAction::SeekIndependentEvidence)) {
    options.semantic_candidates =
        std::min<size_t>(64, options.semantic_candidates + 4);
    options.max_paths =
        std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
  }
  if (has_action(plan, EscapeAction::SearchContradiction) ||
      has_action(plan, EscapeAction::ExpandMinorityPath)) {
    options.max_paths_per_root =
        std::min<size_t>(64, options.max_paths_per_root + 4);
    options.max_paths =
        std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
  }
  if (has_action(plan, EscapeAction::PruneRetrievalNoise)) {
    options.minimum_confidence =
        std::min(0.95, options.minimum_confidence + 0.05);
  }
  if (has_searchable_action(plan)) {
    options.max_visited_states =
        std::min<size_t>(200000,
                         std::max<size_t>(2, options.max_visited_states * 2));
  }
  return options;
}

DialecticOptions opposition_options(DialecticOptions options,
                                    const OppositionReport& opposition) {
  if (!opposition.requests_reexpansion || opposition.reopen_nodes.empty()) {
    return options;
  }
  options.reopen_nodes = opposition.reopen_nodes;
  options.semantic_candidates = std::min<size_t>(
      64, options.semantic_candidates +
              std::min<size_t>(4, opposition.reopen_nodes.size()));
  options.max_paths =
      std::min<size_t>(256, std::max<size_t>(2, options.max_paths * 2));
  options.max_paths_per_root = std::min<size_t>(
      64, options.max_paths_per_root +
              std::min<size_t>(4, opposition.reopen_nodes.size()));
  options.max_visited_states =
      std::min<size_t>(200000,
                       std::max<size_t>(2, options.max_visited_states * 2));
  return options;
}

bool options_changed(const DialecticOptions& left,
                     const DialecticOptions& right) {
  return left.semantic_candidates != right.semantic_candidates ||
         left.max_hops != right.max_hops ||
         left.max_paths != right.max_paths ||
         left.max_paths_per_root != right.max_paths_per_root ||
         left.max_visited_states != right.max_visited_states ||
         left.minimum_confidence != right.minimum_confidence ||
         left.reopen_nodes != right.reopen_nodes;
}

uint64_t append_state_hash(uint64_t hash, uint64_t value) {
  for (int shift = 0; shift < 8; ++shift) {
    hash ^= static_cast<unsigned char>((value >> (shift * 8)) & 0xffU);
    hash *= 1099511628211ULL;
  }
  return hash;
}

uint64_t reasoning_state_hash(const FiberBundle& bundle,
                              const DialecticOptions& options) {
  uint64_t hash = bundle.immutable_hash;
  hash = append_state_hash(hash, bundle.visited_states);
  hash = append_state_hash(hash, bundle.truncated ? 1U : 0U);
  hash = append_state_hash(hash, options.semantic_candidates);
  hash = append_state_hash(hash, options.max_hops);
  hash = append_state_hash(hash, options.max_paths);
  hash = append_state_hash(hash, options.max_paths_per_root);
  hash = append_state_hash(hash, options.max_visited_states);
  hash = append_state_hash(
      hash, static_cast<uint64_t>(options.minimum_confidence * 1000000.0));
  for (uint32_t node : options.reopen_nodes) {
    hash = append_state_hash(hash, node);
  }
  return hash;
}

std::vector<uint32_t> all_evidence_edges(
    const ConvergedAnswer& convergence) {
  std::vector<uint32_t> values = convergence.evidence_edges;
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
  return values;
}

template <typename T>
void sort_unique_values(std::vector<T>* values) {
  std::sort(values->begin(), values->end());
  values->erase(std::unique(values->begin(), values->end()), values->end());
}

std::vector<uint32_t> hypothesis_nodes(const FiberBundle& bundle) {
  std::vector<uint32_t> nodes;
  nodes.reserve(bundle.fibers.size());
  for (const auto& fiber : bundle.fibers) nodes.push_back(fiber.target_node);
  sort_unique_values(&nodes);
  return nodes;
}

std::vector<std::string> selected_evidence_families(
    const FiberBundle& bundle,
    const ConvergedAnswer& convergence) {
  std::vector<std::string> families;
  for (const auto& reference : convergence.selected_paths) {
    const auto fiber_it = std::find_if(
        bundle.fibers.begin(), bundle.fibers.end(),
        [&](const TargetFiber& fiber) {
          return fiber.target_node == reference.root_node;
        });
    if (fiber_it == bundle.fibers.end() ||
        reference.path_index >= fiber_it->paths.size()) {
      continue;
    }
    const auto& path = fiber_it->paths[reference.path_index];
    families.insert(families.end(), path.evidence_family_lineage.begin(),
                    path.evidence_family_lineage.end());
  }
  sort_unique_values(&families);
  return families;
}

std::vector<uint32_t> evidence_edges_for_nodes(
    const FiberBundle& bundle,
    const std::vector<uint32_t>& nodes) {
  std::vector<uint32_t> edges;
  for (const auto& fiber : bundle.fibers) {
    if (std::find(nodes.begin(), nodes.end(), fiber.target_node) ==
        nodes.end()) {
      continue;
    }
    for (const auto& path : fiber.paths) {
      edges.insert(edges.end(), path.edges.begin(), path.edges.end());
    }
  }
  sort_unique_values(&edges);
  return edges;
}

std::vector<std::string> evidence_families_for_nodes(
    const FiberBundle& bundle,
    const std::vector<uint32_t>& nodes) {
  std::vector<std::string> families;
  for (const auto& fiber : bundle.fibers) {
    if (std::find(nodes.begin(), nodes.end(), fiber.target_node) ==
        nodes.end()) {
      continue;
    }
    for (const auto& path : fiber.paths) {
      families.insert(families.end(), path.evidence_family_lineage.begin(),
                      path.evidence_family_lineage.end());
    }
  }
  sort_unique_values(&families);
  return families;
}

void append_epistemic_event(ReasoningReceipt* receipt,
                            NativeEpistemicEvent event) {
  event.sequence =
      static_cast<uint32_t>(receipt->epistemic_events.size() + 1);
  sort_unique_values(&event.competing_hypotheses);
  sort_unique_values(&event.reopen_nodes);
  sort_unique_values(&event.evidence_edges);
  sort_unique_values(&event.evidence_family_ids);
  receipt->epistemic_events.push_back(std::move(event));
}
