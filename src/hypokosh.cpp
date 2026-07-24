#include "graphene/hypokosh.hpp"

#include <algorithm>
#include <set>

namespace graphene {
namespace {

template <typename T>
void append_unique(std::vector<T>* output, const T& value) {
  if (std::find(output->begin(), output->end(), value) == output->end()) {
    output->push_back(value);
  }
}

}  // namespace

HypoKoshEngine::HypoKoshEngine(const GrapheneDB& db) : db_(db) {}

HypothesisSet HypoKoshEngine::propose(
    const std::vector<float>& query,
    uint64_t query_signature,
    const DialecticOptions& options,
    size_t max_hypotheses,
    uint64_t snapshot_version) const {
  HypothesisSet output;
  max_hypotheses = std::clamp<size_t>(max_hypotheses, 1, 16);
  const DialecticResult dialectic =
      DialecticEngine(db_).reason(query, query_signature, options,
                                 snapshot_version);
  const BundleSet& bundle = dialectic.has_reopened_bundle
                                ? dialectic.reopened_bundle
                                : dialectic.initial_bundle;
  output.snapshot_version = bundle.snapshot_version;
  output.epistemic_status = dialectic.synthesis.epistemic_status;
  output.discriminating_tests =
      dialectic.final_opposition.falsification_questions;
  output.warnings = bundle.warnings;

  for (const RootBundle& root : bundle.roots) {
    if (output.proposals.size() >= max_hypotheses) break;
    const auto node = db_.get_node(root.root_node, bundle.snapshot_version);
    if (!node) continue;
    HypothesisProposal proposal;
    proposal.root_node = root.root_node;
    proposal.statement = node->content;
    // A newly generated candidate remains a hypothesis even when the
    // underlying evidence nodes are observed. Truth promotion requires a
    // separate governed evidence operation.
    proposal.proposal_origin = EdgeOrigin::Hypothetical;
    proposal.plausibility = std::clamp(root.confidence, 0.0, 1.0);
    std::set<uint32_t> evidence;
    for (const DialecticPath& path : root.paths) {
      evidence.insert(path.edges.begin(), path.edges.end());
    }
    proposal.evidence_edges.assign(evidence.begin(), evidence.end());
    proposal.discriminating_tests = output.discriminating_tests;
    proposal.eligible_for_truth_promotion = false;
    output.proposals.push_back(std::move(proposal));
  }

  // No causal path is itself useful information. Surface bounded semantic
  // candidates as explicitly unsupported hypotheses rather than pretending
  // semantic similarity established a cause.
  if (output.proposals.empty()) {
    for (uint32_t node_id : bundle.semantic_candidates) {
      if (output.proposals.size() >= max_hypotheses) break;
      const auto node = db_.get_node(node_id, bundle.snapshot_version);
      if (!node) continue;
      HypothesisProposal proposal;
      proposal.root_node = node_id;
      proposal.statement = node->content;
      proposal.proposal_origin = EdgeOrigin::Hypothetical;
      proposal.plausibility = 0.0;
      proposal.discriminating_tests = output.discriminating_tests;
      proposal.eligible_for_truth_promotion = false;
      output.proposals.push_back(std::move(proposal));
    }
    append_unique(&output.warnings,
                  std::string("NO_CAUSAL_EVIDENCE_HYPOTHESES_ONLY"));
  }
  output.durable_writes = false;
  return output;
}

}  // namespace graphene
