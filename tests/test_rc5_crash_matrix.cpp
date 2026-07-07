#include "graphene/db.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace graphene;
namespace fs = std::filesystem;

static uint64_t fnv1a_test(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  return h;
}

static std::string hex_encode(const std::string& input) {
  static const char* hex = "0123456789abcdef";
  std::string out;
  for (unsigned char c : input) { out.push_back(hex[c >> 4]); out.push_back(hex[c & 0x0f]); }
  return out;
}

static std::string frame(const std::string& payload) {
  return std::to_string(payload.size()) + "|" + std::to_string(fnv1a_test(payload)) + "|" + payload + "\n";
}

static std::string manifest_body(uint32_t dim = 4, uint32_t storage_format = 2) {
  std::ostringstream os;
  os << "graphenedb_manifest_v1\n";
  os << "storage_format=" << storage_format << "\n";
  os << "wal_frame_format=1\n";
  os << "lattice_format=1\n";
  os << "extraction_format=1\n";
  os << "dimension=" << dim << "\n";
  os << "version=1\n";
  os << "next_node_id=0\n";
  os << "next_edge_id=0\n";
  os << "next_txid=1\n";
  return os.str();
}

static void write_manifest_file(const fs::path& dir, const std::string& body, bool valid_checksum = true) {
  std::ofstream out(dir / "MANIFEST", std::ios::trunc);
  out << body;
  out << "checksum=" << (valid_checksum ? fnv1a_test(body) : 1) << "\n";
}

static std::string vec_csv(float base) {
  std::ostringstream os;
  os << base << "," << (base + 0.01f) << "," << (base + 0.02f) << "," << (base + 0.03f);
  return os.str();
}

static std::string metadata_payload(const std::vector<std::pair<std::string, std::string>>& metadata) {
  std::ostringstream os;
  bool first = true;
  for (const auto& kv : metadata) {
    if (!first) os << ';';
    first = false;
    os << hex_encode(kv.first) << "=" << hex_encode(kv.second);
  }
  return os.str();
}

static std::string node_payload(uint64_t tx, uint32_t id, uint64_t created, const std::string& content, float base, LatticeCoord coord, bool root=false, bool symptom=false, std::vector<std::pair<std::string, std::string>> metadata = {{"kind", "crash"}}) {
  std::ostringstream os;
  os << "PUT_NODE\t" << tx << "\t" << id << "\t" << created << "\t18446744073709551615\t"
     << signature_for(6, 7) << "\t607\t" << (root ? 1 : 0) << "\t" << (symptom ? 1 : 0)
     << "\t0\t" << hex_encode(content) << "\t" << vec_csv(base) << "\t"
     << coord.q << "," << coord.r << "," << coord.layer << "\t0\t" << metadata_payload(metadata);
  return os.str();
}

static std::string edge_payload(uint64_t tx, uint32_t id, uint32_t from, uint32_t to, BondType bond=BondType::Sigma, LayerCoupling coupling=LayerCoupling::SameLayer, std::vector<std::pair<std::string, std::string>> metadata = {}) {
  std::ostringstream os;
  os << "PUT_EDGE\t" << tx << "\t" << id << "\t" << from << "\t" << to << "\t0\t4\t0.900000\t3\t18446744073709551615\t"
     << static_cast<int>(bond) << "\t0\t" << static_cast<int>(coupling) << "\t0.850000\t" << metadata_payload(metadata);
  return os.str();
}

static void require(Status st, const char* what) {
  if (!st) { std::cerr << "FAIL " << what << ": " << st.message << "\n"; std::abort(); }
}

static DBOptions opts() {
  DBOptions opt;
  opt.dimension = 4;
  opt.require_lattice = true;
  opt.fsync_on_commit = false;
  opt.tuning.enable_lattice_retrieval = true;
  opt.tuning.min_candidate_floor = 4;
  return opt;
}

int main() {
  fs::path base = fs::temp_directory_path() / "graphenedb_rc5_crash_matrix";
  fs::remove_all(base);
  auto opt = opts();

  // Committed lattice batch is replayed as a unit.
  fs::path committed = base / "committed_batch";
  {
    GrapheneDB db; require(db.open(committed, opt), "open committed"); require(db.close(), "close committed empty");
    std::ofstream wal(committed / "graphene.wal", std::ios::app);
    wal << frame("BEGIN\t9001");
    wal << frame(node_payload(9001, 0, 1, "batch-root", 0.1f, {0, 0, 0}, true));
    wal << frame(node_payload(9001, 1, 2, "batch-neighbor", 0.2f, {1, 0, 0}));
    wal << frame(edge_payload(9001, 0, 0, 1));
    wal << frame("COMMIT\t9001");
  }
  {
    GrapheneDB db; require(db.open(committed, opt), "reopen committed batch");
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    auto b = db.causal_search({0.2f, 0.21f, 0.22f, 0.23f}, signature_for(6, 7), QueryMode::Empirical);
    assert(!b.abstain);
    assert(b.lattice_score > 0.0);
    require(db.validate(), "validate committed batch");
    require(db.close(), "close committed batch");
  }

  // Committed extraction-shaped WAL records replay with identity metadata,
  // and put_extraction() treats replayed records as already imported.
  fs::path extraction_replay = base / "extraction_replay";
  {
    GrapheneDB db; require(db.open(extraction_replay, opt), "open extraction replay"); require(db.close(), "close extraction replay empty");
    std::ofstream wal(extraction_replay / "graphene.wal", std::ios::app);
    wal << frame("BEGIN\t9010");
    wal << frame(node_payload(9010, 0, 1, "extraction-root", 0.1f, {0, 0, 0}, true, false, {
      {"graphene_source_id", "replayed-pack"},
      {"graphene_external_id", "root"},
      {"graphene_scoped_external_id", std::string("replayed-pack") + "\x1e" + "root"},
      {"graphene_ingest", "extraction-v1"}
    }));
    wal << frame(node_payload(9010, 1, 2, "extraction-neighbor", 0.2f, {1, 0, 0}, false, true, {
      {"graphene_source_id", "replayed-pack"},
      {"graphene_external_id", "neighbor"},
      {"graphene_scoped_external_id", std::string("replayed-pack") + "\x1e" + "neighbor"},
      {"graphene_ingest", "extraction-v1"}
    }));
    wal << frame(edge_payload(9010, 0, 0, 1, BondType::Sigma, LayerCoupling::SameLayer, {
      {"graphene_source_id", "replayed-pack"},
      {"graphene_relation_key", std::string("replayed-pack") + "\x1e" + "root" + "\x1e" + "neighbor" + "\x1e" + std::to_string(static_cast<int>(EdgeRole::Supports))},
      {"graphene_ingest", "extraction-v1"}
    }));
    wal << frame("COMMIT\t9010");
  }
  {
    GrapheneDB db; require(db.open(extraction_replay, opt), "reopen extraction replay");
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    assert(db.metadata_search("graphene_source_id", "replayed-pack").size() == 2);
    ExtractionInput input;
    input.source_id = "replayed-pack";
    input.incident = 607;
    input.signature = signature_for(6, 7);
    ExtractionNode root;
    root.external_id = "root";
    root.content = "extraction-root";
    root.vector = {0.1f, 0.11f, 0.12f, 0.13f};
    root.role = ExtractionRole::Root;
    input.nodes.push_back(root);
    ExtractionNode neighbor;
    neighbor.external_id = "neighbor";
    neighbor.content = "extraction-neighbor";
    neighbor.vector = {0.2f, 0.21f, 0.22f, 0.23f};
    neighbor.role = ExtractionRole::Symptom;
    input.nodes.push_back(neighbor);
    ExtractionRelation rel;
    rel.from_external_id = "root";
    rel.to_external_id = "neighbor";
    rel.role = EdgeRole::Supports;
    input.relations.push_back(rel);
    ExtractionResult result;
    require(db.put_extraction(input, &result), "idempotent extraction after WAL replay");
    assert(result.inserted_node_ids.empty());
    assert(result.inserted_edge_ids.empty());
    assert(result.existing_node_ids.size() == 2);
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    require(db.validate(), "validate extraction replay");
    require(db.close(), "close extraction replay");
  }

  // Missing COMMIT drops the whole batch.
  fs::path uncommitted = base / "uncommitted_batch";
  {
    GrapheneDB db; require(db.open(uncommitted, opt), "open uncommitted"); require(db.close(), "close uncommitted empty");
    std::ofstream wal(uncommitted / "graphene.wal", std::ios::app);
    wal << frame("BEGIN\t9002");
    wal << frame(node_payload(9002, 0, 1, "lost-root", 0.1f, {0, 0, 0}, true));
    wal << frame(node_payload(9002, 1, 2, "lost-neighbor", 0.2f, {1, 0, 0}));
    wal << frame(edge_payload(9002, 0, 0, 1));
  }
  {
    GrapheneDB db; require(db.open(uncommitted, opt), "reopen uncommitted batch");
    assert(db.node_count() == 0);
    assert(db.edge_count() == 0);
    require(db.close(), "close uncommitted batch");
  }

  // Torn tail after COMMIT is ignored.
  {
    std::ofstream wal(committed / "graphene.wal", std::ios::app);
    wal << "999|123|TORN_RC5";
  }
  {
    GrapheneDB db; require(db.open(committed, opt), "reopen committed torn tail");
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    require(db.close(), "close committed torn tail");
  }

  // Invalid committed lattice topology is rejected by validate(), preserving controlled failure.
  fs::path invalid = base / "invalid_lattice";
  {
    GrapheneDB db; require(db.open(invalid, opt), "open invalid"); require(db.close(), "close invalid empty");
    std::ofstream wal(invalid / "graphene.wal", std::ios::app);
    wal << frame("BEGIN\t9003");
    wal << frame(node_payload(9003, 0, 1, "invalid-root", 0.1f, {0, 0, 0}, true));
    wal << frame(node_payload(9003, 1, 2, "invalid-far", 0.2f, {4, 4, 0}));
    wal << frame(edge_payload(9003, 0, 0, 1));
    wal << frame("COMMIT\t9003");
  }
  {
    GrapheneDB db; require(db.open(invalid, opt), "reopen invalid lattice");
    std::string report;
    auto st = db.validate(&report);
    assert(!st);
    assert(report.find("invalid same-layer lattice bond") != std::string::npos);
    require(db.close(), "close invalid lattice");
  }

  // Leftover checkpoint/manifest temp files from interrupted writes are ignored.
  fs::path leftovers = base / "leftover_tmp";
  {
    GrapheneDB db; require(db.open(leftovers, opt), "open leftovers");
    BatchInput batch;
    NodeInput a;
    a.content = "leftover-root";
    a.vector = {0.1f, 0.11f, 0.12f, 0.13f};
    a.signature = signature_for(6, 7);
    a.incident = 607;
    a.root = true;
    a.lattice = LatticeCoord{0, 0, 0};
    NodeInput b = a;
    b.content = "leftover-neighbor";
    b.root = false;
    b.vector = {0.2f, 0.21f, 0.22f, 0.23f};
    b.lattice = LatticeCoord{1, 0, 0};
    batch.nodes = {a, b};
    EdgeInput e{0, 1, EdgeOrigin::Observed, EdgeRole::Causal, 0.9};
    e.bond_type = BondType::Sigma;
    e.layer_coupling = LayerCoupling::SameLayer;
    batch.edges = {e};
    require(db.put_batch(batch), "put leftover batch");
    require(db.compact(), "compact leftovers");
    require(db.close(), "close leftovers");
    std::ofstream(leftovers / "graphene.data.tmp") << "not-a-valid-data-file\n";
    std::ofstream(leftovers / "MANIFEST.tmp") << "not-a-valid-manifest\n";
  }
  {
    GrapheneDB db; require(db.open(leftovers, opt), "reopen leftovers");
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1);
    require(db.validate(), "validate leftovers");
    require(db.close(), "close leftovers reopened");
  }

  // Manifest corruption fails predictably before replay/open proceeds.
  fs::path bad_checksum = base / "manifest_bad_checksum";
  {
    fs::create_directories(bad_checksum);
    write_manifest_file(bad_checksum, manifest_body(), false);
    GrapheneDB db;
    auto st = db.open(bad_checksum, opt);
    assert(!st);
    assert(st.code == ErrorCode::DataCorrupt);
    assert(st.message.find("checksum") != std::string::npos);
  }

  fs::path bad_header = base / "manifest_bad_header";
  {
    fs::create_directories(bad_header);
    auto body = manifest_body();
    body.replace(0, std::string("graphenedb_manifest_v1").size(), "graphenedb_manifest_v99");
    write_manifest_file(bad_header, body, true);
    GrapheneDB db;
    auto st = db.open(bad_header, opt);
    assert(!st);
    assert(st.code == ErrorCode::DataCorrupt);
    assert(st.message.find("header") != std::string::npos);
  }

  fs::path future_storage = base / "manifest_future_storage";
  {
    fs::create_directories(future_storage);
    write_manifest_file(future_storage, manifest_body(4, 999), true);
    GrapheneDB db;
    auto st = db.open(future_storage, opt);
    assert(!st);
    assert(st.code == ErrorCode::UnsupportedMode);
  }

  std::cout << "graphenedb_rc5_crash_matrix_passed=true\n";
  return 0;
}
