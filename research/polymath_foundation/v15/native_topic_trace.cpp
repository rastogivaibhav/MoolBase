// F15 real 20-topic MoolBase test: raw knowledge paragraphs, no answer edges.
// Native returns document IDs and epistemic status, NOT English answers.
#include "graphene/db.hpp"
#include "graphene/generic_relation.hpp"
#include "graphene/hypokosh_runtime.hpp"
#include "graphene/model_world.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
using namespace graphene;
static void demand(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
static const char* yes(bool b){return b?"true":"false";}
static std::string jescape(const std::string& s) {
  std::string t;
  for(unsigned char c:s) {
    if(c=='"')t+="\\\"";
    else if(c=='\\')t+="\\\\";
    else if(c=='\n')t+="\\n";
    else if(c<32)t+=' ';
    else t+=char(c);
  }
  return t;
}
static std::vector<std::string> split(const std::string& s,char sep) {
  std::vector<std::string> a;size_t at=0;
  for(size_t i=0;i<s.size();++i)if(s[i]==sep){a.push_back(s.substr(at,i-at));at=i+1;}
  a.push_back(s.substr(at));return a;
}
struct Article{
 std::string id,title,body,url,status;
};
static std::vector<Article> docs(const fs::path& path){
 std::ifstream f(path);
 demand(bool(f),"cannot open frozen article TSV "+path.string());
 std::string line;
 demand(bool(std::getline(f,line)),"empty frozen article table");
 demand(line=="topic_id\ttitle\tbody\tsource_url\tsource_status","unregistered article schema");
 std::vector<Article> out;std::set<std::string> seen;
 while(std::getline(f,line)) {
  auto p=split(line,'\t');
  demand(p.size()==5,"malformed source TSV article");
  demand(!p[0].empty()&&seen.insert(p[0]).second,"duplicate article ID");
  demand(p[4]=="project_authored_summary_not_third_party_authenticated",
         "article falsely claims authenticated authority");
  demand(p[2].size()>600,"short article slipped into long-topic test");
  out.push_back({p[0],p[1],p[2],p[3],p[4]});
 }
 demand(out.size()==19||out.size()==20,"only 19 or 20 authored topics allowed");
 return out;
}
static std::vector<std::pair<std::string,std::string>> questions(const fs::path& path) {
 std::ifstream f(path);demand(bool(f),"no questions table");
 std::string line;demand(bool(std::getline(f,line)),"empty queries");
 demand(line=="query_id\tquestion","unregistered questions format");
 std::vector<std::pair<std::string,std::string>> q;
 while(std::getline(f,line)){
  auto p=split(line,'\t');demand(p.size()==2,"bad question");
  q.emplace_back(p[0],p[1]);
 }
 demand(q.size()==4,"expected exactly 4 frozen prompts");return q;
}
static std::string selected_topic(const GrapheneDB& db,uint32_t id){
 auto node=db.get_node(id);
 if(!node||!node->metadata.count("topic_id"))return "none";
 return node->metadata.at("topic_id");
}

static std::string js_strings(const std::vector<std::string>& x){
 std::ostringstream o;o<<"[";
 for(size_t i=0;i<x.size();++i){if(i)o<<",";o<<"\""<<jescape(x[i])<<"\"";}
 o<<"]";return o.str();
}
template<typename T>static std::string js_numbers(const std::vector<T>& x){
 std::ostringstream o;o<<"[";
 for(size_t i=0;i<x.size();++i){if(i)o<<",";o<<x[i];}
 o<<"]";return o.str();
}
static std::string js_fibers(const FiberBundle& b){
 std::ostringstream o;
 o<<"{\"schema_version\":"<<b.schema_version
  <<",\"snapshot_version\":"<<b.snapshot_version
  <<",\"immutable_hash\":\""<<b.immutable_hash<<"\""
  <<",\"visited_states\":"<<b.visited_states
  <<",\"truncated\":"<<yes(b.truncated)
  <<",\"warnings\":"<<js_strings(b.warnings)<<",\"fibers\":[";
 for(size_t i=0;i<b.fibers.size();++i){
  if(i)o<<",";
  const auto& f=b.fibers[i];
  o<<"{\"target_node\":"<<f.target_node
    <<",\"raw_path_count\":"<<f.raw_path_count
    <<",\"independent_evidence_family_count\":"<<f.independent_evidence_family_count
    <<",\"independent_support_score\":"<<f.independent_support_score
    <<",\"contradiction_mass\":"<<f.contradiction_mass
    <<",\"evidence_coverage\":"<<f.evidence_coverage
    <<",\"paths\":[";
  for(size_t j=0;j<f.paths.size();++j){
   if(j)o<<",";
   const auto& p=f.paths[j];
   o<<"{\"id\":\""<<p.id<<"\""
     <<",\"target_node\":"<<p.target_node
     <<",\"anchor_node\":"<<p.anchor_node
     <<",\"role\":\""<<fiber_path_role_name(p.role)<<"\""
     <<",\"nodes\":"<<js_numbers(p.nodes)
     <<",\"edges\":"<<js_numbers(p.edges)
     <<",\"eligible_for_support\":"<<yes(p.eligible_for_support)
     <<",\"eligible_for_opposition\":"<<yes(p.eligible_for_opposition)
     <<",\"contains_hypothetical\":"<<yes(p.contains_hypothetical)
     <<",\"contains_contradiction\":"<<yes(p.contains_contradiction)
     <<",\"semantic_verification_code\":"<<int(p.semantic_verification)
     <<",\"confidence\":"<<p.confidence
     <<",\"relevance\":"<<p.query_relevance
     <<",\"validity\":{\"graph_continuous\":"<<yes(p.validity.graph_continuous)
     <<",\"reaches_target\":"<<yes(p.validity.reaches_target)
     <<",\"every_critical_edge_has_evidence\":"<<yes(p.validity.every_critical_edge_has_evidence)
     <<",\"critical_edge_coverage\":"<<p.validity.critical_edge_coverage
     <<",\"findings\":"<<js_strings(p.validity.findings)
     <<"},\"evidence_source_ids\":[";
   for(size_t k=0;k<p.evidence.size();++k){
     if(k)o<<",";
     o<<"\""<<jescape(p.evidence[k].source_id)<<"\"";
   }
   o<<"]}";
  }
  o<<"],\"correlation_groups\":[";
  for(size_t j=0;j<f.correlation_groups.size();++j){
   if(j)o<<",";
   const auto& g=f.correlation_groups[j];
   o<<"{\"id\":\""<<g.id<<"\",\"representative_path_id\":\""<<g.representative_path_id
     <<"\",\"independent_support\":"<<yes(g.independent_support)
     <<",\"role\":\""<<fiber_path_role_name(g.role)<<"\""
     <<",\"path_ids\":[";
   for(size_t k=0;k<g.path_ids.size();++k){
     if(k)o<<",";
     o<<"\""<<g.path_ids[k]<<"\"";
   }
   o<<"],\"evidence_family_ids\":"<<js_strings(g.evidence_family_ids)<<"}";
  }
  o<<"]}";
 }
 o<<"]}";return o.str();
}
static std::string js_admissibility(const EpistemicAdmissibility& d){
 std::ostringstream o;
 o<<"{\"relevance\":"<<d.relevance
  <<",\"target_consistency\":"<<d.target_consistency
  <<",\"completeness\":"<<d.completeness
  <<",\"provenance\":"<<d.provenance
  <<",\"independent_support\":"<<d.independent_support
  <<",\"retrieval_noise\":"<<d.retrieval_noise
  <<",\"unresolved_contradiction\":"<<d.unresolved_contradiction
  <<",\"evidence_admissible\":"<<yes(d.evidence_admissible)
  <<",\"sufficient_independent_support\":"<<yes(d.sufficient_independent_support)
  <<",\"requires_external_verification\":"<<yes(d.requires_external_verification)
  <<",\"contradiction_blocks_resolution\":"<<yes(d.contradiction_blocks_resolution)
  <<",\"semantic_tie\":"<<yes(d.semantic_tie)
  <<",\"reasons\":"<<js_strings(d.reasons)<<"}";
 return o.str();
}
static std::string js_stability(const StabilityAssessment& d){
 std::ostringstream o;
 o<<"{\"total_score\":"<<d.total_score
   <<",\"lyapunov_energy\":"<<d.lyapunov_energy
   <<",\"missing_evidence_penalty\":"<<d.missing_evidence_penalty
   <<",\"provenance_score\":"<<d.provenance_score
   <<",\"completeness_score\":"<<d.completeness_score
   <<",\"contradiction_score\":"<<d.contradiction_score
   <<",\"requires_escape\":"<<yes(d.requires_escape)
   <<",\"requires_abstention\":"<<yes(d.requires_abstention)
   <<",\"stable\":"<<yes(d.stable)
   <<",\"reasons\":"<<js_strings(d.reasons)<<"}";
 return o.str();
}
static std::string js_convergence(const ConvergedAnswer& a) {
 std::ostringstream o;
 o<<"{\"has_answer\":"<<yes(a.has_answer)
  <<",\"primary_node\":"<<a.primary_node
  <<",\"confidence\":"<<a.confidence
  <<",\"false_promotion_risk\":"<<a.false_promotion_risk
  <<",\"evidence_edges\":"<<js_numbers(a.evidence_edges)
  <<",\"residual_uncertainty\":"<<js_strings(a.residual_uncertainty)
  <<",\"selected_paths\":[";
 for(size_t i=0;i<a.selected_paths.size();++i) {
  if(i)o<<",";
  const auto& p=a.selected_paths[i];
  o<<"{\"root_node\":"<<p.root_node<<",\"path_index\":"<<p.path_index
    <<",\"reason\":\""<<jescape(p.reason)<<"\"}";
 }
 o<<"]}";return o.str();
}
static std::string js_opposition(const OppositionReport& d){
 std::ostringstream o;
 o<<"{\"challenged_claims\":"<<js_strings(d.challenged_claims)
  <<",\"falsification_questions\":"<<js_strings(d.falsification_questions)
  <<",\"corroboration_questions\":"<<js_strings(d.corroboration_questions)
  <<",\"reopen_nodes\":"<<js_numbers(d.reopen_nodes)
  <<",\"challenged_targets\":"<<js_numbers(d.challenged_targets)
  <<",\"opposition_score\":"<<d.opposition_score
  <<",\"selected_answer_opposition_score\":"<<d.selected_answer_opposition_score
  <<",\"dialectical_challenge\":"<<yes(d.dialectical_challenge)
  <<",\"corroboration_search_required\":"<<yes(d.corroboration_search_required)
  <<",\"requests_reexpansion\":"<<yes(d.requests_reexpansion)
  <<"}";
 return o.str();
}
static std::string js_events(const ReasoningReceipt& r){
 std::ostringstream o;o<<"[";
 for(size_t i=0;i<r.epistemic_events.size();++i){
  if(i)o<<",";
  const auto& e=r.epistemic_events[i];
  o<<"{\"sequence\":"<<e.sequence<<",\"round_index\":"<<e.round_index
   <<",\"event_source_code\":"<<int(e.source)
   <<",\"event_type_code\":"<<int(e.type)
   <<",\"source\":\""<<epistemic_event_source_name(e.source)<<"\""
   <<",\"type\":\""<<epistemic_event_type_name(e.type)<<"\""
   <<",\"epistemic_state\":\""<<jescape(e.epistemic_state)<<"\""
   <<",\"reason\":\""<<jescape(e.reason)<<"\""
   <<",\"hypothesis_node\":"<<e.hypothesis_node
   <<",\"competing_hypotheses\":"<<js_numbers(e.competing_hypotheses)
   <<",\"evidence_edges\":"<<js_numbers(e.evidence_edges)
   <<"}";
 }
 o<<"]";return o.str();
}
static std::string js_diagnostics(const HypoKoshRuntimeResult& r){
 std::ostringstream o;
 o<<"{\"graphene_to_hypokosh\":{\"initial_fiber_bundle\":"
  <<js_fibers(r.initial_bundle)
  <<",\"final_fiber_bundle\":"<<js_fibers(r.final_bundle)
  <<"},\"hypokosh\":{\"initial_stability\":"<<js_stability(r.initial_stability)
  <<",\"final_stability\":"<<js_stability(r.final_stability)
  <<",\"initial_admissibility\":"<<js_admissibility(r.initial_admissibility)
  <<",\"final_admissibility\":"<<js_admissibility(r.final_admissibility)
  <<",\"initial_convergence\":"<<js_convergence(r.initial_convergence)
  <<",\"final_convergence\":"<<js_convergence(r.final_convergence)
  <<",\"governed_status\":\""<<governed_status_name(r.status)<<"\""
  <<",\"residual_uncertainty\":"<<js_strings(r.residual_uncertainty)
  <<"},\"dwm\":{\"initial_opposition_inputs\":{\"bundle_hash\":\""
  <<r.initial_bundle.immutable_hash<<"\",\"convergence\":"
  <<js_convergence(r.initial_convergence)
  <<",\"admissibility\":"<<js_admissibility(r.initial_admissibility)
  <<",\"stability\":"<<js_stability(r.initial_stability)
  <<"},\"initial_opposition\":"<<js_opposition(r.initial_opposition)
  <<",\"final_opposition\":"<<js_opposition(r.final_opposition)
  <<"},\"runtime_receipt\":{\"graphene_executed\":"<<yes(r.receipt.graphene_executed)
  <<",\"fiber_bundle_built\":"<<yes(r.receipt.fiber_bundle_built)
  <<",\"epistemic_admissibility_executed\":"<<yes(r.receipt.epistemic_admissibility_executed)
  <<",\"convergence_executed\":"<<yes(r.receipt.convergence_executed)
  <<",\"opposition_executed\":"<<yes(r.receipt.opposition_executed)
  <<",\"governed_projection_executed\":"<<yes(r.receipt.governed_projection_executed)
  <<",\"model_world_updated\":"<<yes(r.receipt.model_world_updated)
  <<",\"recovery_stop_reason\":\""<<jescape(r.receipt.recovery_stop_reason)<<"\""
  <<",\"events\":"<<js_events(r.receipt)
  <<"}}";
 return o.str();
}

static void run(const fs::path& dir,const fs::path& table,
                const std::vector<std::pair<std::string,std::string>>& qs,
                std::ofstream& out,const std::string& corpus){
 fs::remove_all(dir);
 GrapheneDB db;
 DBOptions opts;opts.dimension=16;opts.fsync_on_commit=false;
 demand(bool(db.open(dir,opts)),"real GrapheneDB open failure");
 const auto list=docs(table);
 std::map<uint32_t,std::string> provenance;
 for(const auto& a:list){
  NodeInput input;
  input.content=a.title+". "+a.body;
  input.vector=deterministic_text_vector(input.content,16);
  input.signature=deterministic_text_signature(input.content);
  input.root=true; // a standalone documented topic, not a causal hypothesis
  input.metadata={{"topic_id",a.id},{"title",a.title},{"source_url",a.url},
                  {"source_status",a.status},{"independent_issuer_verified","false"},
                  {"direct_answer_preselected","false"},{"scientific_truth_verified","false"}};
  uint32_t node_id=0;
  auto status=db.put_node(input,&node_id);
  demand(bool(status),"native put_node failed");
  provenance[node_id]=a.id;
 }
 demand(db.node_count()==list.size(),"native graph node count mismatch");
 demand(db.edge_count()==0,"someone authored links into the source corpus");
 demand(bool(db.close()),"native storage close failure");
 demand(bool(db.open(dir,opts)),"native graph reopen failure");
 demand(db.node_count()==list.size()&&db.edge_count()==0,
        "reopened source graph was modified");

 out<<"{\"experiment\":\"MOOL-POLYMATH-F15-TRACE-001\","
    <<"\"corpus\":\""<<corpus<<"\",\"arm\":\"GRAPH\","
    <<"\"snapshot_version\":"<<db.snapshot()
    <<",\"node_count\":"<<db.node_count()
    <<",\"edge_count\":"<<db.edge_count()
    <<",\"nodes\":[";
 for(size_t i=0;i<list.size();++i){
  if(i)out<<",";
  const auto ids=db.metadata_search("topic_id",list[i].id);
  demand(ids.size()==1,"native topic ID not unique");
  const auto n=db.get_node(ids.front());
  demand(bool(n),"native stored node not readable");
  out<<"{\"id\":"<<n->id
     <<",\"topic_id\":\""<<jescape(list[i].id)<<"\""
     <<",\"title\":\""<<jescape(list[i].title)<<"\""
     <<",\"root\":"<<yes(n->root)
     <<",\"signature\":\""<<n->signature<<"\""
     <<",\"vector_dimension\":"<<n->vector.size()
     <<",\"vector_preview\":[";
  for(size_t k=0;k<std::min<size_t>(8,n->vector.size());++k){
    if(k)out<<",";
    out<<n->vector[k];
  }
  out<<"],\"source_status\":\""<<jescape(n->metadata.at("source_status"))<<"\""
     <<",\"stored_content_bytes\":"<<n->content.size()<<"}";
 }
 out<<"],\"edges\":[]}\n";

 const std::string classes[]={"G0","G1","G2"};
 for(const auto& q:qs){
  const auto vector=deterministic_text_vector(q.second,16);
  const auto signature=deterministic_text_signature(q.second);
  auto hits=db.vector_search(vector,5);
  out<<"{\"experiment\":\"MOOL-POLYMATH-F15-TRACE-001\","
     <<"\"corpus\":\""<<corpus<<"\",\"query_id\":\""<<q.first<<"\","
     <<"\"arm\":\"VECTOR\","
     <<"\"text_query\":\""<<jescape(q.second)<<"\","
     <<"\"result_is_natural_language_answer\":false,"
     <<"\"document_count\":"<<list.size()<<",\"top5\":[";
  for(size_t i=0;i<hits.size();++i){
    if(i)out<<",";
    out<<"{\"topic_id\":\""<<selected_topic(db,hits[i].node_id)
       <<"\",\"score\":"<<std::setprecision(9)<<hits[i].score<<"}";
  }
  out<<"],\"outcome\":\"RETRIEVAL_ONLY\"}\n";
  demand(bool(out),"cannot save vector search trace");
  ModelWorld world;
  CompleteHypoKoshRuntime engine(db,&world);
  for(const auto& arm:classes){
    RuntimeOptions options;
    options.enable_hypokosh=arm!="G0";
    options.enable_dwm=arm=="G2";
    options.update_model_world=false;
    options.max_recursive_cycles=2;
    options.dialectic.mode=QueryMode::Theoretical;
    options.dialectic.max_opposition_rounds=2;
    const auto start=std::chrono::steady_clock::now();
    const auto r=engine.reason(vector,signature,options);
    const auto elapsed=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-start).count();
    demand(world.nodes().empty()&&!r.receipt.model_world_updated,
           "unverified authored topic changed model world");
    demand(r.receipt.graphene_executed,"native evidence graph did not run");
    demand(r.receipt.opposition_executed==(arm=="G2"),
           "native dialectic ablation mismatch");
    const auto selection=r.final_convergence.has_answer?
      selected_topic(db,r.final_convergence.primary_node):"none";
    demand(selection=="none"||std::any_of(list.begin(),list.end(),[&](const Article& a){
      return a.id==selection;
    }),"selected unrelated source topic");
    out<<"{\"experiment\":\"MOOL-POLYMATH-F15-TRACE-001\","
       <<"\"corpus\":\""<<corpus<<"\",\"query_id\":\""<<q.first
       <<"\",\"arm\":\""<<arm<<"\",\"document_count\":"<<list.size()
       <<",\"text_query\":\""<<jescape(q.second)<<"\","
       <<"\"native_status\":\""<<governed_status_name(r.status)<<"\","
       <<"\"selected_topic\":\""<<selection<<"\","
       <<"\"has_selected_document\":"<<yes(r.final_convergence.has_answer)<<","
       <<"\"native_graph_edges\":"<<db.edge_count()<<","
       <<"\"semantic_candidate_count\":"<<r.final_bundle.fibers.size()<<","
       <<"\"actual_opposition_executed\":"<<yes(r.receipt.opposition_executed)<<","
       <<"\"material_challenge\":"<<yes(r.final_opposition.dialectical_challenge)<<","
       <<"\"reopen_requested\":"<<yes(r.final_opposition.requests_reexpansion)<<","
       <<"\"answer_generation_implemented\":false,"
       <<"\"scientific_claim_verified\":false,"
       <<"\"source_authentication_performed\":false,"
       <<"\"learned_weights_updated\":false,"
       <<"\"model_world_updated\":false,"
       <<"\"elapsed_ms\":"<<std::fixed<<std::setprecision(6)<<elapsed
       <<",\"raw_native_diagnostics\":"<<js_diagnostics(r)<<"}\n";
    demand(bool(out),"failed to write native reasoning result");
    std::cout<<"F15 "<<corpus<<" "<<q.first<<" "<<arm
             <<" native_status="<<governed_status_name(r.status)
             <<" selected="<<selection
             <<" challenge="<<r.final_opposition.dialectical_challenge<<"\n";
  }
 }
 demand(bool(db.close()),"native graph close failure");
}
int main(int argc,char**argv){
 try {
  demand(argc==5,"usage: native_topics C0.tsv C1.tsv queries.tsv output.jsonl");
  const auto qs=questions(argv[3]);
  std::ofstream out(argv[4]);demand(bool(out),"cannot write native result");
  run("/tmp/moolbase-f15-C0",argv[1],qs,out,"C0");
  run("/tmp/moolbase-f15-C1",argv[2],qs,out,"C1");
  std::cout<<"F15_TRACE_20TOPIC_NATIVE=PASS retriever_queries=8 genuine_native_runs=24\n";
 }catch(const std::exception& e){std::cerr<<"F15_NATIVE_FAIL="<<e.what()<<"\n";return 1;}
}
