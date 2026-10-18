#include "graphene/db.hpp"
#include "graphene/generic_relation.hpp"
#include "graphene/hypokosh_runtime.hpp"
#include "graphene/relation_ontology.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

#ifdef NDEBUG
// No assertion-only guards: this tool is compiled under Release in CI.
#endif

struct Card {
  std::string id, domain, title, description, assumptions, boundaries;
};
static std::vector<std::string> split(const std::string& line) {
  std::istringstream in(line);
  std::vector<std::string> parts;
  std::string x;
  while(std::getline(in,x,'\t')) parts.push_back(x);
  return parts;
}
static void insist(bool condition, const std::string& error) {
  if (!condition) throw std::runtime_error(error);
}
static std::vector<Card> load(const std::string& path) {
  std::ifstream input(path);
  insist(bool(input), "input manifest not found");
  std::string header;
  std::getline(input,header);
  insist(header=="card_id\tdomain\ttitle\tdescription\tassumptions\tboundaries",
         "bad manifest header");
  std::map<std::string,Card> by_domain;
  std::string line;
  while(std::getline(input,line)) {
    const auto v=split(line);
    insist(v.size()==6, "card manifest must have six TSV fields");
    for(const auto& s:v) insist(!s.empty(),"empty concept field");
    insist(v[1]=="networks"||v[1]=="biology"||v[1]=="supply_chain",
           "unknown domain");
    insist(!by_domain.count(v[1]),"duplicate card domain");
    by_domain[v[1]]={v[0],v[1],v[2],v[3],v[4],v[5]};
  }
  insist(by_domain.size()==3,"must load exactly three domain cards");
  std::vector<Card> cards;
  for (const auto& [_,card]:by_domain) cards.push_back(card);
  return cards;
}
static void write(const std::string& path,const std::vector<Card>& cards) {
  fs::path storage=fs::temp_directory_path()/"moolbase-v5-native-card-ledger";
  fs::remove_all(storage);
  DBOptions opts;
  opts.dimension=16;
  opts.fsync_on_commit=false;
  RelationOntology ontology=RelationOntology::with_core_relations();
  const auto relation=ontology.resolve("requires");
  insist(bool(relation)&&!relation->swap_endpoints&&relation->canonical_id=="requires",
         "ontology concept relation not as preregistered");
  GrapheneDB db;
  insist(bool(db.open(storage,opts)),"cannot open native MoolBase");
  for (const Card& card:cards) {
    NodeInput node;
    node.content=card.description;
    node.vector=deterministic_text_vector(card.title+" "+card.description,16);
    node.signature=deterministic_text_signature(card.title+" "+card.description);
    node.root=true;
    node.metadata={
        {"domain",card.domain}, {"card_id",card.id},
        {"title",card.title}, {"assumptions",card.assumptions},
        {"boundaries",card.boundaries},
        {"source_family","original-project-authored-"+card.domain},
        {"evidence_status","unverified"},
        {"origin","project_authored_pedagogical"},
        {"evidence_rights","original_project_content_only"},
        {"ontology_version","polymath-v5-direction-v1"}
    };
    uint32_t nodeid=0;
    const Status concept_insert=db.put_node(node,&nodeid);
    insist(bool(concept_insert),"cannot insert concept: "+concept_insert.message);
    // GrapheneDB may legitimately assign node id 0 on a newly created DB.
    NodeInput assumed;
    assumed.content=card.assumptions;
    assumed.vector=deterministic_text_vector(card.assumptions,16);
    assumed.signature=deterministic_text_signature(card.assumptions);
    assumed.metadata={
        {"domain",card.domain},{"category","declared_assumptions"},
        {"source_family","original-project-authored-"+card.domain},
        {"evidence_status","unverified"},
        {"origin","project_authored_pedagogical"}
    };
    uint32_t assumed_id=0;
    insist(bool(db.put_node(assumed,&assumed_id)),"cannot insert assumption");
    EdgeInput e;
    e.from=nodeid;
    e.to=assumed_id;
    e.origin=EdgeOrigin::Hypothetical;
    e.role=EdgeRole::Mechanistic;
    e.confidence=0.25;
    e.metadata={
        {"relation_id",relation->canonical_id},
        {"source_id","original-project-authored-"+card.domain},
        {"evidence_status","unverified"},
        {"source_family","original-project-authored-"+card.domain}
    };
    insist(bool(db.put_edge(e)),"cannot write hypothetical assumption relation");
  }
  insist(db.node_count()==6 && db.edge_count()==3,
         "unverified graph count mismatch");
  insist(bool(db.close()),"cannot close before replay");
  insist(bool(db.open(storage,opts)),"cannot reopen persistent graph");
  std::ofstream output(path);
  insist(bool(output),"cannot open output receipt");
  output<<"card_id\tdomain\ttitle\tdescription\tassumptions\tboundaries\t"
        <<"source_family\tevidence_status\tdwm_status\tdwm_evidence_count\t"
        <<"dwm_primary_node\tdwm_world_writes\n";
  ModelWorld world;
  CompleteHypoKoshRuntime native(db,&world);
  RuntimeOptions runtime_options;
  runtime_options.enable_hypokosh=true;
  runtime_options.enable_dwm=true;
  runtime_options.update_model_world=false;
  runtime_options.max_recursive_cycles=1;
  runtime_options.dialectic.mode=QueryMode::Empirical;
  for(const Card& original:cards) {
    const auto ids=db.metadata_search("domain",original.domain);
    std::vector<Node> found;
    for(const auto id:ids) {
      auto n=db.get_node(id);
      if(n && n->metadata.count("card_id")) found.push_back(*n);
    }
    insist(found.size()==1,"card retrieval must resolve exactly one concept");
    const auto& n=found.front();
    const auto& m=n.metadata;
    auto f=[&](const char* key)->std::string {
      auto it=m.find(key); insist(it!=m.end()&&!it->second.empty(),std::string("missing metadata ")+key);
      return it->second;
    };
    insist(f("card_id")==original.id&&f("domain")==original.domain,
           "wrong card returned by native MoolBase");
    insist(f("evidence_status")=="unverified"&&
           f("origin")=="project_authored_pedagogical"&&
           f("source_family")=="original-project-authored-"+original.domain,
           "unverified source provenance boundary violated");
    // The query is the previously stored concept, NOT the test answer or
    // current task target. A3 must run the native reasoner as implemented.
    const auto inference=native.reason(n.vector,n.signature,runtime_options);
    insist(world.nodes().empty(),"inference wrote into model world unexpectedly");
    // Runtime evidence-edge IDs are *not* proof of independent verification.
    // Every returned edge must still be tagged hypothetical and unverified.
    for(uint32_t eid:inference.evidence_edges) {
      const auto edge=db.get_edge(eid);
      insist(bool(edge) && edge->origin==EdgeOrigin::Hypothetical,
             "runtime returned non-hypothetical edge");
      const auto it=edge->metadata.find("evidence_status");
      insist(it!=edge->metadata.end() && it->second=="unverified",
             "runtime returned a falsely verified evidence edge");
    }
    output<<f("card_id")<<'\t'<<f("domain")<<'\t'<<f("title")<<'\t'
          <<n.content<<'\t'<<f("assumptions")<<'\t'<<f("boundaries")<<'\t'
          <<f("source_family")<<'\t'<<f("evidence_status")<<'\t'
          <<governed_status_name(inference.status)<<'\t'
          <<inference.evidence_edges.size()<<'\t'
          <<inference.primary_node<<'\t'
          <<world.nodes().size()<<'\n';
    std::cerr<<"native_retrieved="<<original.domain
             <<" HypoKosh_DWM="<<governed_status_name(inference.status)
             <<" verified_evidence=0\n";
  }
  insist(bool(output),"cannot flush native receipts");
  insist(bool(db.close()),"cannot close graph at end");
  fs::remove_all(storage);
}
int main(int argc,char**argv) {
  try {
    insist(argc==3,"usage: native_card_context manifest.tsv native_receipts.tsv");
    auto cards=load(argv[1]);
    write(argv[2],cards);
    std::cout<<"moolbase_v5_native_retrieval_and_dwm=PASS\n";
  }catch(const std::exception& e) {
    std::cerr<<"moolbase_v5_native_error="<<e.what()<<'\n';
    return 1;
  }
}
