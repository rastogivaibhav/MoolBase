#include "graphene/db.hpp"
#include "graphene/lattice_placement.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> vec(float base) {
  return {base, base + 0.01f, base + 0.02f, base + 0.03f};
}

static NodeInput node(std::string text, float base, LatticeCoord coord, bool root=false, bool symptom=false) {
  NodeInput n;
  n.content = std::move(text);
  n.vector = vec(base);
  n.signature = signature_for(3, 5);
  n.incident = 99;
  n.root = root;
  n.symptom = symptom;
  n.lattice = coord;
  n.metadata["kind"] = "acid";
  return n;
}

static void require(Status st, const char* msg) {
  if (!st) {
    std::cerr << "FAIL: " << msg << ": " << st.message << "\n";
    std::abort();
  }
}

static fs::path repo_root_from_cwd() {
  fs::path p = fs::current_path();
  for (int i = 0; i < 6; ++i) {
    if (fs::exists(p / "CMakeLists.txt") && fs::exists(p / "tests" / "fixtures")) return p;
    if (!p.has_parent_path()) break;
    p = p.parent_path();
  }
  return fs::current_path();
}

static void copy_fixture_store(const fs::path& dir, const std::string& fixture_name) {
  fs::create_directories(dir);
  fs::copy_file(repo_root_from_cwd() / "tests" / "fixtures" / fixture_name, dir / "graphene.data", fs::copy_options::overwrite_existing);
}

int main() {
  fs::path base = fs::temp_directory_path() / "graphenedb_acid_lattice";
  fs::remove_all(base);

  DBOptions opt;
  opt.dimension = 4;
  opt.require_lattice = true;
  opt.fsync_on_commit = false;
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.min_candidate_floor = 4;

  // Backward compatibility: old v1 records without lattice fields still open.
  fs::path old = base / "old_v1";
  copy_fixture_store(old, "storage_v1_graphene.data");
  {
    DBOptions old_opt;
    old_opt.dimension = 4;
    GrapheneDB db;
    require(db.open(old, old_opt), "open old v1 store");
    auto n = db.get_node(0);
    assert(n.has_value());
    assert(!n->lattice.has_value());
    assert(n->metadata.at("kind") == "old");
    require(db.close(), "close old v1 store");
  }

  // Explicit v2 fixture: lattice fields and bond metadata replay from data file.
  fs::path v2 = base / "v2_lattice";
  copy_fixture_store(v2, "storage_v2_lattice_graphene.data");
  {
    GrapheneDB db;
    require(db.open(v2, opt), "open v2 lattice fixture");
    auto n = db.get_node(1);
    assert(n.has_value());
    assert(n->lattice.has_value());
    assert(n->lattice->q == 1 && n->lattice->r == 0 && n->lattice->layer == 0);
    auto e = db.get_edge(0);
    assert(e.has_value());
    assert(e->bond_type == BondType::Sigma);
    assert(e->layer_coupling == LayerCoupling::SameLayer);
    require(db.validate(), "validate v2 lattice fixture");
    require(db.close(), "close v2 lattice fixture");
  }

  fs::path dir = base / "main";
  GrapheneDB db;
  require(db.open(dir, opt), "open acid db");

  // Atomicity: invalid batch edge must not leave partial nodes behind.
  BatchInput bad;
  bad.nodes.push_back(node("batch root", 0.1f, {0, 0, 0}, true));
  bad.nodes.push_back(node("batch non-neighbor", 0.2f, {3, 3, 0}));
  EdgeInput bad_edge{0, 1, EdgeOrigin::Observed, EdgeRole::Causal, 0.9};
  bad_edge.bond_type = BondType::Sigma;
  bad_edge.layer_coupling = LayerCoupling::SameLayer;
  bad.edges.push_back(bad_edge);
  auto st = db.put_batch(bad);
  assert(!st);
  assert(db.node_count() == 0);
  assert(db.edge_count() == 0);

  // Extraction/placement foundation: deterministic hex placement + batch ingest.
  std::vector<NodeInput> raw;
  raw.push_back(node("placed root", 0.10f, {0, 0, 0}, true));
  raw.push_back(node("placed neighbor", 0.20f, {0, 0, 0}));
  raw.back().lattice.reset();
  raw.push_back(node("placed symptom", 0.21f, {0, 0, 0}, false, true));
  raw.back().lattice.reset();
  LatticePlacementOptions place;
  place.base_node_id = 0;
  place.layer = 0;
  place.incident = 99;
  place.signature = signature_for(3, 5);
  LatticePlacementResult placed;
  require(assign_lattice_batch(raw, place, &placed), "assign lattice batch");
  BatchResult ids;
  require(db.put_batch(placed.batch, &ids), "put placed batch");
  assert(ids.node_ids.size() == 3);
  assert(ids.edge_ids.size() == 2);

  // Consistency: cross-layer bond requires explicit cross-layer coupling.
  NodeInput upper = node("upper layer", 0.25f, {0, 0, 1});
  uint32_t upper_id = 0;
  require(db.put_node(upper, &upper_id), "put upper layer");
  EdgeInput cross{0, upper_id, EdgeOrigin::Observed, EdgeRole::Supports, 0.8};
  cross.bond_type = BondType::VanDerWaals;
  cross.layer_coupling = LayerCoupling::None;
  st = db.put_edge(cross);
  assert(!st && st.code == ErrorCode::EdgeInvalid);
  cross.layer_coupling = LayerCoupling::VanDerWaals;
  require(db.put_edge(cross), "put valid cross-layer");

  // Extraction records should also survive compact + backup restore with
  // source-scoped identity metadata and validated lattice bonds.
  ExtractionInput extraction;
  extraction.source_id = "acid-extraction";
  extraction.incident = 199;
  extraction.signature = signature_for(4, 7);
  ExtractionNode eroot;
  eroot.external_id = "root";
  eroot.content = "acid extraction root";
  eroot.vector = vec(0.31f);
  eroot.role = ExtractionRole::Root;
  extraction.nodes.push_back(eroot);
  ExtractionNode eneighbor;
  eneighbor.external_id = "neighbor";
  eneighbor.content = "acid extraction neighbor";
  eneighbor.vector = vec(0.32f);
  eneighbor.role = ExtractionRole::Symptom;
  extraction.nodes.push_back(eneighbor);
  ExtractionRelation erel;
  erel.from_external_id = "root";
  erel.to_external_id = "neighbor";
  erel.role = EdgeRole::Supports;
  erel.bond_type = BondType::Sigma;
  erel.layer_coupling = LayerCoupling::SameLayer;
  extraction.relations.push_back(erel);
  ExtractionResult extraction_ids;
  require(db.put_extraction(extraction, &extraction_ids), "put acid extraction");
  assert(extraction_ids.inserted_node_ids.size() == 2);
  assert(extraction_ids.inserted_edge_ids.size() == 1);
  assert(db.metadata_search("graphene_source_id", "acid-extraction").size() == 2);

  // Isolation: old snapshot sees deleted node, current snapshot does not.
  uint64_t before_delete = db.snapshot();
  require(db.delete_node(ids.node_ids[2]), "delete symptom");
  assert(db.get_node(ids.node_ids[2]).has_value() == false);
  assert(db.get_node(ids.node_ids[2], before_delete).has_value() == true);

  // Reader concurrency smoke under shared locks.
  std::thread reader([&] {
    for (int i = 0; i < 50; ++i) {
      auto r = db.vector_search(vec(0.2f), 3);
      (void)r;
    }
  });
  reader.join();

  std::string report;
  require(db.validate(&report), "validate acid db");
  require(db.compact(), "compact acid db");
  fs::path backup = base / "backup";
  require(db.backup(backup), "backup acid db");
  require(db.close(), "close acid db");

  // Durability: compacted and backed-up stores preserve lattice fields.
  {
    GrapheneDB reopened;
    require(reopened.open(dir, opt), "reopen compacted acid db");
    auto n = reopened.get_node(ids.node_ids[0]);
    assert(n.has_value() && n->lattice.has_value());
    auto e = reopened.get_edge(ids.edge_ids[0]);
    assert(e.has_value() && e->bond_type == BondType::Sigma);
    assert(reopened.metadata_search("graphene_source_id", "acid-extraction").size() == 2);
    require(reopened.close(), "close reopened acid db");
  }
  {
    GrapheneDB copy;
    require(copy.open(backup, opt), "open acid backup");
    assert(copy.node_count() == 5);
    assert(copy.metadata_search("graphene_source_id", "acid-extraction").size() == 2);
    auto restored_root = copy.get_node(extraction_ids.inserted_node_ids[0]);
    assert(restored_root.has_value());
    assert(restored_root->metadata.at("graphene_external_id") == "root");
    auto restored_edge = copy.get_edge(extraction_ids.inserted_edge_ids[0]);
    assert(restored_edge.has_value());
    assert(restored_edge->bond_type == BondType::Sigma);
    require(copy.validate(), "validate acid backup");
    require(copy.close(), "close acid backup");
  }

  std::cout << "graphenedb_acid_lattice_tests_passed=true\n";
  return 0;
}
