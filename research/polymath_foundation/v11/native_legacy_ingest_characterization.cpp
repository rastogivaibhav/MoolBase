// F11 native characterization ONLY: legacy generic ingestion accepts untrusted
// source metadata as an Observed edge. This is a known boundary, NOT its fix.
#include "graphene/generic_relation.hpp"
#include "graphene/relation_ontology.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace graphene;
namespace fs=std::filesystem;
static void check(bool ok,const char* why){
  if(!ok)throw std::runtime_error(why);
}
int main(int argc,char**argv){
  try{
    check(argc==3,"usage: f11-native source-db-directory result.json");
    fs::path db_dir=argv[1];
    fs::remove_all(db_dir);
    GrapheneDB db;
    DBOptions opts;opts.dimension=16;opts.fsync_on_commit=false;
    check(bool(db.open(db_dir,opts)),"native graph could not open");
    auto ontology=RelationOntology::with_core_relations();
    GenericRelationRecord untrusted;
    untrusted.subject="machine_feed";
    untrusted.predicate="contradicts";
    untrusted.object="expected_result";
    untrusted.subject_type="event";
    untrusted.object_type="event";
    untrusted.source_id=""; // untrusted field, no external verification
    untrusted.confidence=0.95;
    GenericIngestOptions policy;policy.dimension=16;
    policy.reject_unknown_relations=true;
    GenericIngestResult result;
    GenericRelationIngestor generic;
    Status status=generic.ingest(db,{untrusted},ontology,policy,&result);
    check(bool(status),"generic ingestion unexpectedly declined input");
    check(result.inserted_edges.size()==1,"native input did not create exactly one edge");
    const auto edge=db.get_edge(result.inserted_edges.front());
    check(bool(edge),"native edge not retrievable");
    check(edge->metadata.count("source_id")!=0,"legacy edge lacks claimed source identifier");
    const bool accepted_observed=edge->origin==EdgeOrigin::Observed;
    const bool used_fallback=edge->metadata.at("source_id")=="generic-input";
    check(accepted_observed && used_fallback,
          "legacy generic Observed/fallback behaviour changed; re-evaluate trust audit");
    check(bool(db.close()),"native graph close failed");
    check(bool(db.open(db_dir,opts)),"native graph reopen failed");
    const auto after=db.get_edge(result.inserted_edges.front());
    check(bool(after)&&after->origin==EdgeOrigin::Observed,
          "native persisted edge no longer matches receipt");
    std::ofstream file(argv[2]);
    check(bool(file),"failed to create characterization receipt");
    file<<"{\"experiment\":\"MOOL-POLYMATH-F11-001\","
        <<"\"characterization\":\"EXISTING_UNTRUSTED_INGEST_BEHAVIOR_NOT_A_REMEDIATION\","
        <<"\"real_native_generic_ingestor_executed\":true,"
        <<"\"untrusted_source_can_set_observed_origin\":"<<(accepted_observed?"true":"false")<<","
        <<"\"source_id_fallback_generic_input\":"<<(used_fallback?"true":"false")<<","
        <<"\"external_source_authenticated\":false,"
        <<"\"scientific_truth_verified\":false,"
        <<"\"production_issue_requires_fix\":true}\n";
    check(bool(file),"characterization receipt failed");
    check(bool(db.close()),"native database close failed");
    fs::remove_all(db_dir);
    std::cout<<"f11_native_existing_generic_ingestion=CHARACTERIZED_UNTRUSTED_OBSERVED "
             "NOT_FIXED external_truth=NO\n";
  }catch(const std::exception& ex){
    std::cerr<<"f11_native_error="<<ex.what()<<"\n";
    return 1;
  }
}
