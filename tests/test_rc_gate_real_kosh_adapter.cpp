#include "graphene/kosh_adapter.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> vec(uint32_t d, float base) { std::vector<float> v(d); for(uint32_t i=0;i<d;i++) v[i]=base+0.001f*i; return v; }
static std::string vec_csv(const std::vector<float>& v){ std::ostringstream os; for(size_t i=0;i<v.size();++i){ if(i) os<<','; os<<std::setprecision(9)<<v[i]; } return os.str(); }
static std::string hex(const std::string& s){ static const char* h="0123456789abcdef"; std::string o; for(unsigned char c:s){ o.push_back(h[c>>4]); o.push_back(h[c&15]); } return o; }
static void require(Status st, const char* what){ if(!st){ std::cerr<<"FAIL "<<what<<": "<<st.message<<"\n"; std::abort(); } }

int main(){
  const uint32_t D=16;
  fs::path dir=fs::temp_directory_path()/"graphenedb_rc_real_kosh_adapter";
  fs::remove_all(dir);
  DBOptions opt; opt.dimension=D; opt.fsync_on_commit=false;
  GrapheneDB db; require(db.open(dir,opt),"open");
  KoshAdapter adapter(db);
  uint64_t sig=signature_for(5,9);
  fs::path import=dir/"llm_kosh_export.tsv";
  {
    std::ofstream out(import);
    out << "ADR-101\tarchitecture_decision\t"<<sig<<"\t101\t1\t0\t"<<vec_csv(vec(D,0.55f))<<"\t"<<hex("ADR: use cache bypass because checkout failures came from stale edge cache")<<"\t"<<hex("project")<<"="<<hex("checkout")<<";"<<hex("source")<<"="<<hex("llm-kosh")<<"\n";
    out << "MOD-101\tmodule_context\t"<<sig<<"\t101\t0\t0\t"<<vec_csv(vec(D,0.56f))<<"\t"<<hex("payment module has dependency on cache bypass during migration")<<"\t"<<hex("project")<<"="<<hex("checkout")<<";"<<hex("source")<<"="<<hex("llm-kosh")<<"\n";
    out << "INC-101\tincident_symptom\t"<<sig<<"\t101\t0\t1\t"<<vec_csv(vec(D,0.56f))<<"\t"<<hex("checkout timeout after migration with stale cache symptoms")<<"\t"<<hex("project")<<"="<<hex("checkout")<<";"<<hex("source")<<"="<<hex("llm-kosh")<<"\n";
  }
  std::vector<uint32_t> ids; require(adapter.ingest_tsv(import,&ids),"ingest tsv");
  assert(ids.size()==3);
  require(adapter.link(ids[0], ids[1], EdgeRole::Causal, EdgeOrigin::Observed, 0.95),"link 1");
  require(adapter.link(ids[1], ids[2], EdgeRole::Causal, EdgeOrigin::Discovered, 0.92),"link 2");
  auto by_project = db.metadata_search("project", "checkout"); assert(by_project.size()==3);
  auto bundle = adapter.retrieve_causal_bundle(vec(D,0.56f), sig, QueryMode::Empirical);
  assert(!bundle.abstain); assert(bundle.target_node == ids[0]); assert(!bundle.paths.empty());
  DialecticOptions dialectic_options;
  dialectic_options.mode = QueryMode::Empirical;
  dialectic_options.semantic_candidates = 3;
  dialectic_options.max_opposition_rounds = 1;
  auto dialectic = adapter.retrieve_dialectic(vec(D,0.56f), sig, dialectic_options);
  assert(dialectic.synthesis.has_answer);
  assert(dialectic.synthesis.primary_node == ids[0]);
  assert(!dialectic.durable_writes);
  auto root = db.get_node(bundle.target_node); assert(root.has_value());
  assert(root->metadata.at("kosh_external_id") == "ADR-101");
  assert(root->metadata.at("adapter") == "graphene-kosh-adapter-v1");
  require(db.close(),"close");
  GrapheneDB reopened; require(reopened.open(dir,opt),"reopen");
  KoshAdapter reopened_adapter(reopened);
  auto rb = reopened_adapter.retrieve_causal_bundle(vec(D,0.56f), sig, QueryMode::Empirical);
  assert(!rb.abstain && rb.target_node == ids[0]);
  require(reopened.close(),"close reopened");
  std::cout << "rc_real_kosh_adapter_gate_passed=true\n";
  return 0;
}
