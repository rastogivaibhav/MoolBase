#include "graphene/db.hpp"
#include "graphene/epistemic_control.hpp"
#include "graphene/epistemic_receipt.hpp"
#include "graphene/fiber_bundle.hpp"
#include "graphene/hypokosh_runtime.hpp"
#include "graphene/stability_critic.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
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

NodeInput node(std::string content,
               std::vector<float> vector,
               uint64_t signature,
               bool root = false,
               bool symptom = false) {
  NodeInput input;
  input.content = std::move(content);
  input.vector = std::move(vector);
  input.signature = signature;
  input.incident = 9911;
  input.root = root;
  input.symptom = symptom;
  input.metadata["source"] = "metamorphic-v1";
  return input;
}

void require_status(Status status, const char* label) {
  if (!status) {
    std::cerr << "metamorphic_failure=" << label
              << " detail=" << status.message << "\n";
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

  // M6: serialize/close/reopen preserves the governed result and durable
  // compact receipt identity.
  namespace fs = std::filesystem;
  const fs::path m6_dir =
      fs::temp_directory_path() / "moolbase-metamorphic-m6";
  fs::remove_all(m6_dir);
  GrapheneDB db;
  DBOptions db_options;
  db_options.dimension = 2;
  db_options.fsync_on_commit = false;
  require_status(db.open(m6_dir, db_options), "M6_open_initial");
  const uint64_t signature = signature_for(77, 88);
  uint32_t m6_h1 = 0, m6_h2 = 0, m6_symptom = 0;
  require_status(
      db.put_node(node("M6 H1", {1.0f, 0.0f}, signature, true), &m6_h1),
      "M6_put_h1");
  require_status(
      db.put_node(node("M6 H2", {-1.0f, 0.0f}, signature, true), &m6_h2),
      "M6_put_h2");
  require_status(
      db.put_node(node("M6 symptom", {0.0f, 1.0f}, signature, false, true),
                  &m6_symptom),
      "M6_put_symptom");
  EdgeInput m6_edge1;
  m6_edge1.from = m6_h1;
  m6_edge1.to = m6_symptom;
  m6_edge1.origin = EdgeOrigin::Observed;
  m6_edge1.role = EdgeRole::Supports;
  m6_edge1.confidence = 0.90;
  m6_edge1.metadata["source_id"] = "m6-source-a";
  m6_edge1.metadata["evidence_family_id"] = "m6-family-a";
  require_status(db.put_edge(m6_edge1), "M6_put_edge1");

  EdgeInput m6_edge2 = m6_edge1;
  m6_edge2.from = m6_h2;
  m6_edge2.confidence = 0.55;
  m6_edge2.metadata["source_id"] = "m6-source-b";
  m6_edge2.metadata["evidence_family_id"] = "m6-family-b";
  require_status(db.put_edge(m6_edge2), "M6_put_edge2");

  RuntimeOptions m6_options;
  m6_options.enable_hypokosh = true;
  m6_options.enable_dwm = false;
  m6_options.enable_opposition_research = false;
  m6_options.update_model_world = false;
  m6_options.dialectic.mode = QueryMode::Empirical;
  m6_options.dialectic.semantic_candidates = 4;
  m6_options.dialectic.max_hops = 2;
  m6_options.dialectic.max_paths = 16;
  m6_options.dialectic.max_paths_per_root = 8;
  m6_options.dialectic.minimum_confidence = 0.10;

  CompleteHypoKoshRuntime m6_runtime_before(db);
  const auto m6_before = m6_runtime_before.reason(
      {0.0f, 1.0f}, signature, m6_options);
  const auto m6_receipt_before =
      build_compact_epistemic_receipt(m6_before);
  require_status(db.close(), "M6_close");

  GrapheneDB reopened;
  require_status(reopened.open(m6_dir, db_options), "M6_reopen");
  CompleteHypoKoshRuntime m6_runtime_after(reopened);
  const auto m6_after = m6_runtime_after.reason(
      {0.0f, 1.0f}, signature, m6_options);
  const auto m6_receipt_after =
      build_compact_epistemic_receipt(m6_after);
  require(
      m6_before.status == m6_after.status &&
          m6_before.primary_node == m6_after.primary_node &&
          std::abs(m6_before.confidence - m6_after.confidence) < 1e-12 &&
          m6_before.final_bundle.immutable_hash ==
              m6_after.final_bundle.immutable_hash &&
          m6_receipt_before.content_hash == m6_receipt_after.content_hash,
      "M6_close_reopen");
  require_status(reopened.close(), "M6_close_reopened");
  fs::remove_all(m6_dir);

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

  // M10: semantic verification breaks an otherwise equal target tie.
  // Keep support strength, independent-family count and best-path score equal
  // so verification is the only ranking coordinate that changes.
  RootBundle m10_h1;
  m10_h1.root_node = 1;
  m10_h1.paths.push_back(path(1, 501, "m10-a", "m10-fa", 0.75));
  RootBundle m10_h2;
  m10_h2.root_node = 2;
  m10_h2.paths.push_back(path(2, 502, "m10-b", "m10-fb", 0.75));
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
            << "metamorphic_M6=PASS\n"
            << "metamorphic_M7=PASS\n"
            << "metamorphic_M8=PASS\n"
            << "metamorphic_M9=PASS\n"
            << "metamorphic_M10=PASS\n"
            << "metamorphic_M11=PASS\n";
  return 0;
}
