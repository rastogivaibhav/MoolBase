#include "graphene/db.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

struct KoshMemory {
  std::string id;
  std::string content;
  std::string type;
  std::vector<float> vector;
  uint64_t signature{0};
  uint32_t incident{0};
  bool root{false};
  bool symptom{false};
};

class FakeKoshAdapter {
public:
  explicit FakeKoshAdapter(GrapheneDB& db) : db_(db) {}
  Status ingest_memory(const KoshMemory& m, uint32_t* out_id=nullptr) {
    NodeInput n;
    n.content = m.content;
    n.vector = m.vector;
    n.signature = m.signature;
    n.incident = m.incident;
    n.root = m.root;
    n.symptom = m.symptom;
    n.metadata["kosh_id"] = m.id;
    n.metadata["kosh_type"] = m.type;
    n.metadata["source"] = "fake-llm-kosh";
    return db_.put_node(n, out_id);
  }
  Status link(uint32_t from, uint32_t to, EdgeRole role=EdgeRole::Causal, EdgeOrigin origin=EdgeOrigin::Observed) {
    EdgeInput e{from, to, origin, role, 0.93};
    e.metadata["adapter"] = "fake-kosh";
    return db_.put_edge(e);
  }
  MemoryBundle retrieve_causal_bundle(const std::vector<float>& query, uint64_t sig) {
    return db_.causal_search(query, sig, QueryMode::Empirical);
  }
private:
  GrapheneDB& db_;
};

static std::vector<float> vec(uint32_t d, float base) { std::vector<float> v(d); for (uint32_t i=0;i<d;i++) v[i]=base+0.001f*i; return v; }
static void require(Status st, const char* what) { if (!st) { std::cerr << "FAIL " << what << ": " << st.message << "\n"; std::abort(); } }

int main() {
  const uint32_t D = 24;
  fs::path dir = fs::temp_directory_path() / "graphenedb_rc_kosh_adapter";
  fs::remove_all(dir);
  DBOptions opt; opt.dimension = D; opt.fsync_on_commit = false;
  GrapheneDB db; require(db.open(dir, opt), "open");
  FakeKoshAdapter kosh(db);

  uint64_t sig = signature_for(7, 3);
  uint32_t adr, module, symptom;
  require(kosh.ingest_memory({"ADR-004", "Use ingress rewrite because checkout latency came from legacy route fanout", "architecture_decision", vec(D, 0.7f), sig, 404, true, false}, &adr), "ingest ADR");
  require(kosh.ingest_memory({"MOD-payment", "payment module depends on ingress rewrite and gateway timeout handling", "module_context", vec(D, 0.72f), sig, 404, false, false}, &module), "ingest module");
  require(kosh.ingest_memory({"INC-1421", "checkout timeout after migration through payment gateway", "incident_symptom", vec(D, 0.72f), sig, 404, false, true}, &symptom), "ingest symptom");
  require(kosh.link(adr, module, EdgeRole::Causal, EdgeOrigin::Observed), "link adr module");
  require(kosh.link(module, symptom, EdgeRole::Causal, EdgeOrigin::Discovered), "link module symptom");

  auto bundle = kosh.retrieve_causal_bundle(vec(D, 0.72f), sig);
  assert(!bundle.abstain);
  assert(bundle.target_node == adr);
  assert(!bundle.paths.empty());
  assert(!bundle.why_retrieved.empty());
  bool has_sig_reason = false;
  for (const auto& why : bundle.why_retrieved) if (why.find("signature-plane") != std::string::npos) has_sig_reason = true;
  assert(has_sig_reason);
  auto root = db.get_node(bundle.target_node);
  assert(root.has_value());
  assert(root->metadata.at("source") == "fake-llm-kosh");
  assert(root->metadata.at("kosh_type") == "architecture_decision");

  // Adapter can persist and reload the Kosh metadata/retrieval graph.
  require(db.close(), "close");
  GrapheneDB reopened; require(reopened.open(dir, opt), "reopen");
  auto rn = reopened.get_node(adr);
  assert(rn.has_value());
  assert(rn->metadata.at("kosh_id") == "ADR-004");
  auto rb = reopened.causal_search(vec(D, 0.72f), sig, QueryMode::Empirical);
  assert(!rb.abstain && rb.target_node == adr);
  require(reopened.close(), "close reopened");

  std::cout << "rc_kosh_adapter_gate_passed=true\n";
  return 0;
}
