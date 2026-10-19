// F10: read-only forensic telemetry on the actual unchanged F9 native runtime.
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
#include <map>
#include <set>
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

struct Audit {
  size_t target_fibers{0};
  size_t fiber_paths{0};
  size_t support_role{0};
  size_t opposition_role{0};
  size_t unknown_role{0};
  size_t noise_role{0};
  size_t support_eligible{0};
  size_t opposition_eligible{0};
  size_t hypothetical_paths{0};
  size_t nonoperative_paths{0};
  size_t opposition_correlation_groups{0};
  size_t opposition_representatives_eligible{0};
  size_t missing_evidence_paths{0};
};
static Audit audit_paths(const FiberBundle& b) {
  Audit out;
  out.target_fibers=b.fibers.size();
  for(const auto& fiber:b.fibers) {
    out.fiber_paths+=fiber.paths.size();
    for(const auto& path:fiber.paths) {
      if(path.role==FiberPathRole::Support)++out.support_role;
      else if(path.role==FiberPathRole::Opposition)++out.opposition_role;
      else if(path.role==FiberPathRole::Noise)++out.noise_role;
      else ++out.unknown_role;
      out.support_eligible+=path.eligible_for_support;
      out.opposition_eligible+=path.eligible_for_opposition;
      out.hypothetical_paths+=path.contains_hypothetical;
      out.nonoperative_paths+=path.contains_nonoperative_evidence;
      out.missing_evidence_paths+=!path.validity.every_critical_edge_has_evidence;
    }
    for(const auto& group:fiber.correlation_groups) {
      if(group.role!=FiberPathRole::Opposition)continue;
      ++out.opposition_correlation_groups;
      for(const auto& path:fiber.paths) {
        if(path.id==group.representative_path_id &&
           path.eligible_for_opposition) {
          ++out.opposition_representatives_eligible;
          break;
        }
      }
    }
  }
  return out;
}
static void append_audit(std::ofstream& out,const Audit& a) {
  out<<"\"retrieved_target_fibers\":"<<a.target_fibers<<","
     <<"\"retrieved_fiber_paths\":"<<a.fiber_paths<<","
     <<"\"support_role_paths\":"<<a.support_role<<","
     <<"\"opposition_role_paths\":"<<a.opposition_role<<","
     <<"\"noise_role_paths\":"<<a.noise_role<<","
     <<"\"unknown_role_paths\":"<<a.unknown_role<<","
     <<"\"support_eligible_paths\":"<<a.support_eligible<<","
     <<"\"opposition_eligible_paths\":"<<a.opposition_eligible<<","
     <<"\"hypothetical_paths\":"<<a.hypothetical_paths<<","
     <<"\"nonoperative_paths\":"<<a.nonoperative_paths<<","
     <<"\"critical_evidence_missing_paths\":"<<a.missing_evidence_paths<<","
     <<"\"opposition_correlation_groups\":"<<a.opposition_correlation_groups<<","
     <<"\"opposition_representatives_eligible\":"<<a.opposition_representatives_eligible<<",";
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
    const Audit initial_audit=audit_paths(result.initial_bundle);
    const Audit final_audit=audit_paths(result.final_bundle);
    const bool operative_selection=result.final_convergence.has_answer;
    std::string selected_id="none";
    if(operative_selection) {
      const auto selected=db.get_node(result.final_convergence.primary_node);
      if(selected && selected->metadata.count("id") &&
         selected->metadata.count("kind") &&
         selected->metadata.at("kind")=="hypothesis") {
        selected_id=selected->metadata.at("id");
      }
    }
    // No assertion that an adversarial hypothesis must become a verified
    // answer; the critical scientific test is precisely whether it doesn't.
    const auto& rc=result.receipt;
    output<<"{\"experiment\":\"MOOL-POLYMATH-F10-001\","
         <<"\"stage\":\""<<stage<<"\",\"arm\":\""<<ablation<<"\","
         <<"\"hypothesis_count\":3,\"input_edge_count\":"<<edges.size()<<","
         <<"\"persistent_contradiction_edges\":"<<
           std::count_if(edges.begin(),edges.end(),
             [](const Edge& e){return e.role==EdgeRole::Contradicts;})<<","
         <<"\"f10_source_contract\":\"synthetic_unverified_only\","
         <<"\"operative_selection\":"<<tf(operative_selection)<<","
         <<"\"selected_hypothesis\":\""<<selected_id<<"\","
         <<"\"initial_bundle\":{";
    append_audit(output,initial_audit);
    output<<"\"end\":true},\"final_bundle\":{";
    append_audit(output,final_audit);
    output<<"\"end\":true},"
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
    std::cout<<"f10 forensic "<<stage<<" "<<ablation
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
    std::cout<<"f10_native_9_path_diagnostics=PASS "
             "synthetic_verified_facts=0 external_feedback=NOT_RUN\n";
  }catch(const std::exception& e){
    std::cerr<<"f10_native_opposition_error="<<e.what()<<"\n";
    return 1;
  }
}
