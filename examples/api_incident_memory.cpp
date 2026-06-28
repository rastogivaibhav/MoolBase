#include "graphene/db.hpp"
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

int main() {
  fs::path dir = fs::temp_directory_path() / "graphenedb_api_incident_memory";
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions opt; opt.dimension = 3; opt.fsync_on_commit = false;
  if (auto st = db.open(dir, opt); !st) { std::cerr << st.message << "\n"; return 2; }

  uint64_t sig = signature_for(5, 8);
  uint32_t root=0, symptom=0;
  db.put_node(NodeInput{"Root cause: ingress timeout dropped from 30s to 3s.", {0.9f,0.1f,0.0f}, sig, 1182, true, false, false, {{"type","root_cause"},{"service","gateway"}}}, &root);
  db.put_node(NodeInput{"Symptom: API timeout spike after release 2.3.", {0.1f,0.9f,0.0f}, sig, 1182, false, true, false, {{"type","incident"},{"service","gateway"}}}, &symptom);
  db.put_edge(EdgeInput{root, symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.94, {{"source","postmortem"}}});

  auto ids = db.metadata_search("service", "gateway");
  std::cout << "metadata_gateway_count=" << ids.size() << "\n";
  auto bundle = db.causal_search({0.1f,0.92f,0.0f}, sig, QueryMode::Empirical);
  std::cout << "incident_target=" << bundle.target_node << " confidence=" << bundle.confidence << "\n";
  db.close();
  fs::remove_all(dir);
}
