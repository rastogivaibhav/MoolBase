#include "graphene/epistemic_control.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
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
                       SemanticVerificationStatus::Unverified,
                   const std::string& family = {},
                   const std::string& derivation = {}) {
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
  value.evidence.push_back(
      {source, "span", "", family, derivation});
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

  // Target-level belief strength combines independent support groups rather
  // than selecting the single strongest path. Two verified independent
  // families may therefore outrank one slightly stronger unverified path.
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
         SemanticVerificationStatus::Verified);
  assert(multi_admissibility.sufficient_independent_support);
  assert(!multi_admissibility.requires_external_verification);
  const ConvergedAnswer multi_answer = controller.converge(
      multi_target, multi_admissibility, multi_stability);
  assert(multi_answer.has_answer);
  assert(multi_answer.primary_node == 2);
  const OppositionReport multi_opposition = controller.oppose(
      multi_target, multi_answer, multi_admissibility, multi_stability);
  assert(multi_opposition.opposition_score > 0.0);
  assert(multi_opposition.requests_reexpansion);
  assert(!multi_opposition.challenged_claims.empty());

  // R1: a materially contradicted single-path H1 must not remain primary
  // over a better independently corroborated H2.
  BundleSet r1_raw;
  r1_raw.snapshot_version = 30;
  RootBundle r1_h1;
  r1_h1.root_node = 1;
  r1_h1.paths.push_back(
      path(1, {101}, "h1-support", 1.0, false, 0.99,
           SemanticVerificationStatus::Verified, "h1-support-family"));
  r1_h1.paths.push_back(
      path(1, {102}, "h1-falsifier", 1.0, true, 0.95,
           SemanticVerificationStatus::Contradicted, "h1-opposition-family"));
  RootBundle r1_h2;
  r1_h2.root_node = 2;
  r1_h2.paths.push_back(
      path(2, {201}, "h2-a", 1.0, false, 0.94,
           SemanticVerificationStatus::Verified, "h2-family-a"));
  r1_h2.paths.push_back(
      path(2, {202}, "h2-b", 1.0, false, 0.93,
           SemanticVerificationStatus::Verified, "h2-family-b"));
  r1_raw.roots = {r1_h1, r1_h2};
  const FiberBundle r1_bundle = FiberBundleBuilder().build(r1_raw);
  const StabilityAssessment r1_stability =
      critic.assess(r1_bundle, QueryMode::Empirical);
  const EpistemicAdmissibility r1_admissibility =
      controller.assess(r1_bundle, r1_stability, QueryMode::Empirical);
  const ConvergedAnswer r1_answer =
      controller.converge(r1_bundle, r1_admissibility, r1_stability);
  assert(r1_answer.has_answer);
  assert(r1_answer.primary_node == 2);
  assert(!r1_admissibility.contradiction_blocks_resolution);
  assert(r1_admissibility.unresolved_contradiction == 0.0);
  assert(r1_admissibility.sufficient_independent_support);
  assert(r1_admissibility.evidence_admissible);

  // R2: weak opposition attenuates but does not automatically dethrone H1.
  BundleSet r2_raw;
  r2_raw.snapshot_version = 31;
  RootBundle r2_h1;
  r2_h1.root_node = 1;
  r2_h1.paths.push_back(
      path(1, {301}, "r2-h1", 1.0, false, 0.99,
           SemanticVerificationStatus::Verified, "r2-h1-family"));
  r2_h1.paths.push_back(
      path(1, {302}, "r2-weak-opp", 1.0, true, 0.05,
           SemanticVerificationStatus::Contradicted, "r2-opp-family"));
  RootBundle r2_h2;
  r2_h2.root_node = 2;
  r2_h2.paths.push_back(
      path(2, {303}, "r2-h2", 1.0, false, 0.90,
           SemanticVerificationStatus::Verified, "r2-h2-family"));
  r2_raw.roots = {r2_h1, r2_h2};
  const FiberBundle r2_bundle = FiberBundleBuilder().build(r2_raw);
  const StabilityAssessment r2_stability =
      critic.assess(r2_bundle, QueryMode::Empirical);
  const EpistemicAdmissibility r2_admissibility =
      controller.assess(r2_bundle, r2_stability, QueryMode::Empirical);
  const ConvergedAnswer r2_answer =
      controller.converge(r2_bundle, r2_admissibility, r2_stability);
  assert(r2_answer.primary_node == 1);

  // R3: duplicated opposition from one family must not multiply falsification.
  BundleSet r3_single = r2_raw;
  r3_single.snapshot_version = 32;
  r3_single.roots.front().paths[1] =
      path(1, {401}, "shared-opposition", 1.0, true, 0.40,
           SemanticVerificationStatus::Contradicted, "shared-opp-family");
  BundleSet r3_duplicate = r3_single;
  r3_duplicate.snapshot_version = 33;
  r3_duplicate.roots.front().paths.push_back(
      path(1, {402}, "shared-opposition", 1.0, true, 0.40,
           SemanticVerificationStatus::Contradicted, "shared-opp-family"));
  const FiberBundle r3_single_bundle = FiberBundleBuilder().build(r3_single);
  const FiberBundle r3_duplicate_bundle =
      FiberBundleBuilder().build(r3_duplicate);
  const auto r3_single_stability =
      critic.assess(r3_single_bundle, QueryMode::Empirical);
  const auto r3_dup_stability =
      critic.assess(r3_duplicate_bundle, QueryMode::Empirical);
  const auto r3_single_admissibility =
      controller.assess(r3_single_bundle, r3_single_stability,
                        QueryMode::Empirical);
  const auto r3_dup_admissibility =
      controller.assess(r3_duplicate_bundle, r3_dup_stability,
                        QueryMode::Empirical);
  const auto r3_single_answer =
      controller.converge(r3_single_bundle, r3_single_admissibility,
                          r3_single_stability);
  const auto r3_dup_answer =
      controller.converge(r3_duplicate_bundle, r3_dup_admissibility,
                          r3_dup_stability);
  assert(r3_single_answer.primary_node == r3_dup_answer.primary_node);
  assert(std::abs(r3_single_answer.confidence - r3_dup_answer.confidence) <
         1e-12);

  // R4: duplicated/correlated H1 support cannot beat two independent H2
  // families merely by path count.
  BundleSet r4_raw;
  r4_raw.snapshot_version = 34;
  RootBundle r4_h1;
  r4_h1.root_node = 1;
  r4_h1.paths.push_back(
      path(1, {501}, "same-doc", 1.0, false, 0.96,
           SemanticVerificationStatus::Unverified, "same-family"));
  r4_h1.paths.push_back(
      path(1, {502}, "same-doc", 1.0, false, 0.95,
           SemanticVerificationStatus::Unverified, "same-family"));
  r4_h1.paths.push_back(
      path(1, {503}, "same-doc", 1.0, false, 0.94,
           SemanticVerificationStatus::Unverified, "same-family"));
  RootBundle r4_h2;
  r4_h2.root_node = 2;
  r4_h2.paths.push_back(
      path(2, {504}, "ind-a", 1.0, false, 0.82,
           SemanticVerificationStatus::Unverified, "ind-family-a"));
  r4_h2.paths.push_back(
      path(2, {505}, "ind-b", 1.0, false, 0.81,
           SemanticVerificationStatus::Unverified, "ind-family-b"));
  r4_raw.roots = {r4_h1, r4_h2};
  const FiberBundle r4_bundle = FiberBundleBuilder().build(r4_raw);
  const auto r4_stability = critic.assess(r4_bundle, QueryMode::Empirical);
  const auto r4_admissibility =
      controller.assess(r4_bundle, r4_stability, QueryMode::Empirical);
  const auto r4_answer =
      controller.converge(r4_bundle, r4_admissibility, r4_stability);
  assert(r4_answer.primary_node == 2);

  // R5: semantic verification is a deterministic tie-break when evidence
  // strength is otherwise equal.
  BundleSet r5_raw;
  r5_raw.snapshot_version = 35;
  RootBundle r5_h1;
  r5_h1.root_node = 1;
  r5_h1.paths.push_back(
      path(1, {601}, "verified", 1.0, false, 0.90,
           SemanticVerificationStatus::Verified, "r5-a"));
  RootBundle r5_h2;
  r5_h2.root_node = 2;
  r5_h2.paths.push_back(
      path(2, {602}, "unverified", 1.0, false, 0.90,
           SemanticVerificationStatus::Unverified, "r5-b"));
  r5_raw.roots = {r5_h2, r5_h1};
  const FiberBundle r5_bundle = FiberBundleBuilder().build(r5_raw);
  const auto r5_stability = critic.assess(r5_bundle, QueryMode::Empirical);
  const auto r5_admissibility =
      controller.assess(r5_bundle, r5_stability, QueryMode::Empirical);
  assert(controller.converge(r5_bundle, r5_admissibility, r5_stability)
             .primary_node == 1);

  // R5b: verification must remain the tie-break even when the verified target
  // has the higher deterministic target id. This prevents the corroboration
  // handoff safeguard from erasing the declared verification ordering.
  BundleSet r5b_raw;
  r5b_raw.snapshot_version = 351;
  RootBundle r5b_h1;
  r5b_h1.root_node = 1;
  r5b_h1.paths.push_back(
      path(1, {611}, "unverified-low-id", 1.0, false, 0.90,
           SemanticVerificationStatus::Unverified, "r5b-a"));
  RootBundle r5b_h2;
  r5b_h2.root_node = 2;
  r5b_h2.paths.push_back(
      path(2, {612}, "verified-high-id", 1.0, false, 0.90,
           SemanticVerificationStatus::Verified, "r5b-b"));
  r5b_raw.roots = {r5b_h1, r5b_h2};
  const FiberBundle r5b_bundle = FiberBundleBuilder().build(r5b_raw);
  const auto r5b_stability = critic.assess(r5b_bundle, QueryMode::Empirical);
  const auto r5b_admissibility =
      controller.assess(r5b_bundle, r5b_stability, QueryMode::Empirical);
  assert(controller.converge(
             r5b_bundle, r5b_admissibility, r5b_stability)
             .primary_node == 2);

  // R6: a better alternative can become operative without being silently
  // promoted to independently corroborated evidence.
  BundleSet r6_raw = r1_raw;
  r6_raw.snapshot_version = 36;
  r6_raw.roots[1].paths.resize(1);
  r6_raw.roots[1].paths.front() =
      path(2, {701}, "h2-only", 1.0, false, 0.60,
           SemanticVerificationStatus::Unverified, "h2-only-family");
  const FiberBundle r6_bundle = FiberBundleBuilder().build(r6_raw);
  const auto r6_stability = critic.assess(r6_bundle, QueryMode::Empirical);
  const auto r6_admissibility =
      controller.assess(r6_bundle, r6_stability, QueryMode::Empirical);
  const auto r6_answer =
      controller.converge(r6_bundle, r6_admissibility, r6_stability);
  assert(!r6_answer.has_answer);
  assert(r6_answer.primary_node == 0);
  assert(!r6_admissibility.sufficient_independent_support);
  assert(r6_admissibility.requires_external_verification);
  assert(r6_admissibility.contradiction_blocks_resolution);
  assert(!r6_admissibility.evidence_admissible);

  // R7: target ranking must be insertion-order invariant.
  BundleSet r7_reordered = r1_raw;
  std::reverse(r7_reordered.roots.begin(), r7_reordered.roots.end());
  for (auto& root_value : r7_reordered.roots) {
    std::reverse(root_value.paths.begin(), root_value.paths.end());
  }
  const FiberBundle r7_bundle = FiberBundleBuilder().build(r7_reordered);
  const auto r7_stability = critic.assess(r7_bundle, QueryMode::Empirical);
  const auto r7_admissibility =
      controller.assess(r7_bundle, r7_stability, QueryMode::Empirical);
  const auto r7_answer =
      controller.converge(r7_bundle, r7_admissibility, r7_stability);
  assert(r7_answer.primary_node == r1_answer.primary_node);
  assert(std::abs(r7_answer.confidence - r1_answer.confidence) < 1e-12);

  // R8: exact evidence ties are semantically unresolved. Numeric target id
  // must not manufacture an operative answer.
  BundleSet r8_raw;
  r8_raw.snapshot_version = 37;
  RootBundle r8_h2;
  r8_h2.root_node = 2;
  r8_h2.paths.push_back(
      path(2, {801}, "tie-b", 1.0, false, 0.90,
           SemanticVerificationStatus::Unverified, "tie-b"));
  RootBundle r8_h1;
  r8_h1.root_node = 1;
  r8_h1.paths.push_back(
      path(1, {802}, "tie-a", 1.0, false, 0.90,
           SemanticVerificationStatus::Unverified, "tie-a"));
  r8_raw.roots = {r8_h2, r8_h1};
  const FiberBundle r8_bundle = FiberBundleBuilder().build(r8_raw);
  const auto r8_stability = critic.assess(r8_bundle, QueryMode::Empirical);
  const auto r8_admissibility =
      controller.assess(r8_bundle, r8_stability, QueryMode::Empirical);
  const auto r8_answer =
      controller.converge(r8_bundle, r8_admissibility, r8_stability);
  assert(!r8_answer.has_answer);
  assert(r8_answer.primary_node == 0);

  // Swapping numeric target ids under otherwise identical evidence must still
  // produce no operative hypothesis.
  BundleSet r8_swapped = r8_raw;
  r8_swapped.snapshot_version = 371;
  r8_swapped.roots[0].root_node = 1;
  r8_swapped.roots[0].paths[0].root_node = 1;
  r8_swapped.roots[1].root_node = 2;
  r8_swapped.roots[1].paths[0].root_node = 2;
  const FiberBundle r8_swapped_bundle =
      FiberBundleBuilder().build(r8_swapped);
  const auto r8_swapped_stability =
      critic.assess(r8_swapped_bundle, QueryMode::Empirical);
  const auto r8_swapped_admissibility =
      controller.assess(r8_swapped_bundle, r8_swapped_stability,
                        QueryMode::Empirical);
  const auto r8_swapped_answer =
      controller.converge(r8_swapped_bundle, r8_swapped_admissibility,
                          r8_swapped_stability);
  assert(!r8_swapped_answer.has_answer);
  assert(r8_swapped_answer.primary_node == 0);

  std::cout << "epistemic_control_contract_passed=true\n";
  return 0;
}
