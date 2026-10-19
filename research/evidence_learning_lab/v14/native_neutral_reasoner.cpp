// V14: actual native HypoKosh/DWM on raw numeric ERP/WMS event observations.
// Unlike V13 there are NO author-labelled links from event metrics to H-A/B/C.
#include "graphene/db.hpp"
#include "graphene/generic_relation.hpp"
#include "graphene/hypokosh_runtime.hpp"
#include "graphene/model_world.hpp"
#include "graphene/relation_ontology.hpp"
#include <algorithm>
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
static void check(bool b,const std::string& why){if(!b)throw std::runtime_error(why);}
static const char* boolean(bool x){return x?"true":"false";}
struct Observation {
  std::string case_id,cutoff,event_id,time,source,field,value,digest;
};
static std::vector<std::string> split(const std::string& s,char sep){
 std::vector<std::string> fields;size_t first=0;
 for(size_t i=0;i<s.size();++i)if(s[i]==sep){fields.push_back(s.substr(first,i-first));first=i+1;}
 fields.push_back(s.substr(first));return fields;
}
static std::map<std::string,std::vector<Observation>> read(const fs::path& file){
 std::ifstream in(file);
 check(bool(in),"no neutral source observation input");
 std::string line;
 check(bool(std::getline(in,line)),"no neutral measurement schema");
 if(!line.empty()&&line.back()=='\r')line.pop_back();
 check(line=="case_id\tcutoff\tevent_id\tevent_time\tsource_system\tfield_name\tvalue\tsource_sha256",
       "adapter is not neutral numeric observational schema");
 std::map<std::string,std::vector<Observation>> grouped;
 while(std::getline(in,line)){
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  const auto v=split(line,'\t');
  check(v.size()==8,"missing neutral numeric field");
  check(v[1]=="T0"||v[1]=="T1","repair/future data leaked into inference");
  check(v[2].find(v[0]+"-")==0,"unrelated event identifier");
  check(v[4]=="order_service"||v[4]=="erp_adapter"||v[4]=="wms_adapter","unknown raw source system");
  check(v[5].find("H-A")==std::string::npos&&v[5].find("H-B")==std::string::npos&&
        v[5].find("H-C")==std::string::npos,"hypothesis encoded in neutral field");
  check(v[7].size()==64,"missing event content hash");
  check(!v[6].empty()&&std::all_of(v[6].begin(),v[6].end(),[](unsigned char x){return x>='0'&&x<='9';}),
        "a numeric field was actually a causal label/string");
  grouped[v[0]+":"+v[1]].push_back({v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]});
 }
 check(grouped.size()==12,"expected 6 SQL cases each at two observed time cutoffs");
 for(const auto& p:grouped)
   check(p.second.size()==(p.first.substr(p.first.size()-2)=="T0"?2u:9u),
         "unexpected raw numeric field counts");
 return grouped;
}
static uint32_t put(GrapheneDB& db,const std::string& text,const std::string& tag,bool is_root){
 NodeInput n;
 n.content=text;
 n.vector=deterministic_text_vector(text,16);
 n.signature=deterministic_text_signature(text);
 n.root=is_root;
 n.metadata={{"candidate_id",tag},{"source_authority","unverified"},
             {"scientific_truth_verified","false"}};
 uint32_t id=0;
 check(bool(db.put_node(n,&id)),"could not persist native graph node");
 return id;
}
static void one(const fs::path& root,const std::string& key,
               const std::vector<Observation>& rows,std::ofstream& out){
 const auto id=rows.front().case_id;
 const auto cutoff=rows.front().cutoff;
 fs::path folder=root/(id+"_"+cutoff);
 fs::create_directories(folder);
 GrapheneDB db;
 DBOptions opts;opts.dimension=16;opts.fsync_on_commit=false;
 check(bool(db.open(folder,opts)),"native graph did not open");
 const std::string question="warehouse stock availability discrepancy and unfulfilled orders";
 const uint32_t symptom=put(db,question,"symptom",false);
 const uint32_t candidates[]={
   put(db,"H-A delay in ERP receipt posting","H-A",true),
   put(db,"H-B WMS snapshot no longer current","H-B",true),
   put(db,"H-C incorrect replenishment threshold","H-C",true)};
 (void)symptom;(void)candidates;
 auto ontology=RelationOntology::with_core_relations();
 GenericRelationIngestor ingestor;
 GenericIngestOptions source_gate;
 source_gate.dimension=16;
 source_gate.reject_unknown_relations=true;
 // No hypothesis edges here. A raw event describes its own numeric fields.
 for(const auto& signal:rows){
   GenericRelationRecord raw;
   raw.subject=signal.event_id;
   raw.predicate="contains";
   raw.object=signal.source+"."+signal.field+"="+signal.value;
   raw.subject_type="event";
   raw.object_type="measurement";
   raw.source_id=signal.event_id;
   raw.confidence=0.5; // neutral weight, not evidence strength or probability
   GenericIngestResult receipt;
   check(bool(ingestor.ingest(db,{raw},ontology,source_gate,&receipt)),
         "native raw observation ingestion failed");
   check(receipt.inserted_edges.size()==1,"one field should create exactly one raw link");
   auto edge=db.get_edge(receipt.inserted_edges.front());
   check(bool(edge)&&edge->origin==EdgeOrigin::Hypothetical&&
         edge->metadata.at("evidence_authority")=="unverified_input",
         "caller-supplied source obtained Observed/Verified status");
 }
 check(db.edge_count()==rows.size(),"unexpected additional causal edges");
 check(bool(db.close()),"cannot persist neutral source graph");
 check(bool(db.open(folder,opts)),"cannot restart the native graph");
 check(db.edge_count()==rows.size(),"raw field count changed during persistence");
 const auto vector=deterministic_text_vector(question,16);
 const auto signature=deterministic_text_signature(question);
 ModelWorld world;
 CompleteHypoKoshRuntime runtime(db,&world);
 for(const std::string arm:{"G1","G2"}){
   RuntimeOptions options;
   options.enable_hypokosh=true;
   options.enable_dwm=arm=="G2";
   options.update_model_world=false;
   options.max_recursive_cycles=2;
   options.dialectic.mode=QueryMode::Theoretical;
   options.dialectic.max_hops=4;
   options.dialectic.max_opposition_rounds=2;
   const auto t=std::chrono::steady_clock::now();
   auto result=runtime.reason(vector,signature,options);
   const double elapsed=std::chrono::duration<double,std::milli>(
       std::chrono::steady_clock::now()-t).count();
   check(world.nodes().empty()&&!result.receipt.model_world_updated,
         "unverified numeric source updated model world");
   std::string choice="none";
   if(result.final_convergence.has_answer){
     auto selected=db.get_node(result.final_convergence.primary_node);
     if(selected&&selected->metadata.count("candidate_id"))
       choice=selected->metadata.at("candidate_id");
   }
   check(choice=="none"||choice=="H-A"||choice=="H-B"||choice=="H-C",
         "native answer not a registered hypothesis");
   for(auto edge_id:result.evidence_edges){
     auto e=db.get_edge(edge_id);
     check(bool(e)&&e->origin==EdgeOrigin::Hypothetical,
           "unverified edge was presented as real observation");
   }
   out<<"{\"experiment\":\"MOOL-EVIDENCE-LEARNING-V14-001\","
      <<"\"case_id\":\""<<id<<"\",\"cutoff\":\""<<cutoff<<"\","
      <<"\"arm\":\""<<arm<<"\",\"status\":\""<<governed_status_name(result.status)<<"\","
      <<"\"selected_hypothesis\":\""<<choice<<"\","
      <<"\"has_answer\":"<<boolean(result.final_convergence.has_answer)<<","
      <<"\"raw_numeric_fields\":"<<rows.size()<<","
      <<"\"native_quarantined_edges\":"<<db.edge_count()<<","
      <<"\"hypothesis_labeled_edges\":0,"
      <<"\"actual_dwm_opposition\":"<<boolean(result.receipt.opposition_executed)<<","
      <<"\"dialectical_challenge\":"<<boolean(result.final_opposition.dialectical_challenge)<<","
      <<"\"reopen_requested\":"<<boolean(result.final_opposition.requests_reexpansion)<<","
      <<"\"source_authenticated\":false,\"scientific_truth_verified\":false,"
      <<"\"model_world_updated\":false,\"time_ms\":"<<std::fixed<<std::setprecision(6)<<elapsed<<"}\n";
   check(bool(out),"native receipt write failure");
   std::cout<<"v14 "<<key<<" "<<arm<<" cause="<<choice
            <<" status="<<governed_status_name(result.status)
            <<" challenge="<<result.final_opposition.dialectical_challenge<<"\n";
 }
 check(bool(db.close()),"native reasoner graph close failed");
}
int main(int argc,char**argv){
 try{
   check(argc==4,"usage: v14_native <neutral.tsv> <dbdir> <results.jsonl>");
   const auto matrix=read(argv[1]);
   fs::path root=argv[2];fs::remove_all(root);
   std::ofstream out(argv[3]);check(bool(out),"cannot write V14 receipts");
   for(const auto& p:matrix)one(root,p.first,p.second,out);
   std::cout<<"v14_native_neutral_event_ablations=PASS calls=24 source_authentication=NOT_RUN\n";
 }catch(const std::exception& e){
   std::cerr<<"v14_neutral_native_error="<<e.what()<<"\n";return 1;
 }
}
