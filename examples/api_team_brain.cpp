#include "graphene/db.hpp"
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

int main() {
  fs::path dir = fs::temp_directory_path() / "graphenedb_api_team_brain";
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions opt; opt.dimension = 3; opt.fsync_on_commit = false;
  if (auto st = db.open(dir, opt); !st) { std::cerr << st.message << "\n"; return 2; }

  uint64_t sig = signature_for(1, 12);
  uint32_t customer=0, decision=0, superseded=0;
  db.put_node(NodeInput{"Customer context: parents need controlled AI tutor mode before public launch.", {0.9f,0.05f,0.05f}, sig, 9001, true, false, false, {{"type","customer_context"}}}, &customer);
  db.put_node(NodeInput{"Decision: ship parent-led practice first; defer open student chat.", {0.2f,0.75f,0.05f}, sig, 9001, false, true, false, {{"type","decision"},{"status","current"}}}, &decision);
  db.put_node(NodeInput{"Old decision: allow unrestricted student chat in v1.", {0.2f,0.70f,0.10f}, sig, 9001, false, true, false, {{"type","decision"},{"status","superseded"}}}, &superseded);
  db.put_edge(EdgeInput{customer, decision, EdgeOrigin::Observed, EdgeRole::Causal, 0.95, {}});
  db.put_edge(EdgeInput{decision, superseded, EdgeOrigin::Observed, EdgeRole::Supersedes, 0.90, {}});

  auto current_decisions = db.metadata_search("status", "current");
  std::cout << "current_decisions=" << current_decisions.size() << "\n";
  auto bundle = db.causal_search({0.2f,0.75f,0.05f}, sig, QueryMode::Balanced);
  std::cout << "team_brain_target=" << bundle.target_node << " confidence=" << bundle.confidence << "\n";
  db.close();
  fs::remove_all(dir);
}
