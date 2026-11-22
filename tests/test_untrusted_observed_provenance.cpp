// Issue #135: untrusted native ingestion must not acquire Observed/Discovered
// authority. Adversarial tests across put_edge, put_batch and put_extraction.
#include "graphene/db.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

constexpr uint32_t kDim = 4;

fs::path fresh_dir(const std::string& name) {
  fs::path p = fs::temp_directory_path() / ("moolbase-135-" + name);
  std::error_code ec;
  fs::remove_all(p, ec);
  return p;
}

NodeInput node(const std::string& text, float base) {
  NodeInput n;
  n.content = text;
  n.vector = {base, base + 0.1f, base + 0.2f, base + 0.3f};
  n.signature = static_cast<uint64_t>(base * 100);
  n.metadata["source"] = "test";
  return n;
}

DBOptions options(ObservedProvenancePolicy policy, const std::string& key = "") {
  DBOptions o;
  o.dimension = kDim;
  o.fsync_on_commit = false;
  o.observed_provenance = policy;
  o.authority_key = key;
  return o;
}

struct Pair {
  uint32_t a{0};
  uint32_t b{0};
};

Pair two_nodes(GrapheneDB& db) {
  Pair p;
  assert(db.put_node(node("a", 0.1f), &p.a));
  assert(db.put_node(node("b", 0.5f), &p.b));
  return p;
}

EdgeInput edge(Pair p, EdgeOrigin origin) {
  EdgeInput e;
  e.from = p.a;
  e.to = p.b;
  e.origin = origin;
  return e;
}

EdgeOrigin stored_origin(GrapheneDB& db, uint32_t id) {
  auto e = db.get_edge(id);
  assert(e);
  return e->origin;
}

std::string meta(GrapheneDB& db, uint32_t id, const std::string& key) {
  auto e = db.get_edge(id);
  assert(e);
  auto it = e->metadata.find(key);
  return it == e->metadata.end() ? std::string() : it->second;
}

// 1. Default policy is unchanged: trusted in-process callers keep legacy behaviour.
void legacy_default_unchanged() {
  GrapheneDB db;
  assert(db.open(fresh_dir("legacy"), options(ObservedProvenancePolicy::TrustedCallers)));
  auto p = two_nodes(db);
  uint32_t id = 0;
  assert(db.put_edge(edge(p, EdgeOrigin::Observed), &id));
  assert(stored_origin(db, id) == EdgeOrigin::Observed);
  assert(meta(db, id, "origin_policy").empty());
  // No authority can be issued without the opt-in policy.
  assert(!db.issue_authority("anything").valid());
}

// 2. RequireAuthority demotes untrusted Observed/Discovered and stamps the audit trail.
void put_edge_is_demoted() {
  GrapheneDB db;
  assert(db.open(fresh_dir("edge"), options(ObservedProvenancePolicy::RequireAuthority, "k-secret")));
  auto p = two_nodes(db);
  uint32_t obs = 0, dis = 0, inf = 0, hyp = 0;
  assert(db.put_edge(edge(p, EdgeOrigin::Observed), &obs));
  assert(db.put_edge(edge(p, EdgeOrigin::Discovered), &dis));
  assert(db.put_edge(edge(p, EdgeOrigin::Inferred), &inf));
  assert(db.put_edge(edge(p, EdgeOrigin::Hypothetical), &hyp));
  assert(stored_origin(db, obs) == EdgeOrigin::Hypothetical);
  assert(stored_origin(db, dis) == EdgeOrigin::Hypothetical);
  assert(meta(db, obs, "origin_requested") == "observed");
  assert(meta(db, dis, "origin_requested") == "discovered");
  assert(meta(db, obs, "origin_policy") == "untrusted_demoted_v1");
  // Origins that carry no empirical authority are untouched and unstamped.
  assert(stored_origin(db, inf) == EdgeOrigin::Inferred);
  assert(stored_origin(db, hyp) == EdgeOrigin::Hypothetical);
  assert(meta(db, inf, "origin_policy").empty());
}

// 3. Forged audit stamps and forged promotion fields cannot launder authority.
void forged_metadata_cannot_help() {
  GrapheneDB db;
  assert(db.open(fresh_dir("forge"), options(ObservedProvenancePolicy::RequireAuthority, "k-secret")));
  auto p = two_nodes(db);
  EdgeInput e = edge(p, EdgeOrigin::Observed);
  e.metadata["origin_policy"] = "authority_verified_v1";
  e.metadata["origin_requested"] = "hypothetical";
  e.metadata["source_id"] = "lab-notebook-17";
  e.metadata["promotion_status"] = "verified";
  uint32_t id = 0;
  assert(db.put_edge(e, &id));
  assert(stored_origin(db, id) == EdgeOrigin::Hypothetical);
  assert(meta(db, id, "origin_policy") == "untrusted_demoted_v1");
  assert(meta(db, id, "origin_requested") == "observed");

  // A pre-forged stamp on an edge that was never demoted is also stripped.
  EdgeInput inferred = edge(p, EdgeOrigin::Inferred);
  inferred.metadata["origin_policy"] = "authority_verified_v1";
  assert(db.put_edge(inferred, &id));
  assert(meta(db, id, "origin_policy").empty());
}

// 4. Authority: wrong key, empty key and foreign-database capabilities all fail closed.
void authority_cannot_be_guessed_or_borrowed() {
  GrapheneDB db;
  assert(db.open(fresh_dir("auth"), options(ObservedProvenancePolicy::RequireAuthority, "k-secret")));
  auto p = two_nodes(db);
  assert(!db.issue_authority("").valid());
  assert(!db.issue_authority("k-secre").valid());
  assert(!db.issue_authority("k-secret-extra").valid());
  assert(!TrustedAuthority().valid());

  // No key configured: nothing can ever be issued.
  GrapheneDB keyless;
  assert(keyless.open(fresh_dir("keyless"), options(ObservedProvenancePolicy::RequireAuthority, "")));
  assert(!keyless.issue_authority("").valid());

  // A valid capability from another database instance does not transfer.
  GrapheneDB other;
  assert(other.open(fresh_dir("other"), options(ObservedProvenancePolicy::RequireAuthority, "k-secret")));
  TrustedAuthority foreign = other.issue_authority("k-secret");
  assert(foreign.valid());
  EdgeInput e = edge(p, EdgeOrigin::Observed);
  e.authority = &foreign;
  uint32_t id = 0;
  assert(db.put_edge(e, &id));
  assert(stored_origin(db, id) == EdgeOrigin::Hypothetical);

  // The genuine capability preserves Observed/Discovered for trusted connectors.
  TrustedAuthority real = db.issue_authority("k-secret");
  assert(real.valid());
  e.authority = &real;
  assert(db.put_edge(e, &id));
  assert(stored_origin(db, id) == EdgeOrigin::Observed);
  assert(meta(db, id, "origin_policy").empty());
  e.origin = EdgeOrigin::Discovered;
  assert(db.put_edge(e, &id));
  assert(stored_origin(db, id) == EdgeOrigin::Discovered);
}

// 5. put_batch: untrusted edges are demoted, trusted batches and edges are not.
void put_batch_is_governed() {
  GrapheneDB db;
  assert(db.open(fresh_dir("batch"), options(ObservedProvenancePolicy::RequireAuthority, "k-secret")));
  auto p = two_nodes(db);
  TrustedAuthority real = db.issue_authority("k-secret");

  BatchInput untrusted;
  untrusted.edges.push_back(edge(p, EdgeOrigin::Observed));
  untrusted.edges.push_back(edge(p, EdgeOrigin::Discovered));
  BatchResult r;
  assert(db.put_batch(untrusted, &r));
  assert(r.edge_ids.size() == 2);
  for (uint32_t id : r.edge_ids) assert(stored_origin(db, id) == EdgeOrigin::Hypothetical);

  BatchInput trusted = untrusted;
  trusted.authority = &real;
  assert(db.put_batch(trusted, &r));
  assert(stored_origin(db, r.edge_ids[0]) == EdgeOrigin::Observed);
  assert(stored_origin(db, r.edge_ids[1]) == EdgeOrigin::Discovered);

  // Per-edge authority inside an otherwise untrusted batch.
  BatchInput mixed;
  mixed.edges.push_back(edge(p, EdgeOrigin::Observed));
  mixed.edges.push_back(edge(p, EdgeOrigin::Observed));
  mixed.edges[1].authority = &real;
  assert(db.put_batch(mixed, &r));
  assert(stored_origin(db, r.edge_ids[0]) == EdgeOrigin::Hypothetical);
  assert(stored_origin(db, r.edge_ids[1]) == EdgeOrigin::Observed);
}

ExtractionInput extraction() {
  ExtractionInput in;
  in.source_id = "model-output-1";
  in.extraction_run_id = "run-1";
  ExtractionNode a;
  a.external_id = "a";
  a.content = "service deploy";
  a.vector = {0.1f, 0.2f, 0.3f, 0.4f};
  ExtractionNode b;
  b.external_id = "b";
  b.content = "outage";
  b.vector = {0.4f, 0.3f, 0.2f, 0.1f};
  in.nodes = {a, b};
  ExtractionRelation r;
  r.from_external_id = "a";
  r.to_external_id = "b";
  r.origin = EdgeOrigin::Observed;
  r.evidence_id = "ev-1";
  r.evidence_text = "deploy preceded outage";
  in.relations = {r};
  return in;
}

// 6. put_extraction: model/plugin output is demoted, idempotent replay still works.
void put_extraction_is_governed() {
  const fs::path dir = fresh_dir("extract");
  {
    GrapheneDB db;
    assert(db.open(dir, options(ObservedProvenancePolicy::RequireAuthority, "k-secret")));
    ExtractionResult first;
    assert(db.put_extraction(extraction(), &first));
    // Replaying identical untrusted output must not look like a changed duplicate.
    ExtractionResult replay;
    assert(db.put_extraction(extraction(), &replay));
    assert(db.edge_count() == 1);
    bool seen = false;
    for (uint32_t id = 0; id < 8; ++id) {
      auto e = db.get_edge(id);
      if (!e) continue;
      seen = true;
      assert(e->origin == EdgeOrigin::Hypothetical);
      assert(e->metadata.at("origin_requested") == "observed");
    }
    assert(seen);

    TrustedAuthority real = db.issue_authority("k-secret");
    ExtractionInput trusted = extraction();
    trusted.extraction_run_id = "run-2";
    trusted.authority = &real;
    ExtractionResult trusted_result;
    // Re-submitting the same relation with a stronger origin is a conflicting
    // duplicate. Replay can never be used to promote an already-demoted edge.
    Status conflict = db.put_extraction(trusted, &trusted_result);
    assert(!conflict);
    // A genuinely new trusted extraction keeps Observed authority.
    ExtractionInput fresh = extraction();
    fresh.extraction_run_id = "run-3";
    fresh.nodes[0].external_id = "c";
    fresh.nodes[1].external_id = "d";
    fresh.nodes[0].content = "config change";
    fresh.nodes[1].content = "latency spike";
    fresh.nodes[0].vector = {0.7f, 0.1f, 0.1f, 0.1f};
    fresh.nodes[1].vector = {0.1f, 0.7f, 0.1f, 0.1f};
    fresh.relations[0].from_external_id = "c";
    fresh.relations[0].to_external_id = "d";
    fresh.relations[0].evidence_id = "ev-2";
    fresh.authority = &real;
    assert(db.put_extraction(fresh, &trusted_result));
    size_t observed = 0;
    for (uint32_t id = 0; id < 16; ++id) {
      auto e = db.get_edge(id);
      if (e && e->origin == EdgeOrigin::Observed) ++observed;
    }
    assert(observed == 1);
  }
  // 7. The demotion is durable: WAL replay after reopen keeps it.
  GrapheneDB reopened;
  assert(reopened.open(dir, options(ObservedProvenancePolicy::RequireAuthority, "k-secret")));
  size_t hypothetical = 0;
  for (uint32_t id = 0; id < 8; ++id) {
    auto e = reopened.get_edge(id);
    if (e && e->origin == EdgeOrigin::Hypothetical) ++hypothetical;
  }
  assert(hypothetical >= 1);
}

}  // namespace

int main() {
  legacy_default_unchanged();
  put_edge_is_demoted();
  forged_metadata_cannot_help();
  authority_cannot_be_guessed_or_borrowed();
  put_batch_is_governed();
  put_extraction_is_governed();
  std::cout << "untrusted observed provenance tests passed\n";
  return 0;
}
