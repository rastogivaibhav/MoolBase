#include "graphene/epistemic_control.hpp"
#include "graphene/epistemic_receipt.hpp"
#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

constexpr uint64_t kSeed = 20260927ULL;
constexpr size_t kDeterministicEpisodes = 500;
constexpr size_t kMutationEpisodes = 100;

struct FamilyResult {
  size_t episodes{0};
  size_t passed{0};
  size_t failed{0};
  std::string first_failure;
};

std::map<std::string, FamilyResult> results;
std::vector<std::string> failures;

void record(const std::string& family, bool ok, const std::string& detail) {
  auto& result = results[family];
  ++result.episodes;
  if (ok) {
    ++result.passed;
  } else {
    ++result.failed;
    if (result.first_failure.empty()) result.first_failure = detail;
    failures.push_back(family + ": " + detail);
  }
}

DialecticPath make_path(uint32_t root,
                        uint32_t edge,
                        std::string source,
                        std::string family,
                        double score,
                        bool contradiction = false,
                        double relevance = 1.0,
                        SemanticVerificationStatus verification =
                            SemanticVerificationStatus::Unverified) {
  DialecticPath path;
  path.root_node = root;
  path.anchor_node = 999;
  path.nodes = {root, 999};
  path.edges = {edge};
  path.score = score;
  path.query_relevance = relevance;
  path.target_consistency = relevance;
  path.completeness = 1.0;
  path.temporal_consistent = true;
  path.contains_contradiction = contradiction;
  path.semantic_verification = verification;
  path.evidence.push_back(
      {std::move(source), "span", "", std::move(family), "", ""});
  return path;
}

FiberBundle make_bundle(std::vector<RootBundle> roots,
                        uint64_t snapshot = 1) {
  BundleSet raw;
  raw.snapshot_version = snapshot;
  raw.roots = std::move(roots);
  return FiberBundleBuilder().build(raw);
}

ConvergedAnswer converge(const FiberBundle& bundle) {
  LyapunovCritic critic;
  EpistemicController controller;
  const auto stability = critic.assess(bundle, QueryMode::Empirical);
  const auto admissibility =
      controller.assess(bundle, stability, QueryMode::Empirical);
  return controller.converge(bundle, admissibility, stability);
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
  input.root = root;
  input.symptom = symptom;
  input.incident = 8801;
  input.metadata["source"] = "moolbase-monkey-v1";
  return input;
}

bool ok(Status status) { return static_cast<bool>(status); }

std::string escape_json(const std::string& value) {
  std::string out;
  for (char ch : value) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += ch; break;
    }
  }
  return out;
}

void family_a(size_t index) {
  const bool correct_lineage = index % 2 == 0;
  RootBundle root;
  root.root_node = 1;
  if (correct_lineage) {
    root.paths.push_back(make_path(1, 10, "shared-source", "family-a", 0.90));
    root.paths.push_back(make_path(1, 11, "shared-source", "family-b", 0.89));
  } else {
    root.paths.push_back(make_path(1, 10, "source-a", "family-a", 0.90));
    root.paths.push_back(make_path(1, 11, "source-b", "family-b", 0.89));
  }
  const FiberBundle bundle = make_bundle({root}, 1000 + index);
  const size_t independent =
      bundle.fibers.front().independent_evidence_family_count;
  const bool pass = correct_lineage ? independent == 1 : independent == 2;
  record("A_provenance", pass,
         "independence count inconsistent with supplied lineage");
}

void family_b(size_t index) {
  RootBundle h1;
  h1.root_node = 1;
  for (uint32_t n = 0; n < 3; ++n) {
    h1.paths.push_back(
        make_path(1, 20 + n, "shared-doc", "shared-family",
                  0.96 - 0.01 * n));
  }
  RootBundle h2;
  h2.root_node = 2;
  h2.paths.push_back(make_path(2, 30, "ind-a", "ind-a", 0.84));
  h2.paths.push_back(make_path(2, 31, "ind-b", "ind-b", 0.83));
  const FiberBundle bundle = make_bundle({h1, h2}, 2000 + index);
  const ConvergedAnswer answer = converge(bundle);
  record("B_hypothesis_competition", answer.primary_node == 2,
         "correlated path count defeated independent corroboration");
}

void family_c(size_t index) {
  const bool weak = index % 2 == 0;
  RootBundle h1;
  h1.root_node = 1;
  h1.paths.push_back(make_path(
      1, 40, "h1", "h1-support", 0.99, false, 1.0,
      SemanticVerificationStatus::Verified));
  h1.paths.push_back(make_path(
      1, 41, "opp", "h1-opposition", weak ? 0.04 : 0.96, true, 1.0,
      SemanticVerificationStatus::Contradicted));
  RootBundle h2;
  h2.root_node = 2;
  h2.paths.push_back(make_path(
      2, 42, "h2-a", "h2-a", 0.92, false, 1.0,
      SemanticVerificationStatus::Verified));
  if (!weak) {
    h2.paths.push_back(make_path(
        2, 43, "h2-b", "h2-b", 0.91, false, 1.0,
        SemanticVerificationStatus::Verified));
  }
  const auto answer = converge(make_bundle({h1, h2}, 3000 + index));
  record("C_contradiction", weak ? answer.primary_node == 1
                                 : answer.primary_node == 2,
         weak ? "weak contradiction dethroned primary"
              : "decisive contradiction failed to demote primary");
}

void family_d(size_t index, const fs::path& base) {
  const fs::path dir = base / ("supersession-" + std::to_string(index));
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions options;
  options.dimension = 2;
  options.fsync_on_commit = false;
  bool pass = ok(db.open(dir, options));
  const uint64_t signature = signature_for(1, 2);
  uint32_t a = 0, b = 0, c = 0;
  pass = pass && ok(db.put_node(node("A", {1.0f, 0.0f}, signature), &a));
  const uint64_t first = db.snapshot();
  pass = pass && ok(db.put_node(node("B", {0.8f, 0.2f}, signature), &b));
  pass = pass && ok(db.put_node(node("C", {0.6f, 0.4f}, signature), &c));
  uint32_t e1 = 0, e2 = 0;
  pass = pass && ok(db.put_edge(
      {a, b, EdgeOrigin::Observed, EdgeRole::Supersedes, 1.0,
       {{"source_id", "decision-1"}}}, &e1));
  pass = pass && ok(db.put_edge(
      {b, c, EdgeOrigin::Observed, EdgeRole::Supersedes, 1.0,
       {{"source_id", "decision-2"}}}, &e2));
  const auto edge1 = db.get_edge(e1);
  const auto edge2 = db.get_edge(e2);
  pass = pass && edge1 && edge2 &&
         edge1->role == EdgeRole::Supersedes &&
         edge2->role == EdgeRole::Supersedes &&
         db.get_node(a, first).has_value() &&
         !db.get_node(b, first).has_value();
  db.close();
  fs::remove_all(dir);
  record("D_supersession_history", pass,
         "typed supersession or historical visibility failed");
}

void family_e(size_t index, const fs::path& base) {
  const fs::path dir = base / ("recovery-" + std::to_string(index));
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  bool pass = ok(db.open(dir, db_options));
  const uint64_t signature = signature_for(7, 8);
  const uint32_t depth = 2 + static_cast<uint32_t>(index % 3);
  std::vector<uint32_t> ids;
  for (uint32_t i = 0; i <= depth; ++i) {
    uint32_t id = 0;
    std::vector<float> vector =
        i == depth ? std::vector<float>{0.0f, 0.0f, 1.0f}
                   : std::vector<float>{1.0f - 0.2f * i, 0.2f * i, 0.0f};
    pass = pass && ok(db.put_node(
        node("chain-" + std::to_string(i), vector, signature,
             i == 0, i == depth),
        &id));
    ids.push_back(id);
  }
  for (uint32_t i = 0; i < depth; ++i) {
    pass = pass && ok(db.put_edge(
        {ids[i], ids[i + 1], EdgeOrigin::Observed, EdgeRole::Causal, 0.98,
         {{"source_id", "chain-source-" + std::to_string(i)}}}));
  }

  RuntimeOptions options;
  options.dialectic.mode = QueryMode::Empirical;
  options.dialectic.semantic_candidates = 1;
  options.dialectic.max_hops = 1;
  options.dialectic.max_paths = 16;
  options.dialectic.max_paths_per_root = 8;
  options.dialectic.max_visited_states = 64;
  options.dialectic.minimum_confidence = 0.20;
  options.max_recursive_cycles = 3;
  options.enable_dwm = false;
  options.update_model_world = false;

  CompleteHypoKoshRuntime runtime(db);
  const auto result =
      runtime.reason({0.0f, 0.0f, 1.0f}, signature, options);
  pass = pass && result.receipt.expansion_rounds <= 3 &&
         result.receipt.recovery_trace.size() <= 3 &&
         !result.receipt.opposition_executed;
  db.close();
  fs::remove_all(dir);
  record("E_recovery_frontier", pass,
         "bounded recovery invariant failed");
}

void family_f(size_t index) {
  RootBundle h1;
  h1.root_node = 1;
  h1.paths.push_back(make_path(1, 60, "f-a", "f-a", 0.90));
  RootBundle h2;
  h2.root_node = 2;
  h2.paths.push_back(make_path(2, 61, "f-b", "f-b", 0.89));
  const FiberBundle bundle = make_bundle({h1, h2}, 6000 + index);
  LyapunovCritic critic;
  EpistemicController controller;
  const auto stability = critic.assess(bundle, QueryMode::Empirical);
  const auto admissibility =
      controller.assess(bundle, stability, QueryMode::Empirical);
  const auto answer = controller.converge(bundle, admissibility, stability);
  const auto opposition =
      controller.oppose(bundle, answer, admissibility, stability);
  const bool pass =
      answer.has_answer && !opposition.challenged_claims.empty() &&
      opposition.reopen_nodes.size() >= 2 &&
      std::find(opposition.reopen_nodes.begin(),
                opposition.reopen_nodes.end(), answer.primary_node) !=
          opposition.reopen_nodes.end();
  record("F_dialectic", pass,
         "competing target failed to create observable opposition/reopen");
}

void family_g(size_t index) {
  HypoKoshRuntimeResult runtime;
  runtime.receipt.snapshot_version = 7000 + index;
  runtime.receipt.hypokosh_capability_enabled = true;
  runtime.receipt.dwm_capability_enabled = index % 2 == 0;
  runtime.receipt.opposition_research_enabled = index % 3 == 0;
  runtime.receipt.graphene_executed = true;
  runtime.receipt.convergence_executed = true;
  runtime.receipt.opposition_executed =
      runtime.receipt.dwm_capability_enabled;
  runtime.receipt.governed_projection_executed = true;
  runtime.receipt.terminal_cause =
      runtime.receipt.dwm_capability_enabled
          ? "dialectic_opposition_blocks_resolution"
          : "resolution_conditions_not_fully_earned";
  runtime.status = runtime.receipt.dwm_capability_enabled
                       ? GovernedEpistemicStatus::Contested
                       : GovernedEpistemicStatus::ProvisionallyResolved;
  runtime.primary_node = 1;
  runtime.confidence = 0.8;
  runtime.final_bundle.immutable_hash = 1234 + index;

  NativeEpistemicEvent terminal;
  terminal.sequence = 1;
  terminal.source = EpistemicEventSource::GrapheneCore;
  terminal.type = EpistemicEventType::Terminal;
  terminal.hypothesis_node = 1;
  terminal.epistemic_state = governed_status_name(runtime.status);
  terminal.reason = runtime.receipt.terminal_cause;
  runtime.receipt.epistemic_events.push_back(terminal);

  const auto first = build_compact_epistemic_receipt(runtime);
  const auto repeated = build_compact_epistemic_receipt(runtime);
  HypoKoshRuntimeResult mutated = runtime;
  mutated.receipt.terminal_cause += "_mutated";
  const auto changed = build_compact_epistemic_receipt(mutated);
  const bool pass =
      first.content_hash == repeated.content_hash &&
      first.content_hash != changed.content_hash &&
      first.dwm_capability_enabled ==
          runtime.receipt.dwm_capability_enabled &&
      first.opposition_executed == runtime.receipt.opposition_executed;
  record("G_receipt_consistency", pass,
         "compact receipt determinism/provenance invariant failed");
}

void family_h(size_t index, const fs::path& base) {
  const fs::path dir = base / ("snapshot-" + std::to_string(index));
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions options;
  options.dimension = 2;
  options.fsync_on_commit = false;
  bool pass = ok(db.open(dir, options));
  const uint64_t signature = signature_for(3, 4);
  uint32_t early = 0, late = 0;
  pass = pass && ok(db.put_node(node("early", {1.0f, 0.0f}, signature), &early));
  const uint64_t snapshot = db.snapshot();
  pass = pass && ok(db.put_node(node("late", {0.0f, 1.0f}, signature), &late));
  pass = pass && db.get_node(early, snapshot).has_value() &&
         !db.get_node(late, snapshot).has_value() &&
         db.get_node(late).has_value();
  db.close();
  fs::remove_all(dir);
  record("H_snapshot_history", pass,
         "late write appeared in immutable earlier snapshot");
}

void family_i(size_t index, const fs::path& base) {
  const fs::path dir = base / ("durability-" + std::to_string(index));
  const fs::path backup = base / ("durability-backup-" + std::to_string(index));
  fs::remove_all(dir);
  fs::remove_all(backup);
  DBOptions options;
  options.dimension = 2;
  options.fsync_on_commit = false;
  GrapheneDB db;
  bool pass = ok(db.open(dir, options));
  uint32_t id = 0;
  pass = pass && ok(db.put_node(
      node("durable", {1.0f, 0.0f}, signature_for(5, 6)), &id));
  pass = pass && ok(db.backup(backup));
  db.close();
  GrapheneDB restored;
  pass = pass && ok(restored.open(backup, options));
  std::string report;
  pass = pass && ok(restored.validate(&report)) &&
         restored.get_node(id).has_value();
  restored.close();
  fs::remove_all(dir);
  fs::remove_all(backup);
  record("I_durability", pass,
         "backup/reopen/validate durability invariant failed");
}

void family_j(size_t index) {
  RootBundle root;
  root.root_node = 1;
  const double relevance =
      index % 3 == 0 ? 0.0 : (index % 3 == 1 ? 0.49 : 1.0);
  DialecticPath candidate =
      make_path(1, 90, "j", "j-family",
                index % 2 == 0 ? 0.0 : 1.0,
                index % 5 == 0, relevance);
  if (relevance < 0.5) candidate.role_hint = PathRoleHint::Noise;
  root.paths.push_back(candidate);
  const FiberBundle bundle = make_bundle({root}, 9000 + index);
  const auto answer = converge(bundle);
  const bool pass =
      answer.confidence >= 0.0 && answer.confidence <= 1.0 &&
      (relevance >= 0.5 || !answer.has_answer);
  record("J_parameter_edges", pass,
         "boundary parameter produced impossible confidence/promotion");
}

void family_k(size_t index) {
  RootBundle h1;
  h1.root_node = 1;
  h1.paths.push_back(make_path(1, 100, "k-a", "k-a", 0.90));
  h1.paths.push_back(make_path(1, 101, "k-b", "k-b", 0.88));
  RootBundle h2;
  h2.root_node = 2;
  h2.paths.push_back(make_path(2, 102, "k-c", "k-c", 0.70));
  BundleSet raw;
  raw.snapshot_version = 10000 + index;
  raw.roots = {h1, h2};
  FiberBundleBuilder builder;
  const FiberBundle original = builder.build(raw);
  const auto answer = converge(original);

  std::reverse(raw.roots.begin(), raw.roots.end());
  for (auto& root : raw.roots) std::reverse(root.paths.begin(), root.paths.end());
  const FiberBundle reordered = builder.build(raw);
  const auto reordered_answer = converge(reordered);

  RootBundle* selected = nullptr;
  for (auto& root : raw.roots) {
    if (root.root_node == 1) selected = &root;
  }
  if (selected) {
    selected->paths.push_back(
        make_path(1, 103, "k-a", "k-a", 0.90));
  }
  const FiberBundle duplicated = builder.build(raw);
  const auto duplicate_answer = converge(duplicated);
  const bool pass =
      original.immutable_hash == reordered.immutable_hash &&
      answer.primary_node == reordered_answer.primary_node &&
      std::abs(answer.confidence - reordered_answer.confidence) < 1e-12 &&
      duplicate_answer.primary_node == answer.primary_node &&
      std::abs(duplicate_answer.confidence - answer.confidence) < 1e-12;
  record("K_metamorphic", pass,
         "reorder/same-family duplicate changed governed selection");
}

void mutation_episode(size_t index, std::mt19937_64& rng) {
  const uint32_t target = 1 + static_cast<uint32_t>(rng() % 3);
  RootBundle root;
  root.root_node = target;
  const size_t paths = 1 + static_cast<size_t>(rng() % 8);
  for (size_t i = 0; i < paths; ++i) {
    const std::string family =
        "mut-family-" + std::to_string(rng() % 4);
    const std::string source =
        "mut-source-" + std::to_string(rng() % 5);
    const double score =
        static_cast<double>(rng() % 1001) / 1000.0;
    const bool contradiction = (rng() % 7) == 0;
    const double relevance =
        static_cast<double>(rng() % 1001) / 1000.0;
    root.paths.push_back(make_path(
        target, 200 + static_cast<uint32_t>(i), source, family, score,
        contradiction, relevance));
  }
  const FiberBundle first = make_bundle({root}, 20000 + index);
  const FiberBundle repeated = make_bundle({root}, 20000 + index);
  const auto first_answer = converge(first);
  const auto repeated_answer = converge(repeated);
  const bool pass =
      first.immutable_hash == repeated.immutable_hash &&
      first_answer.primary_node == repeated_answer.primary_node &&
      first_answer.has_answer == repeated_answer.has_answer &&
      std::abs(first_answer.confidence - repeated_answer.confidence) < 1e-12;
  record("M_mutation_fuzz", pass,
         "identical mutated input produced nondeterministic result");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: graphenedb_moolbase_monkey_v1 <report.json>\n";
    return 2;
  }

  const fs::path report_path = argv[1];
  const fs::path base = fs::temp_directory_path() / "moolbase-monkey-v1";
  fs::remove_all(base);
  fs::create_directories(base);

  for (size_t index = 0; index < kDeterministicEpisodes; ++index) {
    switch (index % 11) {
      case 0: family_a(index); break;
      case 1: family_b(index); break;
      case 2: family_c(index); break;
      case 3: family_d(index, base); break;
      case 4: family_e(index, base); break;
      case 5: family_f(index); break;
      case 6: family_g(index); break;
      case 7: family_h(index, base); break;
      case 8: family_i(index, base); break;
      case 9: family_j(index); break;
      case 10: family_k(index); break;
    }
  }

  std::mt19937_64 rng(kSeed);
  for (size_t index = 0; index < kMutationEpisodes; ++index) {
    mutation_episode(index, rng);
  }

  fs::remove_all(base);

  std::ofstream out(report_path);
  if (!out) {
    std::cerr << "cannot open monkey report path\n";
    return 2;
  }
  out << "{\n"
      << "  \"schema\": \"moolbase-monkey-v1\",\n"
      << "  \"seed\": " << kSeed << ",\n"
      << "  \"deterministic_episodes\": " << kDeterministicEpisodes << ",\n"
      << "  \"mutation_episodes\": " << kMutationEpisodes << ",\n"
      << "  \"families\": {\n";
  bool first = true;
  for (const auto& [family, result] : results) {
    if (!first) out << ",\n";
    first = false;
    out << "    \"" << escape_json(family) << "\": {"
        << "\"episodes\":" << result.episodes
        << ",\"passed\":" << result.passed
        << ",\"failed\":" << result.failed
        << ",\"first_failure\":\""
        << escape_json(result.first_failure) << "\"}";
  }
  out << "\n  },\n"
      << "  \"failures\": [";
  for (size_t i = 0; i < failures.size(); ++i) {
    if (i) out << ',';
    out << "\"" << escape_json(failures[i]) << "\"";
  }
  out << "]\n}\n";
  out.close();

  std::cout << "monkey_seed=" << kSeed << "\n";
  std::cout << "monkey_deterministic_episodes=" << kDeterministicEpisodes << "\n";
  std::cout << "monkey_mutation_episodes=" << kMutationEpisodes << "\n";
  std::cout << "monkey_failures=" << failures.size() << "\n";
  for (const auto& [family, result] : results) {
    std::cout << "monkey_family=" << family
              << " episodes=" << result.episodes
              << " passed=" << result.passed
              << " failed=" << result.failed << "\n";
  }
  return failures.empty() ? 0 : 1;
}
