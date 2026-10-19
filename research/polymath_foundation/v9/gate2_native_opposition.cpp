// F9 Gate 2: REAL CompleteHypoKoshRuntime ablations, no simulated DWM.
// All experiments are synthetic and Hypothetical; never call them Verified.
#include "graphene/db.hpp"
#include "graphene/hypokosh_runtime.hpp"
#include "graphene/generic_relation.hpp"
#include "graphene/model_world.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using namespace graphene;
static void require(bool ok,const std::string& err) {
  if(!ok)throw std::runtime_error(err);
}
static const char* tf(bool x){return x?"true":"false";}
static std::string escaped(const std::string& s) {
  std::string out;
  for(char c:s) {
    if(c=='"')out+="\\\"";
    else if(c=='\\')out+="\\\\";
    else if(c=='\n')out+="\\n";
    else out.push_back(c);
  }
  return out;
}
static uint32_t put(GrapheneDB& db,const std::string& text,
                    const std::string& kind,
                    const std::string& id,bool root) {
  NodeInput n;
  n.content=text;
  n.vector=deterministic_text_vector(text,16);
  n.signature=deterministic_text_signature(text);
  n.root=root;
  n.metadata={
     {"id",id},{"kind",kind},{"epistemic_status","unverified"},
     {"evidence_origin","project_authored_synthetic"},
     {"independent_empirical_source","none"},
     {"not_scientific_truth","true"}};
  uint32_t index=0;
  Status s=db.put_node(n,&index);
  require(bool(s),"put_node failed: "+s.message);
  return index;
}
static void link(GrapheneDB& db,uint32_t from,uint32_t to,
                 EdgeRole role,const std::string& description,
                 double confidence) {
  EdgeInput e;
  e.from=from;e.to=to;
  e.origin=EdgeOrigin::Hypothetical;
  e.role=role;
  e.confidence=confidence;
  e.metadata={
     {"source_id","synthetic-design-only"},
     {"evidence_status","unverified"},
     {"epistemic_status","hypothetical"},
     {"synthetic_observation",description},
     {"independent_empirical_source","none"}};
  Status s=db.put_edge(e);
  require(bool(s),"put_edge failed: "+s.message);
}
static void experiment(const fs::path& root,
                       const std::string& stage,
                       std::ofstream& output) {
  fs::path dir=root/stage;
  fs::create_directories(dir);
  GrapheneDB db;
  DBOptions opts;opts.dimension=16;opts.fsync_on_commit=false;
  require(bool(db.open(dir,opts)),"cannot open stage "+stage);
  const std::string incident=
   "Stock ledger discrepancy and unfulfilled orders in a hypothetical warehouse";
  const uint32_t symptom=put(db,incident,"incident","symptom",true);
  const uint32_t hA=put(db,
    "Candidate H-A: delayed warehouse receipt posting causes inventory discrepancy",
    "hypothesis","H-A",true);
  const uint32_t hB=put(db,
    "Candidate H-B: asynchronous stock snapshot lag causes inventory discrepancy",
    "hypothesis","H-B",true);
  const uint32_t hC=put(db,
    "Candidate H-C: replenishment policy configuration causes inventory discrepancy",
    "hypothesis","H-C",true);
  link(db,symptom,hA,EdgeRole::Supports,"initial_candidate_A",0.48);
  link(db,symptom,hB,EdgeRole::Supports,"initial_candidate_B",0.46);
  link(db,symptom,hC,EdgeRole::Supports,"initial_candidate_C",0.44);
  if(stage=="contradiction"||stage=="revision") {
    // A deliberately artificial counterclaim. Not an empirical observation.
    link(db,symptom,hA,EdgeRole::Contradicts,
         "synthetic_counterexample_to_A",0.72);
  }
  if(stage=="revision") {
    link(db,symptom,hB,EdgeRole::Supports,
         "synthetic_additional_support_for_B",0.63);
    link(db,symptom,hC,EdgeRole::Contradicts,
         "synthetic_counterexample_to_C",0.58);
  }
  require(db.node_count()==4,"expected three actual competing hypotheses");
  const auto edges=db.outgoing_edges(symptom);
  require(edges.size()>=3,"native graph missing competition edges");
  for(const auto& e:edges) {
    require(e.origin==EdgeOrigin::Hypothetical,
            "input graph contains non-hypothetical evidence");
    auto it=e.metadata.find("evidence_status");
    require(it!=e.metadata.end()&&it->second=="unverified",
            "native stage falsely promoted a synthetic claim");
  }
  require(bool(db.close()),"close before persistence test failed");
  require(bool(db.open(dir,opts)),"cannot reopen actual GrapheneDB stage");
  const auto roots=db.metadata_search("kind","hypothesis");
  require(roots.size()==3,"native persistence lost competing hypotheses");
  const auto q=deterministic_text_vector(incident,16);
  const auto signature=deterministic_text_signature(incident);
  ModelWorld world;
  CompleteHypoKoshRuntime reasoner(db,&world);
  for(const std::string ablation:{"G0","G1","G2"}) {
    RuntimeOptions options;
    options.enable_hypokosh=ablation!="G0";
    options.enable_dwm=ablation=="G2";
    options.update_model_world=false;
    options.max_recursive_cycles=2;
    options.dialectic.mode=QueryMode::Empirical;
    options.dialectic.max_opposition_rounds=2;
    const auto result=reasoner.reason(q,signature,options);
    require(world.nodes().empty(),
            "synthetic evidence unexpectedly mutated world state");
    require(!result.receipt.model_world_updated,
            "reasoner reported unauthorised ModelWorld writes");
    for(uint32_t eid:result.evidence_edges) {
      const auto stored=db.get_edge(eid);
      require(bool(stored) && stored->origin==EdgeOrigin::Hypothetical,
              "returned edge not hypothetical");
      auto it=stored->metadata.find("evidence_status");
      require(it!=stored->metadata.end()&&it->second=="unverified",
              "returned synthetic edge claims external verification");
    }
    // No assertion that an adversarial hypothesis must become a verified
    // answer; the critical scientific test is precisely whether it doesn't.
    const auto& rc=result.receipt;
    output<<"{\"experiment\":\"MOOL-POLYMATH-F9-G2-001\","
         <<"\"stage\":\""<<stage<<"\",\"arm\":\""<<ablation<<"\","
         <<"\"hypothesis_count\":3,\"input_edge_count\":"<<edges.size()<<","
         <<"\"source_status\":\"project_authored_synthetic_unverified\","
         <<"\"status\":\""<<escaped(governed_status_name(result.status))<<"\","
         <<"\"primary_node\":"<<result.primary_node<<","
         <<"\"confidence\":"<<std::setprecision(12)<<result.confidence<<","
         <<"\"evidence_edge_count\":"<<result.evidence_edges.size()<<","
         <<"\"initial_opposition_score\":"<<result.initial_opposition.opposition_score<<","
         <<"\"final_opposition_score\":"<<result.final_opposition.opposition_score<<","
         <<"\"dialectical_challenge\":"<<tf(result.final_opposition.dialectical_challenge)<<","
         <<"\"opposition_reopen_requested\":"<<tf(result.final_opposition.requests_reexpansion)<<","
         <<"\"opposition_challenged_target_count\":"<<result.final_opposition.challenged_targets.size()<<","
         <<"\"reopen_target_count\":"<<result.final_opposition.reopen_nodes.size()<<","
         <<"\"actual_graphene_executed\":"<<tf(rc.graphene_executed)<<","
         <<"\"actual_convergence_executed\":"<<tf(rc.convergence_executed)<<","
         <<"\"actual_opposition_executed\":"<<tf(rc.opposition_executed)<<","
         <<"\"native_hypokosh_capability_enabled\":"<<tf(rc.hypokosh_capability_enabled)<<","
         <<"\"native_dwm_capability_enabled\":"<<tf(rc.dwm_capability_enabled)<<","
         <<"\"expansion_rounds\":"<<rc.expansion_rounds<<","
         <<"\"recovery_rounds\":"<<rc.recovery_trace.size()<<","
         <<"\"model_world_updated\":"<<tf(rc.model_world_updated)<<","
         <<"\"model_world_node_count\":"<<world.nodes().size()<<","
         <<"\"independently_verified_facts\":0,"
         <<"\"outcome_feedback_performed\":false}\n";
    require(bool(output),"failed to serialize native research trace");
    std::cout<<"f9 G2 "<<stage<<" "<<ablation
             <<" status="<<governed_status_name(result.status)
             <<" opposition_executed="<<rc.opposition_executed
             <<" challenge="<<result.final_opposition.dialectical_challenge
             <<" modelworld_writes="<<world.nodes().size()<<std::endl;
  }
  require(bool(db.close()),"native stage close failed");
}
int main(int argc,char**argv){
  try {
    require(argc==3,"usage: moolbase_v9_opposition <rootdir> <result.jsonl>");
    fs::path root(argv[1]);
    fs::remove_all(root);
    std::ofstream out(argv[2]);
    require(bool(out),"cannot create Gate 2 receipt JSONL");
    for(const std::string s:{"baseline","contradiction","revision"})
       experiment(root,s,out);
    require(bool(out),"opposition evidence write failed");
    std::cout<<"f9_native_DWM_9_real_ablations=PASS "
             "synthetic_verified_facts=0 external_feedback=NOT_RUN\n";
  }catch(const std::exception& e){
    std::cerr<<"f9_native_opposition_error="<<e.what()<<"\n";
    return 1;
  }
}
