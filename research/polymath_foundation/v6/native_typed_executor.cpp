// Foundation v6: real persistent GrapheneDB + bounded, typed mechanism executor.
// No free-form expression evaluation, no LLM generated code, no Verified claims.
#include "graphene/db.hpp"
#include "graphene/generic_relation.hpp"
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs=std::filesystem;
using namespace graphene;
using Fields=std::map<std::string,std::string>;

static void require(bool test,const std::string& reason) {
  if(!test) throw std::runtime_error(reason);
}
static std::vector<std::string> split(const std::string& s,char delimiter) {
  std::vector<std::string> out;
  std::string token;
  std::istringstream in(s);
  while(std::getline(in,token,delimiter)) out.push_back(token);
  return out;
}
static std::string get(const Fields& fields,const std::string& key) {
  auto it=fields.find(key);
  require(it!=fields.end()&&!it->second.empty(),"missing metadata: "+key);
  return it->second;
}
static std::vector<Fields> read_tsv(const std::string& path,
                                    const std::vector<std::string>& header) {
  std::ifstream file(path);
  require(bool(file),"cannot read TSV: "+path);
  std::string line;
  require(bool(std::getline(file,line)),"empty TSV");
  const auto cols=split(line,'\t');
  require(cols==header,"TSV schema header changed");
  std::vector<Fields> results;
  while(std::getline(file,line)) {
    require(!line.empty(),"empty TSV record");
    auto vals=split(line,'\t');
    require(vals.size()==header.size(),"unexpected TSV field count");
    Fields item;
    for(size_t i=0;i<cols.size();++i) {
      require(!vals[i].empty(),"empty required TSV value "+cols[i]);
      item[cols[i]]=vals[i];
    }
    results.push_back(std::move(item));
  }
  return results;
}
static const std::vector<std::string> MANIFEST={
    "mechanism_id","version","domain","card_id","formula_id",
    "source_family","source_status","license_status","description",
    "input_spec","output_spec","card_sha256"};
static const std::vector<std::string> INPUT={
    "task_id","domain","mechanism_id","target","target_unit","inputs"};
static const std::string OUTPUT_HEADER=
    "task_id\tdomain\tmechanism_id\ttarget\ttarget_unit\tstatus\tvalue\treason\t"
    "source_family\tsource_status\tpolicy_version\tnative_node_id\tscientific_truth_claim";

struct Type {
  std::string kind,unit;
};
struct Procedure {
  std::string mechanism_id,version,domain,card_id,formula_id;
  std::string family,description,card_sha;
  std::map<std::string,Type> input,output;
};
static std::map<std::string,Type> parse_spec(const std::string& spec) {
  std::map<std::string,Type> out;
  for(const std::string& descriptor:split(spec,'|')) {
    const auto f=split(descriptor,':');
    require(f.size()==3,"malformed type specification");
    require(f[1]=="integer"||f[1]=="float","unsupported numerical type");
    require(!f[0].empty()&&!f[2].empty(),"blank typed field");
    require(out.emplace(f[0],Type{f[1],f[2]}).second,
            "duplicate typed field "+f[0]);
  }
  return out;
}
static std::string spec(const std::vector<std::string>& fields) {
  std::string out;
  for(const auto& f:fields) {
    if(!out.empty()) out+="|";
    out+=f;
  }
  return out;
}
static void validate(const Fields& row) {
  const auto& d=get(row,"domain");
  const std::map<std::string,std::vector<std::string>> known{
    {"networks",{
      "queue:integer:packets","arrivals:integer:packets",
      "service:integer:packets","buffer:integer:packets"}},
    {"biology",{
      "observed:float:state_units","setpoint:float:state_units",
      "gain:float:dimensionless","limit:float:state_units",
      "disturbance:float:state_units"}},
    {"supply_chain",{
      "daily_demand:integer:items_per_day","lead_days:integer:days",
      "safety:integer:items","on_hand:integer:items",
      "on_order:integer:items","backorders:integer:items",
      "target:integer:items"}}
  };
  const std::map<std::string,std::vector<std::string>> expected_outputs{
    {"networks",{"next_queue:integer:packets","drops:integer:packets"}},
    {"biology",{"control:float:state_units","next_state:float:state_units"}},
    {"supply_chain",{"reorder_point:integer:items","order_qty:integer:items"}}
  };
  const std::map<std::string,std::pair<std::string,std::string>> allowed{
    {"networks",{"toy.network.finite_queue.v1","FINITE_QUEUE"}},
    {"biology",{"toy.bio.saturated_regulator.v1","SATURATED_REGULATOR"}},
    {"supply_chain",{"toy.supply.reorder.v1","REORDER_POLICY"}}
  };
  require(allowed.count(d)!=0,"domain outside reviewed allowlist");
  require(get(row,"mechanism_id")==allowed.at(d).first &&
          get(row,"formula_id")==allowed.at(d).second,
          "unapproved mechanism/procedure identity");
  require(get(row,"input_spec")==spec(known.at(d)) &&
          get(row,"output_spec")==spec(expected_outputs.at(d)),
          "input/output type/unit contract tampered");
  require(get(row,"version")=="1" &&
          get(row,"source_status")=="unverified" &&
          get(row,"license_status")=="project_authored" &&
          get(row,"source_family")=="original-project-authored-"+d,
          "procedure provenance or version violated");
  const auto h=get(row,"card_sha256");
  require(h.size()==64 &&
          std::all_of(h.begin(),h.end(),[](char c){
            return (c>='0'&&c<='9')||(c>='a'&&c<='f');
          }),"invalid canonical source hash");
}
static bool parse_value(const std::string& text,const Type& field,
                        int64_t* integer,double* floating,std::string* why) {
  try {
    size_t consumed=0;
    if(field.kind=="integer") {
      const auto v=std::stoll(text,&consumed,10);
      if(consumed!=text.size()) { *why="invalid_integer";return false; }
      *integer=static_cast<int64_t>(v);
      *floating=static_cast<double>(v);
    } else {
      const auto v=std::stod(text,&consumed);
      if(consumed!=text.size()||!std::isfinite(v)) {
        *why="nonfinite_or_invalid_float";return false;
      }
      *floating=v;
    }
    return true;
  }catch(const std::exception&) {
    *why="invalid_or_out_of_range_numeric";
    return false;
  }
}
static bool add_i(int64_t a,int64_t b,int64_t* out) {
  if((b>0&&a>std::numeric_limits<int64_t>::max()-b)||
     (b<0&&a<std::numeric_limits<int64_t>::min()-b)) return false;
  *out=a+b;
  return true;
}
static bool sub_i(int64_t a,int64_t b,int64_t* out) {
  if(b==std::numeric_limits<int64_t>::min()) {
    if(a>=0) return false;
    return add_i(a,std::numeric_limits<int64_t>::max(),out) &&
           add_i(*out,1,out);
  }
  return add_i(a,-b,out);
}
static bool mul_nonneg(int64_t a,int64_t b,int64_t* out) {
  if(a<0||b<0) return false;
  if(a!=0 && b>std::numeric_limits<int64_t>::max()/a) return false;
  *out=a*b;
  return true;
}
struct Number {
  int64_t i{0};
  double f{0};
};
struct Outcome {
  bool ok{false};
  double value{0};
  std::string reason;
};
static Outcome apply(const Procedure& p,const std::string& target,
                     const std::string& provided_unit,
                     const std::string& inputs) {
  auto endpoint=p.output.find(target);
  if(endpoint==p.output.end()) return {false,0,"unregistered_output"};
  if(endpoint->second.unit!=provided_unit)
    return {false,0,"target_unit_mismatch"};
  std::map<std::string,Number> args;
  for(const auto& entry:split(inputs,';')) {
    const auto eql=entry.find('=');
    const auto at=entry.find('@');
    const auto hash=entry.find('#');
    if(eql==std::string::npos||at==std::string::npos||
       hash==std::string::npos||eql>=at||at>=hash)
      return {false,0,"malformed_typed_input"};
    const auto name=entry.substr(0,eql);
    const auto value=entry.substr(eql+1,at-eql-1);
    const auto unit=entry.substr(at+1,hash-at-1);
    const auto kind=entry.substr(hash+1);
    auto declared=p.input.find(name);
    if(declared==p.input.end()) return {false,0,"unknown_input_field"};
    if(!args.emplace(name,Number{}).second)
      return {false,0,"duplicate_input_field"};
    if(kind!=declared->second.kind) return {false,0,"input_type_mismatch"};
    if(unit!=declared->second.unit) return {false,0,"input_unit_mismatch"};
    std::string why;
    if(!parse_value(value,declared->second,
                    &args[name].i,&args[name].f,&why)) return {false,0,why};
  }
  if(args.size()!=p.input.size()) return {false,0,"missing_required_input"};
  auto I=[&args](const char* name){return args.at(name).i;};
  auto F=[&args](const char* name){return args.at(name).f;};
  if(p.formula_id=="FINITE_QUEUE") {
    const auto q=I("queue"),a=I("arrivals"),s=I("service"),b=I("buffer");
    if(q<0||a<0||s<0||b<0||q>b)
      return {false,0,"assumption_violation_queue"};
    int64_t x=0,unserved=0,result=0;
    if(!add_i(q,a,&x)||!sub_i(x,s,&unserved))
      return {false,0,"integer_overflow"};
    unserved=std::max<int64_t>(0,unserved);
    if(target=="next_queue") result=std::min(b,unserved);
    else {
      if(unserved>b && !sub_i(unserved,b,&result))
        return {false,0,"integer_overflow"};
    }
    return {true,static_cast<double>(result),""};
  }
  if(p.formula_id=="SATURATED_REGULATOR") {
    const double observed=F("observed"),setpoint=F("setpoint"),
                 gain=F("gain"),limit=F("limit"),
                 disturbance=F("disturbance");
    if(gain<0 || limit<=0) return {false,0,"assumption_violation_regulator"};
    const auto raw=gain*(setpoint-observed);
    if(!std::isfinite(raw)) return {false,0,"float_overflow"};
    const double control=std::max(-limit,std::min(limit,raw));
    const double next=observed+disturbance+control;
    if(!std::isfinite(next)) return {false,0,"float_overflow"};
    return {true,target=="control"?control:next,""};
  }
  if(p.formula_id=="REORDER_POLICY") {
    for(const auto& [_,v]:args) if(v.i<0)
      return {false,0,"assumption_violation_inventory"};
    int64_t position=0,tmp=0,point=0,qty=0;
    if(!add_i(I("on_hand"),I("on_order"),&tmp) ||
       !sub_i(tmp,I("backorders"),&position) ||
       !mul_nonneg(I("daily_demand"),I("lead_days"),&tmp) ||
       !add_i(tmp,I("safety"),&point))
      return {false,0,"integer_overflow"};
    if(position<=point) {
      if(!sub_i(I("target"),position,&tmp))
        return {false,0,"integer_overflow"};
      qty=std::max<int64_t>(0,tmp);
    }
    return {true,static_cast<double>(target=="reorder_point"?point:qty),""};
  }
  return {false,0,"unapproved_formula"};
}
static void run(const std::string& manifest,const std::string& tasks,
                const std::string& output) {
  const auto defs=read_tsv(manifest,MANIFEST);
  require(defs.size()==3,"exactly three reviewed mechanism definitions required");
  std::set<std::string> domains,identities;
  std::string common_hash;
  for(const auto& row:defs) {
    validate(row);
    require(domains.insert(get(row,"domain")).second,"duplicate domain");
    require(identities.insert(get(row,"mechanism_id")).second,
            "duplicate mechanism");
    if(common_hash.empty()) common_hash=get(row,"card_sha256");
    require(common_hash==get(row,"card_sha256"),"cross-source content hashes differ");
  }
  fs::path dbdir=fs::temp_directory_path()/"moolbase-f6-mechanism-evidence-ledger";
  fs::remove_all(dbdir);
  GrapheneDB db;
  DBOptions options;
  options.dimension=16;
  options.fsync_on_commit=false;
  require(bool(db.open(dbdir,options)),"cannot open native DB");
  for(const auto& row:defs) {
    NodeInput n;
    n.content=get(row,"description");
    n.vector=deterministic_text_vector(get(row,"mechanism_id")+" "+n.content,16);
    n.signature=deterministic_text_signature(get(row,"mechanism_id"));
    n.root=true;
    n.metadata=row;
    n.metadata["epistemic_class"]="project_authored_unverified";
    n.metadata["scientific_truth_claim"]="false";
    n.metadata["executor_kind"]="allowlisted_cpp_no_eval";
    const auto st=db.put_node(n);
    require(bool(st),"native procedure insert error: "+st.message);
  }
  require(db.node_count()==3 && db.edge_count()==0,
          "unexpected observed/reasoning edges in toy mechanism ledger");
  require(bool(db.close()),"native DB close failed");
  require(bool(db.open(dbdir,options)),"native DB reopen failed");
  const auto questions=read_tsv(tasks,INPUT);
  require(!questions.empty(),"no benchmark questions");
  std::ofstream out(output);
  require(bool(out),"cannot create receipt");
  out<<OUTPUT_HEADER<<'\n';
  std::set<std::string> taskids;
  size_t executed=0,abstained=0;
  for(const auto& row:questions) {
    const auto id=get(row,"task_id"),mech=get(row,"mechanism_id");
    require(taskids.insert(id).second,"duplicate task ID in native input");
    const auto ids=db.metadata_search("mechanism_id",mech);
    std::string result_source="-",result_status="unverified",node_id="-";
    Outcome answer{false,0,"unknown_mechanism"};
    if(!ids.empty()) {
      require(ids.size()==1,"ambiguous native mechanism identity");
      const auto found=db.get_node(ids.front());
      require(bool(found),"native retrieval returned missing node");
      const auto& m=found->metadata;
      require(get(m,"mechanism_id")==mech &&
              get(m,"source_status")=="unverified" &&
              get(m,"epistemic_class")=="project_authored_unverified" &&
              get(m,"scientific_truth_claim")=="false" &&
              get(m,"executor_kind")=="allowlisted_cpp_no_eval",
              "metadata provenance changed after native persistence");
      validate(m);
      Procedure p;
      p.mechanism_id=get(m,"mechanism_id");
      p.version=get(m,"version");
      p.domain=get(m,"domain");
      p.card_id=get(m,"card_id");
      p.formula_id=get(m,"formula_id");
      p.family=get(m,"source_family");
      p.card_sha=get(m,"card_sha256");
      p.input=parse_spec(get(m,"input_spec"));
      p.output=parse_spec(get(m,"output_spec"));
      node_id=std::to_string(found->id);
      result_source=p.family;
      if(get(row,"domain")!=p.domain)
        answer={false,0,"domain_mismatch"};
      else
        answer=apply(p,get(row,"target"),get(row,"target_unit"),get(row,"inputs"));
    }
    if(answer.ok) ++executed; else ++abstained;
    std::ostringstream val;
    val<<std::setprecision(17)<<answer.value;
    out<<id<<'\t'<<get(row,"domain")<<'\t'<<mech<<'\t'
       <<get(row,"target")<<'\t'<<get(row,"target_unit")<<'\t'
       <<(answer.ok?"executed_unverified":"abstain")<<'\t'
       <<(answer.ok?val.str():"-")<<'\t'
       <<(answer.ok?"-":answer.reason)<<'\t'
       <<result_source<<'\t'<<result_status<<'\t'
       <<"MOOL-F6-TYPED-V1"<<'\t'<<node_id<<'\t'<<"false"<<'\n';
  }
  require(bool(out),"cannot flush execution receipts");
  require(bool(db.close()),"native DB close failed");
  fs::remove_all(dbdir);
  std::cout<<"f6_native_typed_execution=PASS tasks="<<questions.size()
           <<" executed="<<executed<<" abstained="<<abstained
           <<" verified_facts=0"<<std::endl;
}
int main(int argc,char**argv) {
  try {
    require(argc==4,"usage: moolbase_f6_executor mechanisms.tsv inputs.tsv receipts.tsv");
    run(argv[1],argv[2],argv[3]);
  }catch(const std::exception& e) {
    std::cerr<<"f6_native_contract_error="<<e.what()<<std::endl;
    return 1;
  }
}
