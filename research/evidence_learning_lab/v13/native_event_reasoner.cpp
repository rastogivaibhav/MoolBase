// V13: actual GrapheneDB + actual HypoKosh/DWM consuming relations derived
// from executable ERP/WMS-like SQLite event logs, NOT oracle assignments.
#include "graphene/db.hpp"
#include "graphene/generic_relation.hpp"
#include "graphene/hypokosh_runtime.hpp"
#include "graphene/model_world.hpp"
#include "graphene/relation_ontology.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace graphene;
namespace fs=std::filesystem;

static void require(bool p,const std::string& error){if(!p)throw std::runtime_error(error);}
static const char* yes(bool p){return p?"true":"false";}
struct Signal {
 std::string case_id,cutoff,hypothesis,role,event_id,time,source,digest;
 double strength{0};
};
static std::vector<std::string> split(const std::string& input,char delimiter) {
 std::vector<std::string> parts;
 size_t start=0;
 for(size_t p=0;p<input.size();++p)if(input[p]==delimiter) {
   parts.emplace_back(input.substr(start,p-start));start=p+1;
 }
 parts.emplace_back(input.substr(start));
 return parts;
}
static std::map<std::string,std::vector<Signal>> read_signals(const fs::path& filename) {
 std::ifstream in(filename);
 require(bool(in),"could not read event adapter TSV");
 std::string text;
 require(bool(std::getline(in,text)),"missing adapter header");
 require(text=="case_id\tcutoff\thypothesis\tedge_role\tstrength\tevent_id\tevent_time\tsource_system\tsource_sha256",
         "unexpected or altered event schema");
 std::map<std::string,std::vector<Signal>> grouped;
 while(std::getline(in,text)) {
   auto v=split(text,'\t');
   require(v.size()==9,"malformed evidence TSV");
   require(v[1]=="T0"||v[1]=="T1","future repaired outcome leaked into inference");
   require(v[2]=="H-A"||v[2]=="H-B"||v[2]=="H-C",
           "unexpected hypothesis identifier");
   require(v[3]=="supports"||v[3]=="contradicts","invalid relation role");
   require(v[8].size()==64,"no recorded event content digest");
   const double strength=std::stod(v[4]);
   require(std::isfinite(strength)&&strength>0&&strength<=1,"invalid strength");
   grouped[v[0]+":"+v[1]].push_back({v[0],v[1],v[2],v[3],v[5],v[6],v[7],v[8],strength});
 }
 require(grouped.size()==12,"not six incidents x two event cutoffs");
 for(const auto& group:grouped) {
   const size_t want=group.first.substr(group.first.size()-2)=="T0"?3:6;
   require(group.second.size()==want,"missing candidate edges in native stage");
 }
 return grouped;
}
static uint32_t node(GrapheneDB& db,const std::string& name,
                     const std::string& tag,bool root) {
 NodeInput input;
 input.content=name;
 input.vector=deterministic_text_vector(name,16);
 input.signature=deterministic_text_signature(name);
 input.root=root;
 input.metadata={{"hypothesis_id",tag},{"source_authority","unverified"},
                 {"source_kind","controlled_erp_wms_sqlite_sandbox"},
                 {"scientific_truth_verified","false"}};
 uint32_t id=0;
 auto result=db.put_node(input,&id);
 require(bool(result),"native node insertion failed");
 return id;
}
static void add_relation(GrapheneDB& db,uint32_t hypothesis,uint32_t symptom,
                         const Signal& e) {
 EdgeInput link;
 link.from=hypothesis;link.to=symptom;
 link.origin=EdgeOrigin::Hypothetical;
 link.role=e.role=="contradicts"?EdgeRole::Contradicts:EdgeRole::Supports;
 link.confidence=e.strength;
 link.metadata={
   {"source_id",e.event_id},{"source_family_id","controlled_sandbox_single_issuer"},
   {"event_time",e.time},{"source_system",e.source},
   {"source_content_sha256",e.digest},
   {"evidence_status","unverified"},{"source_authenticated","false"},
   {"scientific_truth_verified","false"},
   {"relation_derivation","project_authored_event_adapter_v13"}
 };
 auto result=db.put_edge(link);
 require(bool(result),"native hypothetical edge insertion failed");
}
static void run_case(const fs::path& root,const std::string& key,
                     const std::vector<Signal>& rows,std::ofstream& out) {
 const auto id=rows.front().case_id;
 const auto cutoff=rows.front().cutoff;
 const auto dir=root/(id+"_"+cutoff);
 fs::create_directories(dir);
 GrapheneDB db;
 DBOptions opts;opts.dimension=16;opts.fsync_on_commit=false;
 require(bool(db.open(dir,opts)),"cannot open native graph");
 const std::string symptom_text="warehouse stock availability discrepancy and unfulfilled orders";
 const uint32_t symptom=node(db,symptom_text,"symptom",false);
 const uint32_t h[3]={
   node(db,"H-A delay in ERP receipt posting","H-A",true),
   node(db,"H-B WMS snapshot no longer current","H-B",true),
   node(db,"H-C incorrect replenishment threshold","H-C",true)
 };
 // Actual production ingestion invoked with raw telemetry assertion; the
 // source ID is merely a caller-supplied event identifier, NOT authority.
 GenericRelationRecord raw;
 raw.subject="ERP-count";raw.predicate="contradicts";raw.object="WMS-count";
 raw.subject_type="event";raw.object_type="event";
 raw.source_id=rows.front().event_id;raw.confidence=0.96;
 GenericIngestOptions ingestion;ingestion.dimension=16;
 ingestion.reject_unknown_relations=true;
 GenericIngestResult ingested;
 GenericRelationIngestor adapter;
 auto ontology=RelationOntology::with_core_relations();
 require(bool(adapter.ingest(db,{raw},ontology,ingestion,&ingested)),
         "actual generic source ingestion failed");
 require(ingested.inserted_edges.size()==1,"raw event was not persisted");
 const auto raw_edge=db.get_edge(ingested.inserted_edges.front());
 require(bool(raw_edge) && raw_edge->origin==EdgeOrigin::Hypothetical &&
         raw_edge->metadata.at("evidence_authority")=="unverified_input",
         "unsafe raw telemetry source promotion");
 for(const auto& e:rows) {
   int i=e.hypothesis=="H-A"?0:e.hypothesis=="H-B"?1:2;
   add_relation(db,h[i],symptom,e);
 }
 const size_t edge_count=db.edge_count();
 require(edge_count==rows.size()+1,"unexpected extra graph evidence");
 require(bool(db.close()),"initial database close failed");
 require(bool(db.open(dir,opts)),"stored evidence did not reopen");
 require(db.edge_count()==edge_count,"source events not durable");
 const auto query=deterministic_text_vector(symptom_text,16);
 const auto signature=deterministic_text_signature(symptom_text);
 ModelWorld world;
 CompleteHypoKoshRuntime reasoner(db,&world);
 for(const auto arm:{"G1","G2"}) {
   RuntimeOptions settings;
   settings.enable_hypokosh=true;
   settings.enable_dwm=std::string(arm)=="G2";
   settings.update_model_world=false;
   settings.max_recursive_cycles=2;
   settings.dialectic.mode=QueryMode::Theoretical;
   settings.dialectic.max_hops=4;
   settings.dialectic.max_opposition_rounds=2;
   auto start=std::chrono::steady_clock::now();
   const auto r=reasoner.reason(query,signature,settings);
   double elapsed=std::chrono::duration<double,std::milli>(
                     std::chrono::steady_clock::now()-start).count();
   require(world.nodes().empty()&&!r.receipt.model_world_updated,
           "untrusted data modified verified learning model");
   for(uint32_t eid:r.evidence_edges) {
     const auto e=db.get_edge(eid);
     require(bool(e)&&e->origin==EdgeOrigin::Hypothetical,
             "returned evidence had unauthorized provenance");
   }
   std::string choice="none";
   if(r.final_convergence.has_answer) {
     const auto n=db.get_node(r.final_convergence.primary_node);
     if(n && n->metadata.count("hypothesis_id")!=0)
       choice=n->metadata.at("hypothesis_id");
   }
   out<<"{\"experiment\":\"MOOL-EVIDENCE-LEARNING-V13-001\","
      <<"\"case_id\":\""<<id<<"\",\"cutoff\":\""<<cutoff<<"\","
      <<"\"arm\":\""<<arm<<"\",\"status\":\""<<governed_status_name(r.status)<<"\","
      <<"\"selected_hypothesis\":\""<<choice<<"\","
      <<"\"has_answer\":"<<yes(r.final_convergence.has_answer)<<","
      <<"\"input_signal_count\":"<<rows.size()<<","
      <<"\"persisted_graph_edges\":"<<edge_count<<","
      <<"\"actual_native_opposition\":"<<yes(r.receipt.opposition_executed)<<","
      <<"\"challenge\":"<<yes(r.final_opposition.dialectical_challenge)<<","
      <<"\"reopen\":"<<yes(r.final_opposition.requests_reexpansion)<<","
      <<"\"source_quarantined\":true,\"source_authenticated\":false,"
      <<"\"scientific_truth_verified\":false,\"model_world_updated\":false,"
      <<"\"elapsed_ms\":"<<std::fixed<<std::setprecision(6)<<elapsed<<"}\n";
   require(bool(out),"could not write real native result");
   std::cout<<"v13 native "<<id<<" "<<cutoff<<" "<<arm
            <<" status="<<governed_status_name(r.status)
            <<" cause="<<choice<<" challenged="<<r.final_opposition.dialectical_challenge<<"\n";
 }
 require(bool(db.close()),"native graph close failure");
}
int main(int argc,char**argv) {
 try {
   require(argc==4,"usage: v13_native_event_lab <relations.tsv> <workdir> <receipts.jsonl>");
   auto grouped=read_signals(argv[1]);
   fs::path root(argv[2]);
   fs::remove_all(root);
   std::ofstream receipts(argv[3]);
   require(bool(receipts),"failed to open native receipts");
   for(auto& group:grouped)run_case(root,group.first,group.second,receipts);
   std::cout<<"v13_actual_native_event_inference=PASS 6_cases 2_times 2_arms "
               "external_authentication=NO future_outcomes_leaked=NO\n";
   return 0;
 }catch(const std::exception& e){
   std::cerr<<"v13_native_failure="<<e.what()<<"\n";
   return 1;
 }
}
