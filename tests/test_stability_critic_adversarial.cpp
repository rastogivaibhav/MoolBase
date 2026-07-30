#include "graphene/escape.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace graphene;

namespace {

DialecticPath make_path(uint32_t root, uint32_t anchor,
                        std::vector<uint32_t> edges,
                        const std::string& source,
                        double score,
                        bool temporal = true,
                        bool contradiction = false) {
  DialecticPath path;
  path.root_node = root;
  path.anchor_node = anchor;
  path.nodes = {root, anchor};
  path.edges = std::move(edges);
  path.score = score;
  path.temporal_consistent = temporal;
  path.contains_contradiction = contradiction;
  if (!source.empty()) path.evidence.push_back({source, "", ""});
  return path;
}

}  // namespace

int main() {
  FiberBundleBuilder builder;
  StabilityCriticV0 critic;

  BundleSet duplicated;
  duplicated.snapshot_version = 99;
  RootBundle duplicate_root;
  duplicate_root.root_node = 1;
  for (size_t i = 0; i < 100; ++i) {
    duplicate_root.paths.push_back(make_path(1, 9, {2, 3}, "same-source", 0.99));
  }
  duplicated.roots.push_back(duplicate_root);
  const FiberBundle deduped = builder.build(duplicated);
  assert(deduped.fibers.front().raw_path_count == 100);
  assert(deduped.fibers.front().paths.size() == 1);
  assert(deduped.fibers.front().independent_path_count == 1);
  const auto duplicate_assessment = critic.assess(deduped);
  assert(duplicate_assessment.pattern_lock_score > 0.70);
  assert(duplicate_assessment.degeneracy_score < 0.51);

  BundleSet contradictory;
  contradictory.snapshot_version = 100;
  RootBundle contradictory_root;
  contradictory_root.root_node = 2;
  contradictory_root.paths.push_back(make_path(2, 8, {10, 11}, "source-a", 0.90, true, false));
  contradictory_root.paths.push_back(make_path(2, 8, {12, 13}, "source-b", 0.88, true, true));
  contradictory.roots.push_back(contradictory_root);
  const FiberBundle contradiction_bundle = builder.build(contradictory);
  const auto contradiction_assessment = critic.assess(contradiction_bundle, QueryMode::Empirical);
  assert(std::abs(contradiction_assessment.contradiction_score - 0.5) < 1e-9);
  assert(contradiction_assessment.requires_opposition);

  BundleSet temporal;
  temporal.snapshot_version = 101;
  RootBundle temporal_root;
  temporal_root.root_node = 3;
  temporal_root.paths.push_back(make_path(3, 7, {20, 21}, "source-c", 0.90, false, false));
  temporal.roots.push_back(temporal_root);
  const auto temporal_assessment = critic.assess(builder.build(temporal), QueryMode::Empirical);
  assert(temporal_assessment.temporal_consistency == 0.0);
  assert(temporal_assessment.requires_abstention);

  StabilityWeights provenance_heavy;
  provenance_heavy.provenance = 0.80;
  StabilityWeights diversity_heavy;
  diversity_heavy.diversity = 0.80;
  const auto first = critic.assess(contradiction_bundle, QueryMode::Balanced, provenance_heavy);
  const auto repeated = critic.assess(contradiction_bundle, QueryMode::Balanced, provenance_heavy);
  const auto alternate = critic.assess(contradiction_bundle, QueryMode::Balanced, diversity_heavy);
  assert(first.total_score == repeated.total_score);
  assert(first.reasons == repeated.reasons);
  assert(std::isfinite(alternate.total_score));

  CorrectiveEscape escape;
  const auto plan = escape.plan(contradiction_bundle, contradiction_assessment,
                                QueryMode::Theoretical);
  bool found_contradiction = false;
  bool found_falsification = false;
  for (const auto& task : plan.tasks) {
    found_contradiction = found_contradiction ||
                          task.action == EscapeAction::SearchContradiction;
    found_falsification = found_falsification ||
                          task.action == EscapeAction::GenerateFalsificationQuestion;
  }
  assert(found_contradiction);
  assert(found_falsification);

  std::cout << "stability_critic_adversarial_contract_passed=true\n";
  return 0;
}
