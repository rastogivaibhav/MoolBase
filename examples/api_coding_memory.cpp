#include "graphene/db.hpp"
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

int main() {
  fs::path dir = fs::temp_directory_path() / "graphenedb_api_coding_memory";
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions opt; opt.dimension = 4; opt.fsync_on_commit = false;
  auto st = db.open(dir, opt);
  if (!st) { std::cerr << st.message << "\n"; return 2; }

  uint64_t sig = signature_for(3, 2);
  NodeInput adr{"ADR-007: split checkout pricing module because migration latency created retry storms.", {0.8f,0.1f,0.1f,0.0f}, sig, 7007, true, false, false, {{"type","adr"},{"module","checkout-pricing"},{"status","current"}}};
  NodeInput bug{"BUG-331: retry storm appeared when the monolith pricing call exceeded 600ms.", {0.1f,0.8f,0.1f,0.0f}, sig, 7007, false, true, false, {{"type","bug"},{"module","checkout-pricing"}}};
  uint32_t adr_id=0, bug_id=0;
  db.put_node(adr, &adr_id); db.put_node(bug, &bug_id);
  EdgeInput e{adr_id, bug_id, EdgeOrigin::Observed, EdgeRole::Causal, 0.92, {{"evidence","incident-review"}}};
  db.put_edge(e);

  auto result = db.causal_search({0.1f,0.85f,0.05f,0.0f}, sig, QueryMode::Empirical);
  std::cout << "coding_memory_target=" << result.target_node << " confidence=" << result.confidence << "\n";
  for (const auto& why : result.why_retrieved) std::cout << "why=" << why << "\n";
  db.close();
  fs::remove_all(dir);
}
