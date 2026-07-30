#include "graphene/db.hpp"
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> vec(float base) {
  return {base, base + 0.01f, base + 0.02f, base + 0.03f};
}

static NodeInput memory(std::string text, float base, uint64_t sig, uint32_t incident, LatticeCoord coord, bool root=false, bool symptom=false) {
  NodeInput n;
  n.content = std::move(text);
  n.vector = vec(base);
  n.signature = sig;
  n.incident = incident;
  n.root = root;
  n.symptom = symptom;
  n.lattice = coord;
  return n;
}

int main() {
  fs::path dir = fs::temp_directory_path() / "graphenedb_lattice_demo";
  fs::remove_all(dir);

  DBOptions opt;
  opt.dimension = 4;
  opt.require_lattice = true;
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.min_candidate_floor = 4;

  GrapheneDB db;
  auto st = db.open(dir, opt);
  if (!st) { std::cerr << st.message << "\n"; return 1; }

  const uint64_t sig = signature_for(1, 4);
  uint32_t root = 0, deploy = 0, symptom = 0, distant = 0;
  db.put_node(memory("root: ingress migration changed checkout path", 0.10f, sig, 42, {0, 0, 0}, true), &root);
  db.put_node(memory("neighbor: deployment touched gateway routing", 0.20f, sig, 42, {1, 0, 0}), &deploy);
  db.put_node(memory("anchor: checkout timeouts after migration", 0.21f, sig, 42, {1, -1, 0}, false, true), &symptom);
  db.put_node(memory("distant strained note from another layer", 0.22f, sig, 42, {4, 4, 1}), &distant);

  EdgeInput e1{root, deploy, EdgeOrigin::Observed, EdgeRole::Causal, 0.95};
  e1.bond_type = BondType::Sigma;
  e1.layer_coupling = LayerCoupling::SameLayer;
  e1.bond_strength = 0.95;
  db.put_edge(e1);

  EdgeInput e2{deploy, symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.93};
  e2.bond_type = BondType::Pi;
  e2.layer_coupling = LayerCoupling::SameLayer;
  e2.bond_strength = 0.9;
  db.put_edge(e2);

  EdgeInput defect{root, distant, EdgeOrigin::Observed, EdgeRole::Supports, 0.45};
  defect.bond_type = BondType::Defect;
  defect.defect_type = DefectType::Strain;
  defect.bond_strength = 0.5;
  db.put_edge(defect);

  auto bundle = db.causal_search(vec(0.21f), sig, QueryMode::Empirical);
  if (bundle.abstain) {
    std::cout << "ABSTAIN " << bundle.reason << "\n";
  } else {
    std::cout << "target_root=" << bundle.target_node << " confidence=" << bundle.confidence
              << " lattice_score=" << bundle.lattice_score << "\n";
    for (const auto& why : bundle.why_retrieved) std::cout << "why=" << why << "\n";
  }

  db.close();
  return 0;
}
