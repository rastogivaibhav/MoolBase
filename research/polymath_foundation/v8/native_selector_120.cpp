// F8: frozen F7 native selector algorithm, repackaged for 120 semantic queries.
// This DOES NOT solve unseen science or learn mechanisms; it matches schemas.
#include "graphene/db.hpp"
#include "graphene/generic_relation.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using namespace graphene;
using Row=std::map<std::string,std::string>;

static void ensure(bool condition,const std::string& why) {
  if(!condition) throw std::runtime_error(why);
}
static std::vector<std::string> split(const std::string& text,char delimiter) {
  std::istringstream in(text);
  std::string token;
  std::vector<std::string> v;
  while(std::getline(in,token,delimiter)) v.push_back(token);
  return v;
}
static const std::vector<std::string> MANIFEST={
  "mechanism_id","version","domain","card_id","formula_id","source_family",
  "source_status","license_status","description","input_spec","output_spec","card_sha256"};
static const std::vector<std::string> QUESTIONS={
  "opaque_id","description","target","target_unit","inputs"};
static const std::string OUT=
  "opaque_id\tselection_status\tmechanism_id\tdomain\treason\tsource_family\tnative_node_id";
static std::string field(const Row& r,const std::string& key) {
  const auto it=r.find(key);
  ensure(it!=r.end()&&!it->second.empty(),"missing TSV field: "+key);
  return it->second;
}
static std::vector<Row> tsv(const std::string& path,
                             const std::vector<std::string>& expected) {
  std::ifstream file(path);
  ensure(bool(file),"TSV unavailable");
  std::string s;
  ensure(bool(std::getline(file,s)),"empty TSV");
  const auto cols=split(s,'\t');
  ensure(cols==expected,"malformed TSV header");
  std::vector<Row> records;
  while(std::getline(file,s)) {
    auto parts=split(s,'\t');
    ensure(parts.size()==expected.size(),"incorrect TSV value count");
    Row r;
    for(size_t i=0;i<parts.size();++i) {
      ensure(!parts[i].empty(),"blank TSV field");
      r[cols[i]]=parts[i];
    }
    records.push_back(std::move(r));
  }
  return records;
}
static std::map<std::string,std::string> canonical_ids{
  {"networks","toy.network.finite_queue.v1"},
  {"biology","toy.bio.saturated_regulator.v1"},
  {"supply_chain","toy.supply.reorder.v1"}};
static std::map<std::string,std::string> expected_input{
  {"networks","queue:integer:packets|arrivals:integer:packets|service:integer:packets|buffer:integer:packets"},
  {"biology","observed:float:state_units|setpoint:float:state_units|gain:float:dimensionless|limit:float:state_units|disturbance:float:state_units"},
  {"supply_chain","daily_demand:integer:items_per_day|lead_days:integer:days|safety:integer:items|on_hand:integer:items|on_order:integer:items|backorders:integer:items|target:integer:items"}};
static std::map<std::string,std::string> expected_output{
  {"networks","next_queue:integer:packets|drops:integer:packets"},
  {"biology","control:float:state_units|next_state:float:state_units"},
  {"supply_chain","reorder_point:integer:items|order_qty:integer:items"}};
static std::map<std::string,std::string> expected_formula{
  {"networks","FINITE_QUEUE"},
  {"biology","SATURATED_REGULATOR"},
  {"supply_chain","REORDER_POLICY"}};

static void validate_manifest(const Row& r,const std::string& commonhash) {
  const auto d=field(r,"domain");
  ensure(canonical_ids.count(d),"unknown domain");
  ensure(field(r,"mechanism_id")==canonical_ids.at(d) &&
         field(r,"formula_id")==expected_formula.at(d) &&
         field(r,"version")=="1", "nonallowlisted scientific procedure");
  ensure(field(r,"input_spec")==expected_input.at(d) &&
         field(r,"output_spec")==expected_output.at(d),
         "unreviewed input/output unit scheme");
  ensure(field(r,"source_status")=="unverified" &&
         field(r,"license_status")=="project_authored" &&
         field(r,"source_family")=="original-project-authored-"+d,
         "unsupported Verified/authenticated claims");
  ensure(field(r,"card_sha256")==commonhash && commonhash.size()==64,
         "source identity inconsistent");
}
using Spec=std::map<std::string,std::pair<std::string,std::string>>;
static Spec typed_spec(const std::string& text) {
  Spec out;
  for(const auto& entry:split(text,'|')) {
    const auto p=split(entry,':');
    ensure(p.size()==3,"invalid native typed declaration");
    ensure(out.emplace(p[0],std::make_pair(p[1],p[2])).second,
           "duplicate schema field");
  }
  return out;
}
static bool decode_input(const std::string& text,Spec* out) {
  for(const auto& part:split(text,';')) {
    const size_t eq=part.find('='), at=part.find('@'), hash=part.find('#');
    if(eq==std::string::npos||at==std::string::npos||hash==std::string::npos||
       eq>=at ||at>=hash) return false;
    const std::string name=part.substr(0,eq), value=part.substr(eq+1,at-eq-1);
    if(name.empty()||value.empty()) return false;
    if(!out->emplace(name,std::make_pair(part.substr(hash+1),
                                      part.substr(at+1,hash-at-1))).second) return false;
  }
  return !out->empty();
}
struct Selection {
  bool matched{false};
  Row record;
  std::string reason;
  uint32_t node_id{0};
};
static Selection pick(const Row& task,const std::vector<std::pair<Row,uint32_t>>& registered) {
  Spec provided;
  if(!decode_input(field(task,"inputs"),&provided))
    return {false,{},"MALFORMED_INPUT",0};
  std::vector<std::pair<Row,uint32_t>> candidates;
  for(const auto& item:registered) {
    const auto& procedure=item.first;
    const auto inputs=typed_spec(field(procedure,"input_spec"));
    const auto outputs=typed_spec(field(procedure,"output_spec"));
    const auto output=outputs.find(field(task,"target"));
    if(output==outputs.end()||output->second.second!=field(task,"target_unit"))
      continue;
    if(provided.size()<inputs.size()-1||provided.size()>inputs.size())
      continue;
    bool match=true;
    for(const auto& [name,kindunit]:provided) {
      const auto it=inputs.find(name);
      if(it==inputs.end()||it->second!=kindunit) {
        match=false;
        break;
      }
    }
    if(match) candidates.push_back(item);
  }
  if(candidates.empty()) return {false,{},"NO_APPLICABLE_SCHEMA",0};
  if(candidates.size()>1) return {false,{},"AMBIGUOUS_SCHEMA",0};
  return {true,candidates[0].first,"SCHEMA_UNIQUE",
          candidates[0].second};
}
static void run(const char* definitions,const char* queries,const char* result) {
  auto declared=tsv(definitions,MANIFEST);
  ensure(declared.size()==3,"exactly three distinct registered procedures");
  const auto commonhash=field(declared.front(),"card_sha256");
  std::set<std::string> domains;
  for(const auto& def:declared) {
    validate_manifest(def,commonhash);
    ensure(domains.insert(field(def,"domain")).second,
           "duplicate domain/mechanism identity");
  }
  fs::path root=fs::temp_directory_path()/"moolbase-f8-select-native";
  fs::remove_all(root);
  DBOptions options;
  options.dimension=16;
  options.fsync_on_commit=false;
  GrapheneDB db;
  ensure(bool(db.open(root,options)),"GrapheneDB cannot open");
  for(const auto& def:declared) {
    NodeInput node;
    node.content=field(def,"description");
    node.vector=deterministic_text_vector(node.content+" "+field(def,"mechanism_id"),16);
    node.signature=deterministic_text_signature(field(def,"mechanism_id"));
    node.root=true;
    node.metadata=def;
    node.metadata["epistemic_status"]="unverified";
    node.metadata["executor_policy"]="strict_typed_allowlist";
    const auto status=db.put_node(node);
    ensure(bool(status),"native MoolBase put_node failed: "+status.message);
  }
  ensure(db.node_count()==3 && db.edge_count()==0,
         "unexpected nodes/Observed edges");
  ensure(bool(db.close()),"GrapheneDB could not close");
  ensure(bool(db.open(root,options)),"GrapheneDB failed restart");
  const auto ids=db.metadata_search("source_status","unverified");
  ensure(ids.size()==3,"not exactly three unverified source records");
  std::vector<std::pair<Row,uint32_t>> candidates;
  std::set<std::string> after_restart;
  for(const auto id:ids) {
    const auto n=db.get_node(id);
    ensure(bool(n),"native candidate not retrieved after restart");
    const Row& meta=n->metadata;
    validate_manifest(meta,commonhash);
    ensure(field(meta,"epistemic_status")=="unverified" &&
           field(meta,"executor_policy")=="strict_typed_allowlist",
           "source verification upgraded during retrieval");
    ensure(after_restart.insert(field(meta,"mechanism_id")).second,
           "ambiguous native candidate identity");
    candidates.emplace_back(meta,id);
  }
  const auto tasks=tsv(queries,QUESTIONS);
  ensure(tasks.size()==120,"F8 fixed semantic population mismatch");
  std::ofstream out(result);
  ensure(bool(out),"cannot create selection receipt");
  out<<OUT<<'\n';
  std::set<std::string> taskids;
  int chosen=0,declined=0;
  for(const auto& task:tasks) {
    const auto id=field(task,"opaque_id");
    ensure(taskids.insert(id).second,"duplicate opaque task identifier");
    Selection s=pick(task,candidates);
    if(s.matched) ++chosen; else ++declined;
    out<<id<<'\t'<<(s.matched?"selected":"abstain")<<'\t'
       <<(s.matched?field(s.record,"mechanism_id"):"-")<<'\t'
       <<(s.matched?field(s.record,"domain"):"-")<<'\t'
       <<s.reason<<'\t'
       <<(s.matched?field(s.record,"source_family"):"-")<<'\t'
       <<(s.matched?std::to_string(s.node_id):"-")<<'\n';
  }
  ensure(bool(out),"native receipt flush failed");
  ensure(bool(db.close()),"native GrapheneDB close error");
  fs::remove_all(root);
  std::cout<<"f8_native_graph_routing=PASS trials="<<tasks.size()
           <<" selected="<<chosen<<" abstained="<<declined
           <<" verified_facts=0"<<std::endl;
}
int main(int argc,char**argv) {
  try {
    ensure(argc==4,"usage: f7_native_selector manifest.tsv query.tsv route.tsv");
    run(argv[1],argv[2],argv[3]);
  }catch(const std::exception& ex) {
    std::cerr<<"f7_native_route_error="<<ex.what()<<std::endl;
    return 1;
  }
}
