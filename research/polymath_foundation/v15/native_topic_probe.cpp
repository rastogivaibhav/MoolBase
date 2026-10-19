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
 const std::string classes[]={"G0","G1","G2"};
 for(const auto& q:qs){
  const auto vector=deterministic_text_vector(q.second,16);
  const auto signature=deterministic_text_signature(q.second);
  auto hits=db.vector_search(vector,5);
  out<<"{\"experiment\":\"MOOL-POLYMATH-F15-001\","
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
    out<<"{\"experiment\":\"MOOL-POLYMATH-F15-001\","
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
       <<"\"elapsed_ms\":"<<std::fixed<<std::setprecision(6)<<elapsed<<"}\n";
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
  std::cout<<"F15_20TOPIC_NATIVE=PASS retriever_queries=8 genuine_native_runs=24\n";
 }catch(const std::exception& e){std::cerr<<"F15_NATIVE_FAIL="<<e.what()<<"\n";return 1;}
}
