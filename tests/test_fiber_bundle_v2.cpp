#include "graphene/fiber_bundle.hpp"

#include <algorithm>
#include <cassert>
#include <iostream>

using namespace graphene;

namespace {

DialecticPath path(uint32_t root,
                   std::vector<uint32_t> edges,
                   const std::string& source,
                   double relevance = 1.0,
                   bool contradiction = false,
                   const std::string& family = {}) {
  DialecticPath value;
  value.root_node = root;
  value.anchor_node = 99;
  value.nodes = {root, 99};
  value.edges = std::move(edges);
  value.score = 0.90;
  value.query_relevance = relevance;
  value.target_consistency = relevance;
  value.contains_contradiction = contradiction;
  value.evidence.push_back({source, "span", "", family});
  return value;
}

FiberBundle build_one(const DialecticPath& value, uint64_t snapshot) {
  BundleSet raw;
  raw.snapshot_version = snapshot;
  RootBundle root;
  root.root_node = value.root_node;
  root.paths.push_back(value);
  raw.roots.push_back(std::move(root));
  return FiberBundleBuilder().build(raw);
}

}  // namespace

int main() {
  FiberBundleBuilder builder;

  BundleSet correlated_raw;
  correlated_raw.snapshot_version = 11;
  RootBundle correlated_root;
  correlated_root.root_node = 1;
  correlated_root.paths.push_back(path(1, {1, 2}, "same-source"));
  correlated_root.paths.push_back(path(1, {3, 4}, "same-source"));
  correlated_raw.roots.push_back(correlated_root);
  const FiberBundle correlated = builder.build(correlated_raw);
  const TargetFiber& correlated_fiber = correlated.fibers.front();
  assert(correlated_fiber.paths.size() == 2);
  assert(correlated_fiber.unique_route_count == 2);
  assert(correlated_fiber.independent_evidence_family_count == 1);
  assert(correlated_fiber.independent_path_count == 1);
  assert(correlated_fiber.relevant_route_diversity == 0.0);
  assert(correlated_fiber.correlation_groups.size() == 1);
  assert(correlated_fiber.invalid_path_count == 0);
  assert(correlated_fiber.paths.front().validity.graph_continuous);
  assert(correlated_fiber.paths.front().validity.reaches_target);
  assert(correlated_fiber.paths.front().validity.every_critical_edge_has_evidence);
  assert(correlated_fiber.paths.front().validity.completeness_score == 1.0);

  // Explicit family labels cannot split one underlying source into fabricated
  // independent evidence. Source ancestry remains a correlation boundary.
  BundleSet family_split_raw;
  family_split_raw.snapshot_version = 12;
  RootBundle family_split_root;
  family_split_root.root_node = 1;
  family_split_root.paths.push_back(
      path(1, {5, 6}, "shared-document", 1.0, false, "family-a"));
  family_split_root.paths.push_back(
      path(1, {7, 8}, "shared-document", 1.0, false, "family-b"));
  family_split_raw.roots.push_back(std::move(family_split_root));
  const FiberBundle family_split = builder.build(family_split_raw);
  assert(family_split.fibers.front().paths.size() == 2);
  assert(family_split.fibers.front().correlation_groups.size() == 1);
  assert(family_split.fibers.front().independent_evidence_family_count == 1);

  BundleSet independent_raw;
  independent_raw.snapshot_version = 13;
  RootBundle independent_root;
  independent_root.root_node = 1;
  independent_root.paths.push_back(path(1, {1, 2}, "source-a"));
  independent_root.paths.push_back(path(1, {3, 4}, "source-b"));
  independent_raw.roots.push_back(independent_root);
  const FiberBundle independent = builder.build(independent_raw);
  assert(independent.fibers.front().independent_evidence_family_count == 2);
  assert(independent.fibers.front().relevant_route_diversity > 0.90);

  BundleSet noise_raw = independent_raw;
  noise_raw.snapshot_version = 14;
  DialecticPath noise = path(1, {7, 8}, "noise-source", 0.0);
  noise.role_hint = PathRoleHint::Noise;
  noise_raw.roots.front().paths.push_back(noise);
  const FiberBundle noisy = builder.build(noise_raw);
  assert(noisy.fibers.front().noise_path_count == 1);
  assert(noisy.fibers.front().independent_evidence_family_count == 2);
  assert(noisy.fibers.front().retrieval_noise_ratio > 0.0);

  BundleSet contradiction_raw = independent_raw;
  contradiction_raw.snapshot_version = 15;
  contradiction_raw.roots.front().paths.push_back(
      path(1, {9, 10}, "opposition-source", 1.0, true));
  const FiberBundle contradicted = builder.build(contradiction_raw);
  assert(contradicted.fibers.front().contradiction_mass > 0.0);

  // A contradiction citing the same source family as support remains a
  // separate opposition group. Correlation must not silently hide its role.
  BundleSet shared_opposition_raw;
  shared_opposition_raw.snapshot_version = 16;
  RootBundle shared_opposition_root;
  shared_opposition_root.root_node = 1;
  shared_opposition_root.paths.push_back(
      path(1, {11, 12}, "shared-source"));
  shared_opposition_root.paths.push_back(
      path(1, {13, 14}, "shared-source", 1.0, true));
  shared_opposition_raw.roots.push_back(shared_opposition_root);
  const FiberBundle shared_opposition =
      builder.build(shared_opposition_raw);
  const TargetFiber& shared_fiber = shared_opposition.fibers.front();
  assert(shared_fiber.correlation_groups.size() == 2);
  assert(shared_fiber.independent_evidence_family_count == 1);
  assert(shared_fiber.contradiction_mass > 0.0);
  assert(std::any_of(
      shared_fiber.correlation_groups.begin(),
      shared_fiber.correlation_groups.end(),
      [](const EvidenceCorrelationGroup& group) {
        return group.role == FiberPathRole::Opposition;
      }));

  // Edge-level evidence is mandatory. A sourced-looking path with an explicit
  // missing-evidence finding cannot count as independent support.
  DialecticPath missing_evidence = path(1, {20, 21}, "partial-source");
  missing_evidence.provenance_findings.push_back(
      {21, "MISSING_EVIDENCE", "critical bridge has no evidence"});
  const FiberBundle unsupported = build_one(missing_evidence, 17);
  const FiberPath& unsupported_path = unsupported.fibers.front().paths.front();
  assert(!unsupported_path.validity.every_critical_edge_has_evidence);
  assert(unsupported_path.validity.critical_edge_coverage == 0.5);
  assert(!unsupported_path.eligible_for_support);
  assert(unsupported.fibers.front().independent_evidence_family_count == 0);
  assert(unsupported.fibers.front().invalid_path_count == 1);

  // Hyperedge paths remain invalid until every declared source/member is
  // present in the path.
  DialecticPath incomplete_joint = path(1, {30, 31}, "joint-source");
  JointRequirement requirement;
  requirement.hyperedge_id = "joint-1";
  requirement.target_node = 1;
  requirement.source_nodes = {2, 3};
  requirement.member_edges = {30, 32};
  requirement.all_sources_present = false;
  incomplete_joint.joint_requirements.push_back(requirement);
  const FiberBundle joint_bundle = build_one(incomplete_joint, 18);
  const FiberPath& joint_path = joint_bundle.fibers.front().paths.front();
  assert(!joint_path.validity.satisfies_joint_requirements);
  assert(joint_path.validity.completeness_score == 0.0);
  assert(!joint_path.eligible_for_support);

  // Obvious chain discontinuity is recorded rather than silently accepted.
  DialecticPath broken = path(1, {40}, "broken-source");
  broken.nodes = {7, 99};
  const FiberBundle broken_bundle = build_one(broken, 19);
  const FiberPath& broken_path = broken_bundle.fibers.front().paths.front();
  assert(!broken_path.validity.reaches_target);
  assert(!broken_path.validity.graph_continuous);
  assert(!broken_path.eligible_for_support);

  BundleSet reordered = independent_raw;
  std::reverse(reordered.roots.front().paths.begin(),
               reordered.roots.front().paths.end());
  assert(builder.build(reordered).immutable_hash ==
         independent.immutable_hash);

  // Exact route/evidence duplicates are merged conservatively. Conflicting
  // verifier outcomes cannot become order-dependent or retain the optimistic
  // result merely because it appeared first.
  DialecticPath optimistic =
      path(1, {50, 51}, "duplicate-source", 0.95, false, "duplicate-family");
  optimistic.semantic_verification = SemanticVerificationStatus::Verified;
  optimistic.verifier_version = "verifier-v1";
  DialecticPath cautious =
      path(1, {50, 51}, "duplicate-source", 0.70, false, "duplicate-family");
  cautious.semantic_verification = SemanticVerificationStatus::Unverified;
  cautious.verifier_version = "verifier-v2";

  BundleSet duplicate_forward;
  duplicate_forward.snapshot_version = 20;
  RootBundle duplicate_root;
  duplicate_root.root_node = 1;
  duplicate_root.paths = {optimistic, cautious};
  duplicate_forward.roots.push_back(duplicate_root);
  BundleSet duplicate_reverse = duplicate_forward;
  std::reverse(duplicate_reverse.roots.front().paths.begin(),
               duplicate_reverse.roots.front().paths.end());
  const FiberBundle merged_forward = builder.build(duplicate_forward);
  const FiberBundle merged_reverse = builder.build(duplicate_reverse);
  assert(merged_forward.fibers.front().paths.size() == 1);
  assert(merged_forward.immutable_hash == merged_reverse.immutable_hash);
  const FiberPath& merged = merged_forward.fibers.front().paths.front();
  assert(merged.query_relevance == 0.70);
  assert(merged.target_consistency == 0.70);
  assert(merged.semantic_verification ==
         SemanticVerificationStatus::Unverified);
  assert(merged.verifier_version == "mixed");
  assert(std::find(
      merged.verification_findings.begin(),
      merged.verification_findings.end(),
      "exact duplicate received results from different verifier versions") !=
      merged.verification_findings.end());

  // Verifier-state changes alter the immutable bundle receipt even when route
  // and evidence identity remain the same.
  BundleSet lower_relevance = independent_raw;
  lower_relevance.roots.front().paths.front().query_relevance = 0.75;
  assert(builder.build(lower_relevance).immutable_hash !=
         independent.immutable_hash);

  std::cout << "fiber_bundle_v2_contract_passed=true\n";
  return 0;
}
