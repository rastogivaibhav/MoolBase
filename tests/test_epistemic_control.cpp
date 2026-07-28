#include "graphene/epistemic_control.hpp"

#include <cassert>
#include <iostream>

using namespace graphene;

namespace {

DialecticPath path(std::vector<uint32_t> edges,
                   const std::string& source,
                   double relevance = 1.0,
                   bool contradiction = false) {
  DialecticPath value;
  value.root_node = 1;
  value.anchor_node = 9;
  value.nodes = {1, 9};
  value.edges = std::move(edges);
  value.score = 0.95;
  value.query_relevance = relevance;
  value.target_consistency = relevance;
  value.completeness = 1.0;
  value.contains_contradiction = contradiction;
  value.evidence.push_back({source, "span", ""});
  return value;
}

FiberBundle bundle(std::vector<DialecticPath> paths) {
  BundleSet raw;
  raw.snapshot_version = 1;
  RootBundle root;
  root.root_node = 1;
  root.paths = std::move(paths);
  raw.roots.push_back(std::move(root));
  return FiberBundleBuilder().build(raw);
}

}  // namespace

int main() {
  LyapunovCritic critic;
  EpistemicController controller;

  const FiberBundle gold = bundle({path({1, 2}, "gold-source")});
  const StabilityAssessment gold_stability =
      critic.assess(gold, QueryMode::Empirical);
  const EpistemicAdmissibility gold_admissibility =
      controller.assess(gold, gold_stability, QueryMode::Empirical);
  assert(gold_admissibility.evidence_admissible);
  assert(!gold_admissibility.sufficient_independent_support);
  assert(gold_admissibility.requires_external_verification);
  const ConvergedAnswer gold_answer = controller.converge(
      gold, gold_admissibility, gold_stability);
  assert(gold_answer.has_answer);
  assert(gold_answer.primary_node == 1);

  const FiberBundle duplicate = bundle({
      path({1, 2}, "gold-source"),
      path({3, 4}, "gold-source")});
  const StabilityAssessment duplicate_stability =
      critic.assess(duplicate, QueryMode::Empirical);
  assert(duplicate.fibers.front().independent_path_count == 1);
  assert(duplicate_stability.lyapunov_energy + 1e-12 >=
         gold_stability.lyapunov_energy);

  DialecticPath noise = path({5, 6}, "noise-source", 0.0);
  noise.role_hint = PathRoleHint::Noise;
  const FiberBundle distracted = bundle({
      path({1, 2}, "gold-source"), noise});
  const StabilityAssessment distracted_stability =
      critic.assess(distracted, QueryMode::Empirical);
  assert(distracted_stability.retrieval_noise_penalty > 0.0);
  assert(distracted_stability.lyapunov_energy >
         gold_stability.lyapunov_energy);

  const FiberBundle contradicted = bundle({
      path({1, 2}, "gold-source"),
      path({7, 8}, "opposition-source", 1.0, true)});
  const StabilityAssessment contradiction_stability =
      critic.assess(contradicted, QueryMode::Empirical);
  const EpistemicAdmissibility contradiction_admissibility =
      controller.assess(contradicted, contradiction_stability,
                        QueryMode::Empirical);
  assert(contradiction_admissibility.contradiction_blocks_resolution);
  assert(!contradiction_admissibility.evidence_admissible);
  assert(contradiction_stability.lyapunov_energy >
         gold_stability.lyapunov_energy);
  const OppositionReport opposition = controller.oppose(
      contradicted,
      controller.converge(contradicted, contradiction_admissibility,
                          contradiction_stability),
      contradiction_admissibility, contradiction_stability);
  assert(opposition.opposition_score > 0.0);
  assert(opposition.requests_reexpansion);

  std::cout << "epistemic_control_contract_passed=true\n";
  return 0;
}
