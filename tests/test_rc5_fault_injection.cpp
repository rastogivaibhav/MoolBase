#include "graphene/db.hpp"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> vec(float base) {
  return {base, base + 0.01f, base + 0.02f, base + 0.03f};
}

static NodeInput node(const std::string& text, float base, LatticeCoord coord, bool root=false) {
  NodeInput n;
  n.content = text;
  n.vector = vec(base);
  n.signature = signature_for(4, 6);
  n.incident = 406;
  n.root = root;
  n.lattice = coord;
  return n;
}

static ExtractionInput extraction_input() {
  ExtractionInput input;
  input.source_id = "fault-extraction";
  input.incident = 507;
  input.signature = signature_for(5, 7);
  input.layer = 0;

  ExtractionNode root;
  root.external_id = "root";
  root.content = "fault extraction root";
  root.vector = vec(0.3f);
  root.role = ExtractionRole::Root;
  input.nodes.push_back(root);

  ExtractionNode neighbor;
  neighbor.external_id = "neighbor";
  neighbor.content = "fault extraction neighbor";
  neighbor.vector = vec(0.4f);
  neighbor.role = ExtractionRole::Symptom;
  input.nodes.push_back(neighbor);

  ExtractionRelation rel;
  rel.from_external_id = "root";
  rel.to_external_id = "neighbor";
  rel.role = EdgeRole::Supports;
  rel.bond_type = BondType::Sigma;
  rel.layer_coupling = LayerCoupling::SameLayer;
  rel.confidence = 0.9;
  rel.bond_strength = 0.85;
  input.relations.push_back(rel);
  return input;
}

static void require(Status st, const char* msg) {
  if (!st) {
    std::cerr << "FAIL: " << msg << ": " << st.message << "\n";
    std::abort();
  }
}

static void set_env(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

static void unset_env(const char* name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

int main() {
  fs::path base = fs::temp_directory_path() / "graphenedb_rc5_fault_injection";
  fs::remove_all(base);

  DBOptions opt;
  opt.dimension = 4;
  opt.require_lattice = true;
  opt.fsync_on_commit = false;
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.min_candidate_floor = 4;
  DBOptions fsync_opt = opt;
  fsync_opt.fsync_on_commit = true;

  // WAL append failure must leave no in-memory batch state behind.
  fs::path wal_fail = base / "wal_fail";
  {
    GrapheneDB db;
    require(db.open(wal_fail, opt), "open wal fail db");
    BatchInput batch;
    batch.nodes.push_back(node("root", 0.1f, {0, 0, 0}, true));
    batch.nodes.push_back(node("neighbor", 0.2f, {1, 0, 0}));
    EdgeInput e{0, 1, EdgeOrigin::Observed, EdgeRole::Causal, 0.9};
    e.bond_type = BondType::Sigma;
    e.layer_coupling = LayerCoupling::SameLayer;
    batch.edges.push_back(e);
    set_env("GRAPHENEDB_TEST_FAIL_WAL_APPEND", "1");
    auto st = db.put_batch(batch);
    unset_env("GRAPHENEDB_TEST_FAIL_WAL_APPEND");
    assert(!st && st.code == ErrorCode::IoError);
    assert(db.node_count() == 0);
    assert(db.edge_count() == 0);
    require(db.validate(), "validate after failed WAL append");
    require(db.close(), "close wal fail db");
  }
  {
    GrapheneDB db;
    require(db.open(wal_fail, opt), "reopen wal fail db");
    assert(db.node_count() == 0);
    assert(db.edge_count() == 0);
    require(db.close(), "close reopened wal fail db");
  }

  // WAL write failure simulates disk-pressure before bytes are written.
  fs::path wal_write_fail = base / "wal_write_fail";
  {
    GrapheneDB db;
    require(db.open(wal_write_fail, opt), "open wal write fail db");
    BatchInput batch;
    batch.nodes.push_back(node("write-root", 0.1f, {0, 0, 0}, true));
    batch.nodes.push_back(node("write-neighbor", 0.2f, {1, 0, 0}));
    EdgeInput e{0, 1, EdgeOrigin::Observed, EdgeRole::Causal, 0.9};
    e.bond_type = BondType::Sigma;
    e.layer_coupling = LayerCoupling::SameLayer;
    batch.edges.push_back(e);
    set_env("GRAPHENEDB_TEST_FAIL_WAL_WRITE", "1");
    auto st = db.put_batch(batch);
    unset_env("GRAPHENEDB_TEST_FAIL_WAL_WRITE");
    assert(!st && st.code == ErrorCode::IoError);
    assert(db.node_count() == 0);
    assert(db.edge_count() == 0);
    require(db.validate(), "validate after failed WAL write");
    require(db.close(), "close wal write fail db");
  }
  {
    GrapheneDB db;
    require(db.open(wal_write_fail, opt), "reopen wal write fail db");
    assert(db.node_count() == 0);
    assert(db.edge_count() == 0);
    require(db.close(), "close reopened wal write fail db");
  }

  // WAL fsync failure rolls the append back to the previous WAL boundary before
  // returning, so the failed commit does not replay later.
  fs::path wal_fsync_fail = base / "wal_fsync_fail";
  {
    GrapheneDB db;
    require(db.open(wal_fsync_fail, fsync_opt), "open wal fsync fail db");
    set_env("GRAPHENEDB_TEST_FAIL_WAL_FSYNC", "1");
    auto st = db.put_node(node("fsync-root", 0.1f, {0, 0, 0}, true));
    unset_env("GRAPHENEDB_TEST_FAIL_WAL_FSYNC");
    assert(!st && st.code == ErrorCode::IoError);
    assert(db.node_count() == 0);
    require(db.validate(), "validate live after failed WAL fsync");
    require(db.close(), "close wal fsync fail db");
  }
  {
    GrapheneDB db;
    require(db.open(wal_fsync_fail, fsync_opt), "reopen wal fsync fail db");
    assert(db.node_count() == 0);
    require(db.validate(), "validate reopened wal fsync fail db");
    require(db.close(), "close reopened wal fsync fail db");
  }

  // WAL append failure during extraction import must leave no partial state behind.
  fs::path extraction_wal_fail = base / "extraction_wal_fail";
  {
    GrapheneDB db;
    require(db.open(extraction_wal_fail, opt), "open extraction wal fail db");
    auto input = extraction_input();
    set_env("GRAPHENEDB_TEST_FAIL_WAL_APPEND", "1");
    auto st = db.put_extraction(input);
    unset_env("GRAPHENEDB_TEST_FAIL_WAL_APPEND");
    assert(!st && st.code == ErrorCode::IoError);
    assert(db.node_count() == 0);
    assert(db.edge_count() == 0);
    assert(db.metadata_search("graphene_source_id", "fault-extraction").empty());
    require(db.validate(), "validate after failed extraction WAL append");
    require(db.close(), "close extraction wal fail db");
  }
  {
    GrapheneDB db;
    require(db.open(extraction_wal_fail, opt), "reopen extraction wal fail db");
    assert(db.node_count() == 0);
    assert(db.edge_count() == 0);
    assert(db.metadata_search("graphene_source_id", "fault-extraction").empty());
    require(db.close(), "close reopened extraction wal fail db");
  }

  // Checkpoint failure before rename leaves the live DB valid and reopenable.
  fs::path checkpoint_fail = base / "checkpoint_fail";
  {
    GrapheneDB db;
    require(db.open(checkpoint_fail, opt), "open checkpoint fail db");
    uint32_t a = 0, b = 0;
    require(db.put_node(node("root", 0.1f, {0, 0, 0}, true), &a), "put checkpoint root");
    require(db.put_node(node("neighbor", 0.2f, {1, 0, 0}), &b), "put checkpoint neighbor");
    EdgeInput e{a, b, EdgeOrigin::Observed, EdgeRole::Causal, 0.9};
    e.bond_type = BondType::Sigma;
    e.layer_coupling = LayerCoupling::SameLayer;
    require(db.put_edge(e), "put checkpoint edge");
    set_env("GRAPHENEDB_TEST_FAIL_CHECKPOINT_RENAME", "1");
    auto st = db.compact();
    unset_env("GRAPHENEDB_TEST_FAIL_CHECKPOINT_RENAME");
    assert(!st && st.code == ErrorCode::IoError);
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    require(db.validate(), "validate after failed checkpoint");
    require(db.close(), "close checkpoint fail db");
  }
  {
    GrapheneDB db;
    require(db.open(checkpoint_fail, opt), "reopen checkpoint fail db");
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    require(db.validate(), "validate reopened checkpoint fail db");
    require(db.close(), "close reopened checkpoint fail db");
  }

  // Checkpoint temp write failure leaves the live DB and reopened state valid.
  fs::path checkpoint_write_fail = base / "checkpoint_write_fail";
  {
    GrapheneDB db;
    require(db.open(checkpoint_write_fail, opt), "open checkpoint write fail db");
    uint32_t a = 0, b = 0;
    require(db.put_node(node("checkpoint-write-root", 0.1f, {0, 0, 0}, true), &a), "put checkpoint write root");
    require(db.put_node(node("checkpoint-write-neighbor", 0.2f, {1, 0, 0}), &b), "put checkpoint write neighbor");
    EdgeInput e{a, b, EdgeOrigin::Observed, EdgeRole::Causal, 0.9};
    e.bond_type = BondType::Sigma;
    e.layer_coupling = LayerCoupling::SameLayer;
    require(db.put_edge(e), "put checkpoint write edge");
    set_env("GRAPHENEDB_TEST_FAIL_CHECKPOINT_WRITE", "1");
    auto st = db.compact();
    unset_env("GRAPHENEDB_TEST_FAIL_CHECKPOINT_WRITE");
    assert(!st && st.code == ErrorCode::IoError);
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    require(db.validate(), "validate after failed checkpoint write");
    require(db.close(), "close checkpoint write fail db");
  }
  {
    GrapheneDB db;
    require(db.open(checkpoint_write_fail, opt), "reopen checkpoint write fail db");
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    require(db.validate(), "validate reopened checkpoint write fail db");
    require(db.close(), "close reopened checkpoint write fail db");
  }

  // Manifest rename failure during close reports an error but leaves WAL/data reopenable.
  fs::path manifest_fail = base / "manifest_fail";
  {
    GrapheneDB db;
    require(db.open(manifest_fail, opt), "open manifest fail db");
    require(db.put_node(node("manifest-root", 0.1f, {0, 0, 0}, true)), "put manifest root");
    set_env("GRAPHENEDB_TEST_FAIL_MANIFEST_RENAME", "1");
    auto st = db.close();
    unset_env("GRAPHENEDB_TEST_FAIL_MANIFEST_RENAME");
    assert(!st && st.code == ErrorCode::IoError);
  }
  {
    GrapheneDB db;
    require(db.open(manifest_fail, opt), "reopen manifest fail db");
    assert(db.node_count() == 1);
    require(db.validate(), "validate manifest fail db");
    require(db.close(), "close manifest fail db reopened");
  }

  // Backup copy failure returns an error and does not mutate the source DB.
  fs::path backup_fail = base / "backup_fail";
  fs::path backup_dst = base / "backup_dst";
  {
    GrapheneDB db;
    require(db.open(backup_fail, opt), "open backup fail db");
    require(db.put_node(node("backup-root", 0.1f, {0, 0, 0}, true)), "put backup root");
    set_env("GRAPHENEDB_TEST_FAIL_BACKUP_COPY", "1");
    auto st = db.backup(backup_dst);
    unset_env("GRAPHENEDB_TEST_FAIL_BACKUP_COPY");
    assert(!st && st.code == ErrorCode::IoError);
    assert(db.node_count() == 1);
    require(db.validate(), "validate after failed backup");
    require(db.close(), "close backup fail db");
  }
  {
    GrapheneDB db;
    require(db.open(backup_fail, opt), "reopen backup fail source");
    assert(db.node_count() == 1);
    require(db.close(), "close backup fail source");
  }

  std::cout << "graphenedb_rc5_fault_injection_passed=true\n";
  return 0;
}
