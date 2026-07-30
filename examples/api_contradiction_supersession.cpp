#include "graphene/db.hpp"
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

static void require(Status st, const char* what) {
  if (!st) {
    std::cerr << "FAIL " << what << ": " << st.message << "\n";
    std::exit(2);
  }
}

int main() {
  fs::path dir = fs::temp_directory_path() / "graphenedb_api_contradiction_supersession";
  fs::remove_all(dir);

  GrapheneDB db;
  DBOptions opt;
  opt.dimension = 4;
  opt.fsync_on_commit = false;
  require(db.open(dir, opt), "open");

  uint64_t sig = signature_for(4, 9);
  uint32_t root = 0, symptom = 0, stale = 0, current = 0;

  require(db.put_node(NodeInput{
    "Root cause: checkout latency came from cache stampede after deploy.",
    {0.90f, 0.10f, 0.02f, 0.01f},
    sig,
    4401,
    true,
    false,
    false,
    {{"kind", "root_cause"}, {"status", "current"}}
  }, &root), "put root");

  require(db.put_node(NodeInput{
    "Symptom: checkout requests timed out for premium customers.",
    {0.10f, 0.90f, 0.02f, 0.01f},
    sig,
    4401,
    false,
    true,
    false,
    {{"kind", "symptom"}}
  }, &symptom), "put symptom");

  require(db.put_node(NodeInput{
    "Old hypothesis: database pool exhaustion caused checkout latency.",
    {0.85f, 0.15f, 0.04f, 0.01f},
    sig,
    4401,
    false,
    false,
    false,
    {{"kind", "hypothesis"}, {"status", "superseded"}}
  }, &stale), "put stale");

  require(db.put_node(NodeInput{
    "Current action: add cache request coalescing and protect hot keys.",
    {0.80f, 0.20f, 0.05f, 0.02f},
    sig,
    4401,
    false,
    false,
    true,
    {{"kind", "action"}, {"status", "current"}}
  }, &current), "put current");

  require(db.put_edge(EdgeInput{root, symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.96, {{"source", "postmortem"}}}), "root->symptom");
  require(db.put_edge(EdgeInput{root, stale, EdgeOrigin::Observed, EdgeRole::Contradicts, 0.90, {{"source", "postmortem"}}}), "root contradicts stale");
  require(db.put_edge(EdgeInput{current, stale, EdgeOrigin::Observed, EdgeRole::Supersedes, 0.92, {{"source", "decision-log"}}}), "current supersedes stale");

  auto bundle = db.causal_search({0.10f, 0.90f, 0.02f, 0.01f}, sig, QueryMode::Balanced);
  std::cout << "target_node=" << bundle.target_node << "\n";
  std::cout << "confidence=" << bundle.confidence << "\n";
  std::cout << "contradiction_ratio=" << bundle.contradiction << "\n";
  for (const auto& why : bundle.why_retrieved) std::cout << "why=" << why << "\n";

  auto current_ids = db.metadata_search("status", "current");
  auto superseded_ids = db.metadata_search("status", "superseded");
  std::cout << "current_count=" << current_ids.size() << "\n";
  std::cout << "superseded_count=" << superseded_ids.size() << "\n";

  require(db.validate(), "validate");
  require(db.close(), "close");
  fs::remove_all(dir);
  return 0;
}
