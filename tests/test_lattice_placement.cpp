#include "graphene/db.hpp"
#include "graphene/lattice_placement.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> vec(float base) {
  return {base, base + 0.01f, base + 0.02f, base + 0.03f};
}

static NodeInput memory(std::string text, uint32_t incident, uint64_t sig, float base, bool root=false, bool symptom=false) {
  NodeInput n;
  n.content = std::move(text);
  n.vector = vec(base);
  n.incident = incident;
  n.signature = sig;
  n.root = root;
  n.symptom = symptom;
  return n;
}

static void require(Status st, const char* msg) {
  if (!st) {
    std::cerr << "FAIL: " << msg << ": " << st.message << "\n";
    std::abort();
  }
}

int main() {
  uint64_t sig_a = signature_for(1, 1);
  uint64_t sig_b = signature_for(2, 2);
  std::vector<NodeInput> raw;
  raw.push_back(memory("b symptom", 20, sig_b, 0.5f, false, true));
  raw.push_back(memory("a symptom", 10, sig_a, 0.2f, false, true));
  raw.push_back(memory("a root", 10, sig_a, 0.1f, true));
  raw.push_back(memory("b root", 20, sig_b, 0.4f, true));

  LatticePlacementOptions opt;
  opt.strategy = PlacementStrategy::SemanticGroups;
  opt.layer = 2;
  LatticePlacementResult placed;
  require(assign_lattice_batch(raw, opt, &placed), "semantic placement");

  assert(placed.batch.nodes.size() == 4);
  assert(placed.batch.nodes[0].content == "a root");
  assert(placed.batch.nodes[1].content == "a symptom");
  assert(placed.batch.nodes[2].content == "b root");
  assert(placed.batch.nodes[3].content == "b symptom");
  assert(placed.batch.nodes[0].lattice->layer == 2);
  assert(placed.batch.edges.size() == 3);
  assert(placed.batch.edges[0].bond_type == BondType::Sigma);
  assert(placed.batch.edges[1].bond_type == BondType::Synthetic);
  assert(placed.batch.edges[1].defect_type == DefectType::Boundary);
  assert(placed.batch.edges[2].bond_type == BondType::Sigma);

  fs::path dir = fs::temp_directory_path() / "graphenedb_lattice_placement_test";
  fs::remove_all(dir);
  DBOptions dbopt;
  dbopt.dimension = 4;
  dbopt.require_lattice = true;
  dbopt.tuning.enable_lattice_retrieval = true;
  dbopt.tuning.min_candidate_floor = 4;
  GrapheneDB db;
  require(db.open(dir, dbopt), "open placement db");
  BatchResult ids;
  require(db.put_batch(placed.batch, &ids), "put semantic placement batch");
  assert(ids.node_ids.size() == 4);
  assert(ids.edge_ids.size() == 3);
  require(db.validate(), "validate placement db");
  auto b = db.causal_search(vec(0.2f), sig_a, QueryMode::Empirical);
  assert(!b.abstain);
  assert(b.lattice_score > 0.0);
  require(db.close(), "close placement db");

  std::cout << "graphenedb_lattice_placement_tests_passed=true\n";
  return 0;
}
