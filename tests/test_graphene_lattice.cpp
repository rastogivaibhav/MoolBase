#include "graphene/db.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> vec(float base) {
  std::vector<float> out(4);
  for (size_t i = 0; i < out.size(); ++i) out[i] = base + static_cast<float>(i) * 0.01f;
  return out;
}

static NodeInput node(const std::string& text, float base, uint64_t sig, uint32_t incident, LatticeCoord coord, bool root=false, bool symptom=false) {
  NodeInput n;
  n.content = text;
  n.vector = vec(base);
  n.signature = sig;
  n.incident = incident;
  n.root = root;
  n.symptom = symptom;
  n.lattice = coord;
  return n;
}

static void require(Status st, const char* msg) {
  if (!st) {
    std::cerr << "FAIL: " << msg << ": " << st.message << "\n";
    std::abort();
  }
}

int main() {
  fs::path base = fs::temp_directory_path() / "graphenedb_lattice_tests";
  fs::remove_all(base);
  DBOptions opt;
  opt.dimension = 4;
  opt.require_lattice = true;
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.min_candidate_floor = 4;

  uint64_t sig = signature_for(2, 3);
  uint32_t root = 0, neighbor = 0, symptom = 0, defect = 0;
  {
    GrapheneDB db;
    require(db.open(base, opt), "open lattice db");

    NodeInput missing;
    missing.content = "missing lattice";
    missing.vector = vec(0.1f);
    auto st = db.put_node(missing);
    assert(!st && st.code == ErrorCode::InvalidInput);

    require(db.put_node(node("root cause", 0.1f, sig, 7, {0, 0, 0}, true), &root), "put root");
    require(db.put_node(node("same-layer neighbor", 0.2f, sig, 7, {1, 0, 0}), &neighbor), "put neighbor");
    require(db.put_node(node("symptom anchor", 0.21f, sig, 7, {1, -1, 0}, false, true), &symptom), "put symptom");
    require(db.put_node(node("defect shortcut", 0.22f, sig, 7, {4, 4, 0}), &defect), "put defect");

    auto dup = node("duplicate coord", 0.3f, sig, 8, {1, 0, 0});
    st = db.put_node(dup);
    assert(!st && st.code == ErrorCode::InvalidInput);

    EdgeInput e1{root, neighbor, EdgeOrigin::Observed, EdgeRole::Causal, 0.95};
    e1.bond_type = BondType::Sigma;
    e1.layer_coupling = LayerCoupling::SameLayer;
    e1.bond_strength = 0.9;
    require(db.put_edge(e1), "put sigma bond");

    EdgeInput e2{neighbor, symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.95};
    e2.bond_type = BondType::Pi;
    e2.layer_coupling = LayerCoupling::SameLayer;
    require(db.put_edge(e2), "put pi bond");

    EdgeInput bad{root, defect, EdgeOrigin::Observed, EdgeRole::Causal, 0.9};
    bad.bond_type = BondType::Sigma;
    bad.layer_coupling = LayerCoupling::SameLayer;
    st = db.put_edge(bad);
    assert(!st && st.code == ErrorCode::EdgeInvalid);

    EdgeInput defect_edge{root, defect, EdgeOrigin::Observed, EdgeRole::Supports, 0.6};
    defect_edge.bond_type = BondType::Defect;
    defect_edge.defect_type = DefectType::Strain;
    defect_edge.bond_strength = 0.5;
    require(db.put_edge(defect_edge), "put defect bond");

    auto one_hop = db.lattice_neighbors(root, 1);
    assert(one_hop.size() == 2);
    assert(one_hop[0] == neighbor);
    assert(one_hop[1] == defect);
    auto two_hop = db.lattice_neighbors(root, 2);
    assert(two_hop.size() == 3);
    assert(two_hop[0] == neighbor);
    assert(two_hop[1] == symptom);
    assert(two_hop[2] == defect);

    auto b = db.causal_search(vec(0.21f), sig, QueryMode::Empirical);
    assert(!b.abstain);
    assert(b.target_node == root);
    assert(b.lattice_score > 0.0);
    assert(!b.lattice_explanation.empty());

    std::string report;
    require(db.validate(&report), "validate lattice");
    uint64_t before_delete = db.snapshot();
    require(db.delete_node(defect), "delete defect");
    auto current_neighbors = db.lattice_neighbors(root, 1);
    assert(current_neighbors.size() == 1 && current_neighbors[0] == neighbor);
    auto old_neighbors = db.lattice_neighbors(root, 1, before_delete);
    assert(old_neighbors.size() == 2 && old_neighbors[1] == defect);
    require(db.compact(), "compact lattice");
    require(db.close(), "close lattice");
  }

  {
    GrapheneDB db;
    require(db.open(base, opt), "reopen lattice db");
    auto n = db.get_node(root);
    assert(n.has_value());
    assert(n->lattice.has_value());
    assert(n->lattice->q == 0 && n->lattice->r == 0 && n->lattice->layer == 0);
    auto e = db.get_edge(0);
    assert(e.has_value());
    assert(e->bond_type == BondType::Sigma);
    assert(e->bond_strength > 0.89);
    std::string out;
    require(db.inspect(&out), "inspect lattice");
    assert(out.find("manifest_format=1") != std::string::npos);
    assert(out.find("storage_format=2") != std::string::npos);
    assert(out.find("wal_frame_format=1") != std::string::npos);
    assert(out.find("lattice_format=1") != std::string::npos);
    assert(out.find("extraction_format=1") != std::string::npos);
    assert(out.find("lattice_nodes=3") != std::string::npos);
    require(db.close(), "close reopened lattice");
  }

  std::cout << "graphenedb_lattice_tests_passed=true\n";
  return 0;
}
