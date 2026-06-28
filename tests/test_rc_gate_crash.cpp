#include "graphene/db.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

static uint64_t fnv1a_test(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  return h;
}

static std::string hex_encode_test(const std::string& input) {
  static const char* hex = "0123456789abcdef";
  std::string out;
  for (unsigned char c : input) { out.push_back(hex[c >> 4]); out.push_back(hex[c & 0x0f]); }
  return out;
}

static std::string frame(const std::string& payload) {
  return std::to_string(payload.size()) + "|" + std::to_string(fnv1a_test(payload)) + "|" + payload + "\n";
}

static std::string vec_csv(uint32_t d, float base) {
  std::ostringstream os;
  for (uint32_t i = 0; i < d; ++i) { if (i) os << ','; os << (base + 0.001f * i); }
  return os.str();
}

static std::string put_node_payload(uint64_t tx, uint32_t id, uint64_t created, uint32_t d, const std::string& content, uint64_t sig, uint32_t incident, bool root=false, bool symptom=false) {
  std::ostringstream os;
  os << "PUT_NODE\t" << tx << "\t" << id << "\t" << created << "\t" << kInfVersion << "\t" << sig << "\t" << incident
     << "\t" << (root ? 1 : 0) << "\t" << (symptom ? 1 : 0) << "\t0\t" << hex_encode_test(content) << "\t" << vec_csv(d, 0.31f) << "\t";
  return os.str();
}

static std::vector<float> make_vec(uint32_t d, float base) {
  std::vector<float> out(d);
  for (uint32_t i = 0; i < d; ++i) out[i] = base + 0.001f * i;
  return out;
}

static NodeInput make_node(const std::string& content, uint32_t d, uint64_t sig, uint32_t incident, bool root=false, bool symptom=false) {
  NodeInput n;
  n.content = content;
  n.vector = make_vec(d, 0.11f + incident * 0.001f);
  n.signature = sig;
  n.incident = incident;
  n.root = root;
  n.symptom = symptom;
  n.metadata["test"] = "crash";
  return n;
}

static void require(Status st, const char* what) {
  if (!st) { std::cerr << "FAIL " << what << ": " << st.message << "\n"; std::abort(); }
}

int main() {
  const uint32_t D = 8;
  DBOptions opt; opt.dimension = D; opt.fsync_on_commit = false;

  // Crash during a middle-ID compaction must not resurrect deleted placeholder nodes.
  fs::path gap = fs::temp_directory_path() / "graphenedb_rc_crash_gap";
  fs::remove_all(gap);
  {
    GrapheneDB db; require(db.open(gap, opt), "open gap");
    uint32_t a,b,c;
    require(db.put_node(make_node("n0", D, signature_for(1,1), 1), &a), "put a");
    require(db.put_node(make_node("n1 delete me", D, signature_for(1,1), 1), &b), "put b");
    require(db.put_node(make_node("n2 survivor", D, signature_for(1,1), 1), &c), "put c");
    assert(a == 0 && b == 1 && c == 2);
    require(db.delete_node(1), "delete middle");
    require(db.compact(), "compact gap");
    require(db.close(), "close gap");
  }
  {
    GrapheneDB db; require(db.open(gap, opt), "reopen gap");
    assert(db.node_count() == 2);
    assert(!db.get_node(1).has_value());
    auto n2 = db.get_node(2);
    assert(n2.has_value() && n2->content == "n2 survivor");
    require(db.close(), "close reopen gap");
  }

  // Uncommitted transaction after BEGIN/PUT is ignored after crash/reopen.
  fs::path uncommitted = fs::temp_directory_path() / "graphenedb_rc_crash_uncommitted";
  fs::remove_all(uncommitted);
  {
    GrapheneDB db; require(db.open(uncommitted, opt), "open uncommitted");
    require(db.put_node(make_node("baseline", D, signature_for(2,2), 2), nullptr), "put baseline");
    require(db.close(), "close baseline");
  }
  {
    std::ofstream wal(uncommitted / "graphene.wal", std::ios::app);
    wal << frame("BEGIN\t700");
    wal << frame(put_node_payload(700, 77, 77, D, "should not replay", signature_for(2,2), 2));
  }
  {
    GrapheneDB db; require(db.open(uncommitted, opt), "reopen uncommitted");
    assert(db.node_count() == 1);
    assert(!db.get_node(77).has_value());
    require(db.close(), "close uncommitted reopen");
  }

  // Committed transaction written directly to WAL is replayed exactly.
  fs::path committed = fs::temp_directory_path() / "graphenedb_rc_crash_committed";
  fs::remove_all(committed);
  {
    GrapheneDB db; require(db.open(committed, opt), "open committed"); require(db.close(), "close empty committed");
  }
  {
    std::ofstream wal(committed / "graphene.wal", std::ios::app);
    wal << frame("BEGIN\t701");
    wal << frame(put_node_payload(701, 42, 42, D, "committed replay node", signature_for(3,3), 3, true, false));
    wal << frame("COMMIT\t701");
  }
  {
    GrapheneDB db; require(db.open(committed, opt), "reopen committed");
    auto n = db.get_node(42);
    assert(n.has_value());
    assert(n->content == "committed replay node");
    assert(n->vector.size() == D);
    assert(n->root);
    assert(db.node_count() == 1);
    require(db.close(), "close committed");
  }

  // Torn tail after a valid committed transaction is ignored safely.
  {
    std::ofstream wal(committed / "graphene.wal", std::ios::app);
    wal << "999|123|TORN_TAIL";
  }
  {
    GrapheneDB db; require(db.open(committed, opt), "reopen torn tail");
    assert(db.node_count() == 1);
    auto n = db.get_node(42);
    assert(n.has_value() && n->content == "committed replay node");
    require(db.close(), "close torn tail");
  }

  // A valid frame with an unknown op must fail controlled, not silently produce state.
  fs::path badop = fs::temp_directory_path() / "graphenedb_rc_crash_badop";
  fs::remove_all(badop);
  {
    GrapheneDB db; require(db.open(badop, opt), "open badop"); require(db.close(), "close badop empty");
    std::ofstream wal(badop / "graphene.wal", std::ios::app);
    wal << frame("BEGIN\t702") << frame("UNKNOWN_OP\t702\tbad") << frame("COMMIT\t702");
  }
  {
    GrapheneDB db;
    auto st = db.open(badop, opt);
    assert(!st);
    assert(st.code == ErrorCode::DataCorrupt || st.code == ErrorCode::WalCorrupt || st.code == ErrorCode::TransactionIncomplete);
  }

  std::cout << "rc_crash_gate_passed=true\n";
  return 0;
}
