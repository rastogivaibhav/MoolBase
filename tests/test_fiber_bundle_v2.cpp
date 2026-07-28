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
                   bool contradiction = false) {
  DialecticPath value;
  value.root_node = root;
  value.anchor_node = 99;
  value.nodes = {root, 99};
  value.edges = std::move(edges);
  value.score = 0.90;
  value.query_relevance = relevance;
  value.target_consistency = relevance;
  value.contains_contradiction = contradiction;
  value.evidence.push_back({source, "span", ""});
  return value;
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

  BundleSet independent_raw;
  independent_raw.snapshot_version = 12;
  RootBundle independent_root;
  independent_root.root_node = 1;
  independent_root.paths.push_back(path(1, {1, 2}, "source-a"));
  independent_root.paths.push_back(path(1, {3, 4}, "source-b"));
  independent_raw.roots.push_back(independent_root);
  const FiberBundle independent = builder.build(independent_raw);
  assert(independent.fibers.front().independent_evidence_family_count == 2);
  assert(independent.fibers.front().relevant_route_diversity > 0.90);

  BundleSet noise_raw = independent_raw;
  noise_raw.snapshot_version = 13;
  DialecticPath noise = path(1, {7, 8}, "noise-source", 0.0);
  noise.role_hint = PathRoleHint::Noise;
  noise_raw.roots.front().paths.push_back(noise);
  const FiberBundle noisy = builder.build(noise_raw);
  assert(noisy.fibers.front().noise_path_count == 1);
  assert(noisy.fibers.front().independent_evidence_family_count == 2);
  assert(noisy.fibers.front().retrieval_noise_ratio > 0.0);

  BundleSet contradiction_raw = independent_raw;
  contradiction_raw.snapshot_version = 14;
  contradiction_raw.roots.front().paths.push_back(
      path(1, {9, 10}, "opposition-source", 1.0, true));
  const FiberBundle contradicted = builder.build(contradiction_raw);
  assert(contradicted.fibers.front().contradiction_mass > 0.0);

  BundleSet reordered = independent_raw;
  std::reverse(reordered.roots.front().paths.begin(),
               reordered.roots.front().paths.end());
  assert(builder.build(reordered).immutable_hash ==
         independent.immutable_hash);

  std::cout << "fiber_bundle_v2_contract_passed=true\n";
  return 0;
}
