#include "graphene/db.hpp"
#include <filesystem>
#include <iostream>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

static void die_if_bad(const Status& st, const char* where) {
  if (!st) {
    std::cerr << where << " failed: " << st.message << "\n";
    std::exit(2);
  }
}

static uint32_t add(GrapheneDB& db, const std::string& content, std::vector<float> vector,
                    uint64_t sig, uint32_t incident, bool root=false, bool symptom=false,
                    std::map<std::string, std::string> metadata = {}) {
  NodeInput n;
  n.content = content;
  n.vector = std::move(vector);
  n.signature = sig;
  n.incident = incident;
  n.root = root;
  n.symptom = symptom;
  n.metadata = std::move(metadata);
  uint32_t id = 0;
  die_if_bad(db.put_node(n, &id), "put_node");
  return id;
}

static void link(GrapheneDB& db, uint32_t from, uint32_t to, EdgeRole role, double confidence = 0.95) {
  EdgeInput e;
  e.from = from;
  e.to = to;
  e.role = role;
  e.confidence = confidence;
  die_if_bad(db.put_edge(e), "put_edge");
}

int main() {
  const fs::path dir = fs::temp_directory_path() / "graphenedb_uniqueness_demo";
  fs::remove_all(dir);

  GrapheneDB db;
  DBOptions opt;
  opt.dimension = 3;
  opt.fsync_on_commit = false;
  die_if_bad(db.open(dir, opt), "open");

  const uint64_t checkout_sig = signature_for(2, 7);
  const uint64_t dns_sig = signature_for(9, 4);

  uint32_t root = add(db,
    "ADR-014: GCP ingress routing changed during checkout migration; rollback is the safe fix.",
    {0.90f, 0.10f, 0.00f}, checkout_sig, 1421, true, false,
    {{"type", "architecture_decision"}, {"status", "current"}});
  uint32_t dependency = add(db,
    "Deployment DEP-771 changed ingress timeout and payment-gateway route.",
    {0.65f, 0.35f, 0.00f}, checkout_sig, 1421, false, false,
    {{"type", "deployment"}});
  uint32_t symptom = add(db,
    "INC-1421: checkout timeout after GCP migration; customers saw payment failures.",
    {0.10f, 0.97f, 0.00f}, checkout_sig, 1421, false, true,
    {{"type", "incident"}, {"service", "checkout"}});
  uint32_t stale = add(db,
    "Old note: checkout failure was thought to be database pool exhaustion before DEP-771 evidence arrived.",
    {0.15f, 0.92f, 0.05f}, checkout_sig, 1421, false, true,
    {{"type", "hypothesis"}, {"status", "superseded"}});
  uint32_t unrelated = add(db,
    "INC-1440: similar checkout timeout, but root cause was DNS propagation outside the migration path.",
    {0.11f, 0.94f, 0.02f}, dns_sig, 1440, false, true,
    {{"type", "incident"}, {"service", "checkout"}, {"status", "similar_but_different"}});

  link(db, root, dependency, EdgeRole::Causal, 0.97);
  link(db, dependency, symptom, EdgeRole::Causal, 0.96);
  link(db, root, stale, EdgeRole::Contradicts, 0.75);
  link(db, unrelated, symptom, EdgeRole::Contradicts, 0.60);

  std::vector<float> query{0.10f, 0.96f, 0.00f};

  std::cout << "=== Same question, three retrieval styles ===\n";
  std::cout << "Question: Why did checkout fail after the GCP migration?\n\n";

  std::cout << "1) Vector-only style\n";
  auto vr = db.vector_search(query, 3);
  for (const auto& r : vr) {
    auto n = db.get_node(r.node_id);
    std::cout << "  score=" << r.score << " node=" << r.node_id << " :: "
              << (n ? n->content : "<missing>") << "\n";
  }

  std::cout << "\n2) Graph-only style\n";
  std::cout << "  Known link path: root ADR -> deployment -> incident symptom\n";
  std::cout << "  But graph-only does not rank semantic closeness or know query intent.\n";

  std::cout << "\n3) Graphene causal-memory style\n";
  auto bundle = db.causal_search(query, checkout_sig, QueryMode::Balanced);
  if (bundle.abstain) {
    std::cout << "  abstain: " << bundle.reason << "\n";
    return 0;
  }
  std::cout << "  target_root=" << bundle.target_node << " confidence=" << bundle.confidence
            << " paths=" << bundle.paths.size() << " snapshot=" << bundle.snapshot_version << "\n";
  for (const auto& why : bundle.why_retrieved) {
    std::cout << "  why=" << why << "\n";
  }
  for (const auto& p : bundle.paths) {
    std::cout << "  path nodes:";
    for (uint32_t id : p.nodes) std::cout << " " << id;
    std::cout << " | edges:";
    for (uint32_t id : p.edges) std::cout << " " << id;
    std::cout << " | contradiction=" << (p.contains_contradiction ? "yes" : "no") << "\n";
  }

  std::cout << "\nConclusion: vector search finds similar chunks; graph traversal finds links; Graphene returns an evidence path with confidence and contradiction awareness.\n";
  db.close();
  fs::remove_all(dir);
  return 0;
}
