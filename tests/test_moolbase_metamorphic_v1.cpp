#include "graphene/epistemic_control.hpp"
#include "graphene/fiber_bundle.hpp"
#include "graphene/stability_critic.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace graphene;

namespace {

DialecticPath path(uint32_t root,
                   uint32_t edge,
                   const std::string& source,
                   const std::string& family,
                   double score,
                   bool contradiction = false,
                   double relevance = 1.0,
                   SemanticVerificationStatus verification =
                       SemanticVerificationStatus::Unverified) {
  DialecticPath value;
  value.root_node = root;
  value.anchor_node = 999;
  value.nodes = {root, 999};
  value.edges = {edge};
  value.score = score;
  value.query_relevance = relevance;
  value.target_consistency = relevance;
  value.completeness = 1.0;
  value.temporal_consistent = true;
  value.contains_contradiction = contradiction;
  value.semantic_verification = verification;
  value.evidence.push_back({source, "span", "", family, "", ""});
  return value;
}

FiberBundle bundle(std::vector<RootBundle> roots, uint64_t snapshot = 1) {
  BundleSet raw;
  raw.snapshot_version = snapshot;
  raw.roots = std::move(roots);
  return FiberBundleBuilder().build(raw);
}

ConvergedAnswer converge(const FiberBundle& value) {
  LyapunovCritic critic;
  EpistemicController controller;
  const auto stability = critic.assess(value, QueryMode::Empirical);
  const auto admissibility =
      controller.assess(value, stability, QueryMode::Empirical);
  return controller.converge(value, admissibility, stability);
}

bool same_answer(const ConvergedAnswer& left, const ConvergedAnswer& right) {
  return left.has_answer == right.has_answer &&
         left.primary_node == right.primary_node &&
         std::abs(left.confidence - right.confidence) < 1e-12;
}

void require(bool value, const char* label) {
  if (!value) {
    std::cerr << "metamorphic_failure=" << label << "\n";
    std::exit(1);
  }
}

}  // namespace

int main() {
  RootBundle h1;
  h1.root_node = 1;
  h1.paths.push_back(path(1, 101, "a", "fa", 0.90));
  h1.paths.push_back(path(1, 102, "b", "fb", 0.88));
  RootBundle h2;
  h2.root_node = 2;
  h2.paths.push_back(path(2, 201, "c", "fc", 0.70));

  const FiberBundle base = bundle({h1, h2}, 100);
  const auto base_answer = converge(base);

  // M1: reorder roots and insertion.
  RootBundle m1_h1 = h1;
  RootBundle m1_h2 = h2;
  std::reverse(m1_h1.paths.begin(), m1_h1.paths.end());
  const auto m1 = converge(bundle({m1_h2, m1_h1}, 100));
  require(same_answer(base_answer, m1), "M1_reorder_insertion");

  // M2: duplicate support within the same family/source.
  RootBundle m2_h1 = h1;
  m2_h1.paths.push_back(path(1, 103, "a", "fa", 0.90));
  const auto m2 = converge(bundle({m2_h1, h2}, 100));
  require(same_answer(base_answer, m2), "M2_same_family_duplicate");

  // M3: change verifier notes that do not alter path coordinates.
  RootBundle m3_h1 = h1;
  m3_h1.paths.front().verification_findings = {"irrelevant-note-renamed"};
  const auto m3 = converge(bundle({m3_h1, h2}, 100));
  require(same_answer(base_answer, m3), "M3_irrelevant_metadata");

  // M4: add explicitly quarantined noise.
  RootBundle m4_h1 = h1;
  auto noise = path(1, 104, "noise", "noise-family", 1.0, true, 0.01);
  noise.role_hint = PathRoleHint::Noise;
  m4_h1.paths.push_back(noise);
  const auto m4 = converge(bundle({m4_h1, h2}, 100));
  require(same_answer(base_answer, m4), "M4_quarantined_noise");

  // M5: path order alone.
  RootBundle m5_h1 = h1;
  std::swap(m5_h1.paths[0], m5_h1.paths[1]);
  const auto m5 = converge(bundle({m5_h1, h2}, 100));
  require(same_answer(base_answer, m5), "M5_path_order");

  // M7: identical query/evidence evaluation twice.
  const auto m7a = converge(base);
  const auto m7b = converge(base);
  require(same_answer(m7a, m7b), "M7_repeat_query");

  // M8: independent corroboration should strengthen the selected target.
  RootBundle m8_h1;
  m8_h1.root_node = 1;
  m8_h1.paths.push_back(path(1, 301, "m8-a", "m8-fa", 0.70));
  const auto m8_before = converge(bundle({m8_h1}, 101));
  m8_h1.paths.push_back(path(1, 302, "m8-b", "m8-fb", 0.70));
  const auto m8_after = converge(bundle({m8_h1}, 101));
  require(m8_after.confidence > m8_before.confidence,
          "M8_independent_support_family");

  // M9: decisive independent contradiction must materially change belief
  // strength or operative target.
  RootBundle m9_h1;
  m9_h1.root_node = 1;
  m9_h1.paths.push_back(path(1, 401, "m9-a", "m9-fa", 0.95));
  RootBundle m9_h2;
  m9_h2.root_node = 2;
  m9_h2.paths.push_back(path(2, 402, "m9-b", "m9-fb", 0.80));
  const auto m9_before = converge(bundle({m9_h1, m9_h2}, 102));
  m9_h1.paths.push_back(path(
      1, 403, "m9-opp", "m9-opp-family", 0.95, true, 1.0,
      SemanticVerificationStatus::Contradicted));
  const auto m9_after = converge(bundle({m9_h1, m9_h2}, 102));
  require(
      m9_after.primary_node != m9_before.primary_node ||
          m9_after.confidence < m9_before.confidence,
      "M9_decisive_contradiction");

  // M10: verification breaks an otherwise equal evidence tie.
  RootBundle m10_h1;
  m10_h1.root_node = 1;
  m10_h1.paths.push_back(path(1, 501, "m10-a", "m10-fa", 0.90));
  RootBundle m10_h2;
  m10_h2.root_node = 2;
  m10_h2.paths.push_back(path(2, 502, "m10-b", "m10-fb", 0.90));
  const auto m10_before = converge(bundle({m10_h2, m10_h1}, 103));
  m10_h2.paths.front().semantic_verification =
      SemanticVerificationStatus::Verified;
  const auto m10_after = converge(bundle({m10_h2, m10_h1}, 103));
  require(m10_before.primary_node == 1 && m10_after.primary_node == 2,
          "M10_semantic_verification");

  // M11: stronger independently corroborated competitor must become operative.
  RootBundle m11_h1;
  m11_h1.root_node = 1;
  m11_h1.paths.push_back(path(1, 601, "m11-a", "m11-fa", 0.80));
  RootBundle m11_h2;
  m11_h2.root_node = 2;
  m11_h2.paths.push_back(path(2, 602, "m11-b", "m11-fb", 0.50));
  const auto m11_before = converge(bundle({m11_h1, m11_h2}, 104));
  m11_h2.paths.push_back(path(2, 603, "m11-c", "m11-fc", 0.90));
  const auto m11_after = converge(bundle({m11_h1, m11_h2}, 104));
  require(m11_before.primary_node == 1 && m11_after.primary_node == 2,
          "M11_stronger_competitor");

  std::cout << "metamorphic_M1=PASS\n"
            << "metamorphic_M2=PASS\n"
            << "metamorphic_M3=PASS\n"
            << "metamorphic_M4=PASS\n"
            << "metamorphic_M5=PASS\n"
            << "metamorphic_M7=PASS\n"
            << "metamorphic_M8=PASS\n"
            << "metamorphic_M9=PASS\n"
            << "metamorphic_M10=PASS\n"
            << "metamorphic_M11=PASS\n";
  return 0;
}
