#include "graphene/path_verifier.hpp"
#include "graphene/fiber_bundle.hpp"

#include <cassert>
#include <iostream>

using namespace graphene;

namespace {

class NoiseVerifier final : public PathVerifier {
 public:
  PathVerificationResult verify(
      const DialecticPath&,
      const PathVerificationContext& context) const override {
    assert(context.query_vector != nullptr);
    assert(context.query_signature == 77);
    assert(context.target_node == 1);
    PathVerificationResult result;
    result.query_relevance = 0.10;
    result.target_consistency = 0.20;
    result.completeness = 0.80;
    result.role = PathRoleHint::Noise;
    result.semantic_verification =
        SemanticVerificationStatus::Unverified;
    result.verifier_version = "noise-verifier-v1";
    result.findings = {"path does not answer the requested question"};
    return result;
  }
};

}  // namespace

int main() {
  BundleSet raw;
  raw.snapshot_version = 12;
  RootBundle root;
  root.root_node = 1;
  DialecticPath path;
  path.root_node = 1;
  path.anchor_node = 9;
  path.nodes = {1, 9};
  path.edges = {3};
  path.score = 0.9;
  path.evidence.push_back({"source-a", "span", ""});
  root.paths.push_back(path);
  raw.roots.push_back(root);

  const std::vector<float> query{0.1f, 0.2f};
  NoiseVerifier verifier;
  apply_path_verifier(&raw, verifier, query, 77, QueryMode::Empirical);
  const DialecticPath& verified = raw.roots.front().paths.front();
  assert(verified.query_relevance == 0.10);
  assert(verified.target_consistency == 0.20);
  assert(verified.completeness == 0.80);
  assert(verified.role_hint == PathRoleHint::Noise);
  assert(verified.verifier_version == "noise-verifier-v1");
  assert(verified.verification_findings.size() == 1);
  assert(raw.warnings.size() == 1);
  assert(raw.warnings.front() ==
         "PATH_VERIFIER_VERSION:noise-verifier-v1");

  const FiberBundle bundle = FiberBundleBuilder().build(raw);
  const FiberPath& bundled = bundle.fibers.front().paths.front();
  assert(bundled.role == FiberPathRole::Noise);
  assert(bundled.verifier_version == "noise-verifier-v1");
  assert(bundled.verification_findings.size() == 1);
  assert(!bundled.eligible_for_support);
  assert(bundle.fibers.front().noise_path_count == 1);

  std::cout << "path_verifier_contract_passed=true\n";
  return 0;
}
