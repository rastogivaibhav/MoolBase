// Foundation v8: real native GrapheneDB semantic alias and unit normalizer.
// The ontology is project-authored, not independent semantic learning.
#include "graphene/db.hpp"
#include "graphene/generic_relation.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
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
#include <vector>

using namespace graphene;
namespace fs=std::filesystem;
using Row=std::map<std::string,std::string>;
static void check(bool ok,const std::string& reason) {
  if(!ok) throw std::runtime_error(reason);
}
static std::vector<std::string> parts(const std::string& s,char sep) {
  std::istringstream in(s);
  std::string term;std::vector<std::string> values;
  while(std::getline(in,term,sep)) values.push_back(term);
  return values;
}
static std::string get(const Row& r,const std::string& field) {
  auto it=r.find(field);
  check(it!=r.end()&&!it->second.empty(),"missing source field "+field);
  return it->second;
}
static const std::vector<std::string> ALIAS{
  "alias","canonical","domain","field_kind","canonical_type",
  "canonical_unit","alias_unit","mul_num","mul_den",
  "source_status","source_family","relation_kind"};
static const std::vector<std::string> QUERY{
  "opaque_id","description","target","target_unit","inputs"};
static const std::string AUDIT_HEADER=
  "opaque_id\tstatus\treason\talias_count\tsource_status\tnode_receipts";
static std::vector<Row> read(const std::string& path,
                             const std::vector<std::string>& headers) {
  std::ifstream stream(path);
  check(bool(stream),"cannot open v8 TSV");
  std::string line;
  check(bool(std::getline(stream,line)),"empty v8 TSV");
  check(parts(line,'\t')==headers,"v8 TSV header mismatch");
  std::vector<Row> results;
  while(std::getline(stream,line)) {
    auto cols=parts(line,'\t');
    check(cols.size()==headers.size(),"v8 TSV malformed row");
    Row row;
    for(size_t i=0;i<cols.size();++i) {
      check(!cols[i].empty(),"v8 empty field");
      row[headers[i]]=cols[i];
    }
    results.push_back(std::move(row));
  }
  return results;
}
static int64_t strict_positive(const std::string& x) {
  size_t end=0;int64_t value=0;
  try { value=std::stoll(x,&end); }
  catch(...) { throw std::runtime_error("invalid alias scale"); }
  check(end==x.size()&&value>0,"invalid alias rational scale");
  return value;
}
static void validate_alias(const Row& r) {
  const std::string domain=get(r,"domain");
  check(domain=="networks"||domain=="biology"||domain=="supply_chain",
        "unregistered alias domain");
  const std::string kind=get(r,"field_kind");
  check(kind=="input"||kind=="output","unregistered alias kind");
  const auto type=get(r,"canonical_type");
  check(type=="integer"||type=="float","unregistered alias scalar type");
  const auto source=get(r,"source_family");
  check(get(r,"source_status")=="unverified" &&
        source=="original-project-authored-"+domain &&
        get(r,"relation_kind")=="lexical_alias_not_scientific_equivalence",
        "alias provenance promotion/forgery");
  const auto num=strict_positive(get(r,"mul_num"));
  const auto den=strict_positive(get(r,"mul_den"));
  const bool plain=(num==1&&den==1&&get(r,"alias_unit")==get(r,"canonical_unit"));
  bool converted=false;
  if(kind=="input") {
    const auto canonical=get(r,"canonical");
    converted=(domain=="biology" &&
      (canonical=="observed"||canonical=="setpoint"||
       canonical=="limit"||canonical=="disturbance") &&
      get(r,"canonical_unit")=="state_units" &&
      get(r,"alias_unit")=="centistate"&&num==1&&den==100);
    converted=converted || (domain=="supply_chain" &&
      canonical=="daily_demand" && get(r,"canonical_unit")=="items_per_day" &&
      get(r,"alias_unit")=="items_per_week"&&num==1&&den==7);
    converted=converted || (domain=="supply_chain" &&
      canonical=="lead_days" && get(r,"canonical_unit")=="days" &&
      get(r,"alias_unit")=="hours"&&num==1&&den==24);
  }
  check(plain||converted,"alias requests unsupported conversion");
  check(get(r,"alias").find_first_of("\t\n;|=@#")==std::string::npos,
        "unsafe alias identifier");
}
struct Resolved {
  bool valid{false};
  Row normalized;
  std::string status{"abstain"};
  std::string reason{"UNKNOWN_TARGET"};
  int matches{0};
  std::vector<uint32_t> ids;
};
static std::pair<Row,uint32_t> lookup(GrapheneDB& db,const std::string& alias) {
  const auto ids=db.metadata_search("alias",alias);
  if(ids.empty()) return {{},0};
  check(ids.size()==1,"ambiguous alias identity in native graph");
  const auto n=db.get_node(ids.front());
  check(bool(n),"native alias node vanished");
  const auto& meta=n->metadata;
  validate_alias(meta);
  check(get(meta,"alias")==alias&&get(meta,"epistemic_class")==
        "project_authored_unverified" &&
        get(meta,"scientific_truth_claim")=="false",
        "native ontology provenance mutation");
  return {meta,ids.front()};
}
static bool normalized_number(const std::string& raw,const Row& a,
                              std::string* normalized,std::string* reason) {
  try {
    size_t offset=0;
    const long double source=std::stold(raw,&offset);
    if(offset!=raw.size()||!std::isfinite(source)) {
      *reason="INVALID_INPUT_NUMBER";return false;
    }
    const long double num=static_cast<long double>(strict_positive(get(a,"mul_num")));
    const long double den=static_cast<long double>(strict_positive(get(a,"mul_den")));
    const long double value=source*num/den;
    if(!std::isfinite(value)) {
      *reason="NONFINITE_CANONICAL_VALUE";return false;
    }
    std::ostringstream stream;
    if(get(a,"canonical_type")=="integer") {
      const long double rounded=std::round(value);
      if(value<static_cast<long double>(std::numeric_limits<int64_t>::min())||
         value>=std::ldexp(1.0L,63)) {
        *reason="NUMERIC_OVERFLOW";return false;
      }
      if(std::fabs(value-rounded)>1e-9L) {
        *reason="NONINTEGRAL_CANONICAL_VALUE";return false;
      }
      stream<<static_cast<int64_t>(rounded);
    } else {
      stream<<std::setprecision(17)<<static_cast<double>(value);
    }
    *normalized=stream.str();
    return true;
  }catch(...) {
    *reason="INVALID_INPUT_NUMBER";return false;
  }
}
static Resolved normalize(GrapheneDB& db,const Row& query) {
  Resolved outcome;
  const auto [target,target_id]=lookup(db,get(query,"target"));
  if(target.empty()||get(target,"field_kind")!="output")
    return outcome;
  outcome.ids.push_back(target_id);
  outcome.matches=1;
  if(get(target,"alias_unit")!=get(query,"target_unit")) {
    outcome.reason="TARGET_UNIT_MISMATCH";return outcome;
  }
  const auto domain=get(target,"domain");
  const auto provided=parts(get(query,"inputs"),';');
  std::set<std::string> seen;
  std::vector<std::string> canonical;
  for(const auto& entry:provided) {
    const auto eql=entry.find('='),at=entry.find('@'),hash=entry.find('#');
    if(eql==std::string::npos||at==std::string::npos||
       hash==std::string::npos||eql>=at||at>=hash) {
      outcome.reason="MALFORMED_TYPED_INPUT";return outcome;
    }
    const auto alias=entry.substr(0,eql);
    const auto number=entry.substr(eql+1,at-eql-1);
    const auto unit=entry.substr(at+1,hash-at-1);
    const auto kind=entry.substr(hash+1);
    const auto [source,id]=lookup(db,alias);
    if(source.empty()||get(source,"field_kind")!="input") {
      outcome.reason="UNKNOWN_ALIAS";return outcome;
    }
    if(get(source,"domain")!=domain ||
       get(source,"source_family")!="original-project-authored-"+domain) {
      outcome.reason="CROSS_DOMAIN_OR_PROVENANCE";return outcome;
    }
    const auto logical=get(source,"canonical");
    if(!seen.insert(logical).second) {
      outcome.reason="DUPLICATE_CANONICAL_CONCEPT";return outcome;
    }
    if(get(source,"alias_unit")!=unit) {
      outcome.reason="INPUT_UNIT_MISMATCH";return outcome;
    }
    if(get(source,"canonical_type")!=kind) {
      outcome.reason="INPUT_TYPE_MISMATCH";return outcome;
    }
    std::string value,reason;
    if(!normalized_number(number,source,&value,&reason)) {
      outcome.reason=reason;return outcome;
    }
    canonical.push_back(logical+"="+value+"@"+
                       get(source,"canonical_unit")+"#"+kind);
    outcome.ids.push_back(id);
    ++outcome.matches;
  }
  if(canonical.empty()) {
    outcome.reason="MISSING_INPUTS";return outcome;
  }
  std::string serialized;
  for(const auto& item:canonical) {
    if(!serialized.empty()) serialized+=";";
    serialized+=item;
  }
  outcome.valid=true;outcome.status="normalized";
  outcome.reason="NORMALIZED_UNVERIFIED";
  outcome.normalized={
    {"opaque_id",get(query,"opaque_id")},
    {"description",get(query,"description")},
    {"target",get(target,"canonical")},
    {"target_unit",get(target,"canonical_unit")},
    {"inputs",serialized}
  };
  return outcome;
}
static void run(const char* ontology,const char* taskpath,
                const char* outpath,const char* auditpath) {
  const auto aliases=read(ontology,ALIAS);
  check(aliases.size()==44,"ontology expected exactly 44 reviewed aliases");
  std::set<std::string> unique;
  for(const auto& a:aliases) {
    validate_alias(a);
    check(unique.insert(get(a,"alias")).second,"duplicate source alias");
  }
  fs::path root=fs::temp_directory_path()/"moolbase-f8-semantic-alias-ontology";
  fs::remove_all(root);
  DBOptions settings;
  settings.dimension=16;
  settings.fsync_on_commit=false;
  GrapheneDB db;
  check(bool(db.open(root,settings)),"cannot open native DB");
  for(const auto& source:aliases) {
    NodeInput node;
    node.content=get(source,"alias")+" maps to "+get(source,"canonical");
    node.vector=deterministic_text_vector(node.content,16);
    node.signature=deterministic_text_signature(node.content);
    node.root=true;
    node.metadata=source;
    node.metadata["epistemic_class"]="project_authored_unverified";
    node.metadata["scientific_truth_claim"]="false";
    const auto status=db.put_node(node);
    check(bool(status),"native semantic ontology insert failed: "+status.message);
  }
  check(db.node_count()==aliases.size()&&db.edge_count()==0,
        "unexpected native model world evidence edges");
  check(bool(db.close()),"native ontology close failed");
  check(bool(db.open(root,settings)),"native ontology reopen failed");
  const auto rows=read(taskpath,QUERY);
  check(rows.size()==120,"fixed semantic trial count changed");
  std::ofstream out(outpath),audit(auditpath);
  check(bool(out)&&bool(audit),"cannot write normalized or audit outputs");
  out<<"opaque_id\tdescription\ttarget\ttarget_unit\tinputs\n";
  audit<<AUDIT_HEADER<<'\n';
  std::set<std::string> identifiers;
  int accepted=0,rejected=0;
  for(const auto& row:rows) {
    const auto id=get(row,"opaque_id");
    check(identifiers.insert(id).second,"duplicate v8 opaque task ID");
    Resolved result=normalize(db,row);
    const auto normalized=result.valid?result.normalized:
       Row{{"opaque_id",id},{"description",get(row,"description")},
           {"target","__unresolved__"},{"target_unit","__unknown__"},
           {"inputs",get(row,"inputs")}};
    if(result.valid) ++accepted;else ++rejected;
    out<<id<<'\t'<<get(normalized,"description")<<'\t'
       <<get(normalized,"target")<<'\t'
       <<get(normalized,"target_unit")<<'\t'
       <<get(normalized,"inputs")<<'\n';
    audit<<id<<'\t'<<result.status<<'\t'<<result.reason<<'\t'
         <<result.matches<<"\tunverified\t";
    if(result.ids.empty()) audit<<"-";
    else {
      for(size_t i=0;i<result.ids.size();++i) {
        if(i) audit<<"|";
        audit<<result.ids[i];
      }
    }
    audit<<'\n';
  }
  check(bool(out)&&bool(audit),"native receipt flush failed");
  check(bool(db.close()),"native ontology final close failed");
  fs::remove_all(root);
  std::cout<<"f8_real_native_MoolBase_ontology=PASS questions="<<rows.size()
           <<" normalized="<<accepted<<" refused="<<rejected
           <<" verified_facts=0"<<std::endl;
}
int main(int argc,char**argv) {
  try {
    check(argc==5,"usage: f8_native ontology.tsv queries.tsv normalized.tsv audit.tsv");
    run(argv[1],argv[2],argv[3],argv[4]);
  }catch(const std::exception& ex) {
    std::cerr<<"f8_native_error="<<ex.what()<<std::endl;
    return 1;
  }
}
