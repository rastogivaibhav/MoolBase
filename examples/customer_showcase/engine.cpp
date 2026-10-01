// Customer adapter: all storage, correlation and reasoning use production APIs.
#include "graphene/hypokosh_runtime.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
using namespace graphene;
namespace fs = std::filesystem;
namespace {
std::unique_ptr<GrapheneDB> db;
std::string directory = "/moolbase-demo";
DBOptions db_options;
RuntimeOptions options;
uint32_t targets[2];
const uint64_t signature = signature_for(21,34);
struct Evidence { uint32_t node; int target; std::string content, family, kind, state; bool verified; };
std::map<std::string,Evidence> records;
std::string response;
std::string quote(const std::string& s) {
  std::ostringstream o; o << '"';
  for (unsigned char c:s) {
    if(c=='"'||c=='\\') o << '\\' << c;
    else if(c<32) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
    else o << c;
  }
  o << '"'; return o.str();
}
void check(Status s) { if(!s) throw std::runtime_error(s.message); }
NodeInput node(const std::string& content, std::vector<float> vector, bool root=false) {
  NodeInput n; n.content=content; n.vector=vector; n.signature=signature;
  n.root=root; n.incident=7001; n.metadata["source"]="customer-showcase"; return n;
}
// A fixture certificate is a supplied external verification decision, not AI truth detection.
class CertificateVerifier : public PathVerifier {
  PathVerificationResult verify(const DialecticPath& p,const PathVerificationContext& c) const override {
    PathVerificationResult r; r.verifier_version="customer-fixture-certificate-v1";
    bool certified=!p.edges.empty();
    for(auto id:p.edges) {
      auto e=db->get_edge(id,c.snapshot_version);
      if(!e || !e->metadata.count("semantic_verification") || e->metadata.at("semantic_verification")!="verified") certified=false;
    }
    if(certified&&!p.contains_contradiction) r.semantic_verification=SemanticVerificationStatus::Verified;
    return r;
  }
} verifier;
std::string evaluate() {
  auto r=CompleteHypoKoshRuntime(*db).reason({0,1,0},signature,options);
  auto traces=EpistemicController().inspect_targets(r.final_bundle);
  std::ostringstream o; o << std::setprecision(10);
  o << "{\"ok\":true,\"status\":" << quote(governed_status_name(r.status))
    << ",\"answer\":" << (r.final_convergence.has_answer?r.final_convergence.primary_node:0)
    << ",\"bundleHash\":" << quote(std::to_string(r.final_bundle.immutable_hash))
    << ",\"snapshot\":" << r.receipt.snapshot_version
    << ",\"visitedStates\":" << r.final_bundle.visited_states
    << ",\"truncated\":" << (r.final_bundle.truncated?"true":"false")
    << ",\"targets\":[";
  for(int i=0;i<2;i++) {
    if(i) o << ','; auto n=db->get_node(targets[i]);
    o << "{\"id\":" << targets[i] << ",\"content\":" << quote(n->content);
    size_t families=0; double support=0,opposition=0,belief=0;
    for(auto& t:traces) if(t.target_node==targets[i]) { families=t.independent_support_family_count; support=t.support_strength; opposition=t.opposition_strength; belief=t.belief_strength; }
    o << ",\"families\":" << families << ",\"support\":" << support << ",\"opposition\":" << opposition << ",\"belief\":" << belief << '}';
  }
  o << "],\"evidence\":["; bool comma=false;
  for(auto& [id,e]:records) {
    if(comma)o<<','; comma=true;
    o << "{\"id\":" << quote(id) << ",\"content\":" << quote(e.content) << ",\"family\":" << quote(e.family)
      << ",\"target\":" << targets[e.target] << ",\"kind\":" << quote(e.kind) << ",\"state\":" << quote(e.state) << ",\"verified\":" << (e.verified?"true":"false") << '}';
  }
  o << "],\"events\":["; comma=false;
  for(auto& e:r.receipt.epistemic_events) {
    if(comma)o<<','; comma=true;
    o << "{\"type\":" << quote(epistemic_event_type_name(e.type)) << ",\"source\":" << quote(epistemic_event_source_name(e.source))
      << ",\"from\":" << e.previous_hypothesis_node << ",\"to\":" << e.hypothesis_node << ",\"reason\":" << quote(e.reason) << '}';
  }
  o << "],\"uncertainty\":["; comma=false;
  for(auto& s:r.residual_uncertainty) {if(comma)o<<',';comma=true;o<<quote(s);}
  o << "],\"receipt\":{\"coreExecuted\":" << (r.receipt.graphene_executed?"true":"false")
    << ",\"convergenceExecuted\":" << (r.receipt.convergence_executed?"true":"false")
    << ",\"noSilentPromotion\":" << (r.receipt.no_silent_promotion?"true":"false")
    << ",\"terminalCause\":" << quote(r.receipt.terminal_cause) << "}}";
  auto& p=options.prior_epistemic_state; p.available=true; p.has_answer=r.final_convergence.has_answer;
  p.operative_node=p.has_answer?r.final_convergence.primary_node:0;
  p.committed_node=p.has_answer&&(r.status==GovernedEpistemicStatus::Resolved||r.status==GovernedEpistemicStatus::ProvisionallyResolved)?p.operative_node:0;
  if(p.committed_node)p.last_committed_node=p.committed_node;
  p.status=r.status;p.bundle_hash=r.final_bundle.immutable_hash;p.stability=r.final_stability;
  return o.str();
}
template<class F> const char* guarded(F fn) {
  try {response=fn();} catch(const std::exception& e) {response="{\"ok\":false,\"error\":"+quote(e.what())+"}";}
  return response.c_str();
}
}
extern "C" {
const char* demo_reset(const char* first,const char* second) {
 return guarded([&] {
  if(!first||!second||std::string(first).size()>200||std::string(second).size()>200)throw std::runtime_error("invalid hypotheses");
  if(db)check(db->close()); db.reset(); fs::remove_all(directory); records.clear();
  db=std::make_unique<GrapheneDB>();db_options.dimension=3;
#ifdef __EMSCRIPTEN__
  db_options.fsync_on_commit=false; // Browser MEMFS has no durable disk flush.
#endif
  check(db->open(directory,db_options));uint32_t sentinel;
  check(db->put_node(node("reserved-null-sentinel",{0,-1,0}),&sentinel));
  check(db->put_node(node(first,{1,0,0},true),&targets[0]));
  check(db->put_node(node(second,{-1,0,0},true),&targets[1]));
  options=RuntimeOptions{};options.dialectic.mode=QueryMode::Empirical;
  options.dialectic.semantic_candidates=32;options.dialectic.max_hops=2;
  options.dialectic.max_paths=128;options.dialectic.max_paths_per_root=64;
  options.dialectic.max_visited_states=4096;options.dialectic.minimum_confidence=.10;
  options.dialectic.reexpansion_threshold=.25;options.max_recursive_cycles=2;
  options.update_model_world=false;options.enable_opposition_research=true; options.path_verifier=&verifier;
  return evaluate();
 });
}
const char* demo_add(const char* id,const char* content,const char* family,int target,const char* kind,int verified,const char* retire) {
 return guarded([&] {
  if(!db||!db->is_open())throw std::runtime_error("start a scenario first");
  std::string key=id,text=content,fam=family,action=kind,old=retire;
  if(key.empty()||key.size()>80||text.empty()||text.size()>1000||fam.empty()||fam.size()>80||target<0||target>1||records.size()>=100)throw std::runtime_error("invalid or excessive evidence");
  if(records.count(key))throw std::runtime_error("evidence ID already exists");
  if(action!="support"&&action!="refute"&&action!="revoke"&&action!="supersede")throw std::runtime_error("unknown evidence action");
  if((action=="revoke"||action=="supersede")&&(old.empty()||!records.count(old)||records.at(old).state!="active"))throw std::runtime_error("choose active evidence to retire");
  if(action!="revoke"&&action!="supersede"&&!old.empty())throw std::runtime_error("retirement requires revoke or supersede");
  if(!old.empty()) {
    auto& e=records.at(old); check(db->delete_node(e.node)); e.state=action=="revoke"?"revoked":"superseded";
    // Keep a nonoperative database audit record with original provenance.
    uint32_t audit; auto a=node(e.content,{0,.95f,0});a.metadata["event_id"]=old;a.metadata["evidence_state"]=e.state;
    check(db->put_node(a,&audit));EdgeInput edge;edge.from=targets[e.target];edge.to=audit;
    edge.origin=EdgeOrigin::Observed;edge.role=EdgeRole::Supports;edge.confidence=.9;
    edge.metadata={{"source_id",old},{"evidence_family_id",e.family},{"evidence_state",e.state}};
    check(db->put_edge(edge,nullptr));
  }
  uint32_t nid;auto n=node(text,{0,1,0});n.metadata["event_id"]=key;check(db->put_node(n,&nid));
  EdgeInput edge;edge.from=targets[target];edge.to=nid;edge.origin=EdgeOrigin::Observed;
  edge.role=action=="refute"?EdgeRole::Contradicts:EdgeRole::Supports;edge.confidence=action=="refute"?.95:.9;
  const std::string state=action=="revoke"?"audit_only":"active";
  edge.metadata={{"source_id",key},{"evidence_family_id","family:"+fam},{"event_kind",action},{"evidence_state",state}};
  if(verified)edge.metadata["semantic_verification"]="verified";
  if(action=="refute")edge.metadata["material"]="true";
  check(db->put_edge(edge,nullptr));records[key]={nid,target,text,"family:"+fam,action,state,verified!=0};
  return evaluate();
 });
}
const char* demo_reopen() {return guarded([] {if(!db)throw std::runtime_error("no database");check(db->close());check(db->open(directory,db_options));return evaluate();});}
}
#ifndef __EMSCRIPTEN__
int main(int argc,char**argv) {
 if(argc!=5){std::cerr<<"usage: customer_showcase NEW_DB_DIR events.tsv hypothesis1 hypothesis2\n";return 2;}
 directory=argv[1]; if(fs::exists(directory)){std::cerr<<"Use a new database directory; existing directories are never overwritten by this CLI.\n";return 2;}
 std::cout<<demo_reset(argv[3],argv[4])<<'\n';std::ifstream f(argv[2]);if(!f)return 2;
 std::string line;
 while(std::getline(f,line)) { std::vector<std::string> v;std::stringstream s(line);std::string part;while(std::getline(s,part,'\t'))v.push_back(part);while(v.size()<8)v.emplace_back();
   if(v.size()!=8)return 2;
   std::cout<<demo_add(v[0].c_str(),v[1].c_str(),v[2].c_str(),std::stoi(v[3]),v[4].c_str(),std::stoi(v[5]),v[6].c_str())<<'\n';
   if(response.find("\"ok\":false")!=std::string::npos)return 1;
 }
 std::cout<<demo_reopen()<<'\n';check(db->close());return 0;
}
#endif
