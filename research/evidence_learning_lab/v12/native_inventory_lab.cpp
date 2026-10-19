// MOOL-EVIDENCE-LEARNING-V12-001: first real native evidence-to-reasoning
// vertical slice. Controlled synthetic fault interventions, NOT externally
// authenticated evidence, independent science or learned generalisation.
#include "graphene/db.hpp"
#include "graphene/generic_relation.hpp"
#include "graphene/hypokosh_runtime.hpp"
#include "graphene/model_world.hpp"
#include "graphene/relation_ontology.hpp"
#include <filesystem>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using namespace graphene;
static void ensure(bool ok,const std::string& why) {
  if(!ok)throw std::runtime_error(why);
}
static const char* bool_text(bool b){return b?"true":"false";}
static std::string json_escape(const std::string& s) {
  std::string out;
  for(char c:s) {
    if(c=='"')out+="\\\"";
    else if(c=='\\')out+="\\\\";
    else if(c=='\n')out+="\\n";
    else out+=c;
  }
  return out;
}
static uint32_t node(GrapheneDB& db,const std::string& text,
                     const std::string& name,bool is_root) {
  NodeInput item;
  item.content=text;
  item.vector=deterministic_text_vector(text,16);
  item.signature=deterministic_text_signature(text);
  item.root=is_root;
  item.metadata={{"case_node",name},{"evidence_status","unverified"},
    {"source_kind","project_authored_synthetic"},
    {"scientific_truth_verified","false"}};
  uint32_t id=0;
  const Status status=db.put_node(item,&id);
  ensure(bool(status),"native put_node failed "+status.message);
  return id;
}
static void relation(GrapheneDB& db,uint32_t hypothesis,uint32_t incident,
                     EdgeRole role,double weight,const std::string& event,
                     const std::string& timestamp) {
  EdgeInput edge;
  edge.from=hypothesis;
  edge.to=incident;
  edge.origin=EdgeOrigin::Hypothetical;
  edge.role=role;
  edge.confidence=weight;
  edge.metadata={{"source_id","synthetic-fault-controller"},
    {"source_family_id","single-controlled-fixture"},
    {"recorded_at",timestamp},
    {"simulated_telemetry",event},{"evidence_status","unverified"},
    {"scientific_truth_verified","false"}};
  const Status status=db.put_edge(edge);
  ensure(bool(status),"native put_edge failed "+status.message);
}
static void case_stage(const fs::path& root,int case_number,int stage,
                       std::ofstream& output) {
  const std::string case_id="incident-"+std::to_string(case_number+101);
  const std::string timepoint=stage==0?"T0":"T1";
  const std::string timestamp=stage==0?"2026-10-19T10:00:00Z":"2026-10-19T10:10:00Z";
  const fs::path path=root/(case_id+"-"+timepoint);
  fs::create_directories(path);
  GrapheneDB db;
  DBOptions opts;
  opts.dimension=16;
  opts.fsync_on_commit=false;
  ensure(bool(db.open(path,opts)),"native GrapheneDB failed to open");
  const std::string symptom_text="warehouse simulated stock ledger discrepancy with delayed orders";
  const uint32_t symptom=node(db,symptom_text,"symptom",false);
  const uint32_t hypotheses[]={
    node(db,"H-A incoming goods receipt not yet posted to ERP","H-A",true),
    node(db,"H-B asynchronous WMS stock snapshot was stale","H-B",true),
    node(db,"H-C inventory order policy misconfigured replenishment","H-C",true)
  };
  // Same starting evidence and opportunity for all three competing models.
  for(int h=0;h<3;++h) {
    relation(db,hypotheses[h],symptom,EdgeRole::Supports,
             0.35,"Initial discrepancy supports multiple possible causes",timestamp);
  }
  // Controlled fault injector provides one discriminating signal at T1.
  // The code chooses the true incident variant, but does not pass the
  // hidden adjudication label to HypoKosh-DWM at runtime.
  if(stage==1) {
    const int other=(case_number+1)%3;
    relation(db,hypotheses[other],symptom,EdgeRole::Contradicts,
             0.78,"Controlled fault-test negative correlation",timestamp);
    relation(db,hypotheses[case_number],symptom,EdgeRole::Supports,
             0.82,"Controlled fault-test strong supporting correlation",timestamp);
  }
  // Feed a separate telemetry assertion through the *actual production*
  // GenericRelationIngestor's new default unverified quarantine policy.
  GenericRelationRecord telemetry;
  telemetry.subject="inventory-snapshot";
  telemetry.predicate="contradicts";
  telemetry.object="inventory-ledger";
  telemetry.subject_type="event";
  telemetry.object_type="event";
  telemetry.source_id="synthetic-unauthenticated-telemetry";
  telemetry.confidence=0.97;
  GenericIngestOptions ingress;
  ingress.dimension=16;ingress.reject_unknown_relations=true;
  GenericIngestResult ingested;
  GenericRelationIngestor generic;
  const auto ontology=RelationOntology::with_core_relations();
  ensure(bool(generic.ingest(db,{telemetry},ontology,ingress,&ingested)),
         "generic telemetry ingest failed");
  ensure(ingested.inserted_edges.size()==1,
         "native source-ingestion was skipped");
  const auto untrusted=db.get_edge(ingested.inserted_edges.front());
  ensure(bool(untrusted) && untrusted->origin==EdgeOrigin::Hypothetical,
         "untrusted telemetry acquired observed authority");
  ensure(untrusted->metadata.at("evidence_authority")=="unverified_input",
         "untrusted telemetry acquired unexpected metadata authority");
  const size_t stored_edges=db.edge_count();
  ensure(stored_edges==size_t(stage==0?4:6),"incorrect fault state stored");
  ensure(bool(db.close()),"could not close first graph");
  ensure(bool(db.open(path,opts)),"native persistence/reopen failed");
  ensure(db.edge_count()==stored_edges,"native graph changed on restart");
  // The caller-supplied telemetry source ID is NOT authenticated. We use
  // Theoretical mode to inspect hypotheses without granting empirical truth.
  const auto question=deterministic_text_vector(symptom_text,16);
  const auto signature=deterministic_text_signature(symptom_text);
  ModelWorld modelworld;
  CompleteHypoKoshRuntime native(db,&modelworld);
  for(const std::string arm:{"G0","G1","G2"}) {
    RuntimeOptions settings;
    settings.enable_hypokosh=arm!="G0";
    settings.enable_dwm=arm=="G2";
    settings.update_model_world=false;
    settings.max_recursive_cycles=2;
    settings.dialectic.mode=QueryMode::Theoretical;
    settings.dialectic.max_hops=4;
    settings.dialectic.max_opposition_rounds=2;
    const auto t0=std::chrono::steady_clock::now();
    auto r=native.reason(question,signature,settings);
    const double ms=std::chrono::duration<double,std::milli>(
       std::chrono::steady_clock::now()-t0).count();
    ensure(modelworld.nodes().empty()&&!r.receipt.model_world_updated,
           "fixture wrongly promoted a learned/Verified model world");
    for(uint32_t id:r.evidence_edges) {
      const auto stored=db.get_edge(id);
      ensure(bool(stored)&&stored->origin==EdgeOrigin::Hypothetical,
             "research result returned an authenticated evidence edge");
    }
    std::string choice="none";
    if(r.final_convergence.has_answer) {
      auto candidate=db.get_node(r.final_convergence.primary_node);
      if(candidate&&candidate->metadata.count("case_node"))
        choice=candidate->metadata.at("case_node");
    }
    output<<"{\"experiment\":\"MOOL-EVIDENCE-LEARNING-V12-001\","
          <<"\"case_id\":\""<<case_id<<"\","
          <<"\"stage\":\""<<timepoint<<"\","
          <<"\"timestamp\":\""<<timestamp<<"\","
          <<"\"arm\":\""<<arm<<"\","
          <<"\"incident_hypotheses\":3,"
          <<"\"native_graph_edges\":"<<stored_edges<<","
          <<"\"generic_telemetry_quarantined\":true,"
          <<"\"evidence_authority\":\"unverified_synthetic\","
          <<"\"reasoning_mode\":\"Theoretical\","
          <<"\"status\":\""<<governed_status_name(r.status)<<"\","
          <<"\"selected_hypothesis\":\""<<choice<<"\","
          <<"\"has_proposed_answer\":"<<bool_text(r.final_convergence.has_answer)<<","
          <<"\"actual_dwm_opposition\":"<<bool_text(r.receipt.opposition_executed)<<","
          <<"\"dialectical_challenge\":"<<bool_text(r.final_opposition.dialectical_challenge)<<","
          <<"\"reopen_requested\":"<<bool_text(r.final_opposition.requests_reexpansion)<<","
          <<"\"recovery_rounds\":"<<r.receipt.recovery_trace.size()<<","
          <<"\"elapsed_ms\":"<<ms<<","
          <<"\"source_authenticated\":false,"
          <<"\"scientific_truth_verified\":false,"
          <<"\"independent_empirical_outcome_observed\":false,"
          <<"\"model_world_updated\":false}\n";
    ensure(bool(output),"failed to persist native reasoning receipt");
    std::cout<<"v12 "<<case_id<<" "<<timepoint<<" "<<arm
             <<" "<<governed_status_name(r.status)<<" "<<choice
             <<" challenge="<<r.final_opposition.dialectical_challenge
             <<" elapsed_ms="<<ms<<"\n";
  }
  ensure(bool(db.close()),"native graph close failed");
}
int main(int argc,char**argv) {
  try {
    ensure(argc==3,"usage: v12_native_lab <workdir> <jsonl_output>");
    const fs::path root=argv[1];
    fs::remove_all(root);
    std::ofstream output(argv[2]);
    ensure(bool(output),"cannot create native trial receipt");
    for(int i=0;i<3;++i)
      for(int stage=0;stage<2;++stage)
        case_stage(root,i,stage,output);
    ensure(bool(output),"trial receipts not flushed");
    std::cout<<"v12_native_actual_reasoning=PASS cases=3 stages=2 arms=3 "
             "new_verified_facts=0 learned_transfer=NOT_RUN\n";
  }catch(const std::exception& ex) {
    std::cerr<<"v12_lab_failure="<<ex.what()<<"\n";
    return 1;
  }
}
