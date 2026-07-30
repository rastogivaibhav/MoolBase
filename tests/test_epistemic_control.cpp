#include "graphene/epistemic_control.hpp"

#include <cassert>
#include <iostream>

using namespace graphene;

namespace {

DialecticPath path(uint32_t root,
                   std::vector<uint32_t> edges,
                   const std::string& source,
                   double relevance = 1.0,
                   bool contradiction = false,
                   double score = 0.95,
                   SemanticVerificationStatus verification =
                       SemanticVerificationStatus::Unverified) {
  DialecticPath value;
  value.root_node = root;
  value.anchor_node = 9;
  value.nodes = {root, 9};
  value.edges = std::move(edges);
  value.score = score;
  value.query_relevance = relevance;
  value.target_consistency = relevance;
  value.completeness = 1.0;
  value.contains_contradiction = contradiction;
  value.semantic_verification = verification;
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

  const FiberBundle gold = bundle({path(1, {1, 2}, "gold-source")});
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
      path(1, {1, 2}, "gold-source"),
      path(1, {3, 4}, "gold-source")});
  const StabilityAssessment duplicate_stability =
      critic.assess(duplicate, QueryMode::Empirical);
  assert(duplicate.fibers.front().independent_path_count == 1);
  assert(duplicate_stability.lyapunov_energy + 1e-12 >=
         gold_stability.lyapunov_energy);

  DialecticPath noise = path(1, {5, 6}, "noise-source", 0.0);
  noise.role_hint = PathRoleHint::Noise;
  const FiberBundle distracted = bundle({
      path(1, {1, 2}, "gold-source"), noise});
  const StabilityAssessment distracted_stability =
      critic.assess(distracted, QueryMode::Empirical);
  assert(distracted_stability.retrieval_noise_penalty > 0.0);
  assert(distracted_stability.lyapunov_energy >
         gold_stability.lyapunov_energy);

  const FiberBundle contradicted = bundle({
      path(1, {1, 2}, "gold-source"),
      path(1, {7, 8}, "opposition-source", 1.0, true)});
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

  // A verified secondary target must not promote a stronger but unverified
  // primary target. Its independent support is preserved as opposition.
  BundleSet multi_target_raw;
  multi_target_raw.snapshot_version = 2;
  RootBundle primary_root;
  primary_root.root_node = 1;
  primary_root.paths.push_back(
      path(1, {20, 21}, "primary-source", 1.0, false, 0.97));
  RootBundle alternative_root;
  alternative_root.root_node = 2;
  alternative_root.paths.push_back(
      path(2, {30, 31}, "alternative-a", 1.0, false, 0.85,
           SemanticVerificationStatus::Verified));
  alternative_root.paths.push_back(
      path(2, {32, 33}, "alternative-b", 1.0, false, 0.84,
           SemanticVerificationStatus::Verified));
  multi_target_raw.roots.push_back(std::move(primary_root));
  multi_target_raw.roots.push_back(std::move(alternative_root));
  const FiberBundle multi_target = FiberBundleBuilder().build(multi_target_raw);
  const StabilityAssessment multi_stability =
      critic.assess(multi_target, QueryMode::Empirical);
  const EpistemicAdmissibility multi_admissibility =
      controller.assess(multi_target, multi_stability, QueryMode::Empirical);
  assert(multi_admissibility.semantic_verification ==
         SemanticVerificationStatus::Unverified);
  assert(!multi_admissibility.sufficient_independent_support);
  assert(multi_admissibility.requires_external_verification);
  const ConvergedAnswer multi_answer = controller.converge(
      multi_target, multi_admissibility, multi_stability);
  assert(multi_answer.has_answer);
  assert(multi_answer.primary_node == 1);
  const OppositionReport multi_opposition = controller.oppose(
      multi_target, multi_answer, multi_admissibility, multi_stability);
  assert(multi_opposition.opposition_score > 0.0);
  assert(multi_opposition.requests_reexpansion);
  assert(!multi_opposition.challenged_claims.empty());

  std::cout << "epistemic_control_contract_passed=true\n";
  return 0;
}
