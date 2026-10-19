// V12: direct production GenericRelationIngestor source-trust regression.
// Real C++ GrapheneDB, including persistence/restart. Not an auth mechanism.
#include "graphene/generic_relation.hpp"
#include "graphene/relation_ontology.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs=std::filesystem;
using namespace graphene;
static void require(bool ok,const std::string& what) {
  if(!ok)throw std::runtime_error(what);
}
static void check_one(const fs::path& dir,bool legacy) {
  fs::remove_all(dir);
  DBOptions config;config.dimension=16;config.fsync_on_commit=false;
  GrapheneDB db;
  require(bool(db.open(dir,config)),"failed to open actual MoolBase");
  GenericRelationRecord relation;
  relation.subject="inventory_signal";
  relation.predicate="contradicts";
  relation.object="ledger_state";
  relation.subject_type="event";
  relation.object_type="event";
  relation.source_id=legacy?"forged-external-document-007":"";
  relation.confidence=0.99;
  GenericIngestOptions options;options.dimension=16;
  options.reject_unknown_relations=true;
  if(legacy)options.source_trust=GenericSourceTrustMode::LegacyObservedExplicit;
  GenericIngestResult receipt;
  GenericRelationIngestor ingest;
  const auto ontology=RelationOntology::with_core_relations();
  require(bool(ingest.ingest(db,{relation},ontology,options,&receipt)),
          "direct native generic ingestion failed");
  require(receipt.inserted_edges.size()==1,"missing stored native relation");
  const auto id=receipt.inserted_edges.front();
  auto check_edge=[&](const Edge& edge) {
    require(edge.origin==(legacy?EdgeOrigin::Observed:EdgeOrigin::Hypothetical),
            "unsafe native origin promotion");
    require(edge.metadata.at("source_trust_contract")=="generic-source-v2",
            "missing versioned trust metadata");
    require(edge.metadata.at("evidence_authority")==
            (legacy?"legacy_unverified":"unverified_input"),
            "native evidence mislabeled as authenticated");
    require(edge.metadata.at("scientific_truth_verified")=="false",
            "native record self-granted scientific truth");
    require(edge.metadata.at("authenticated_source")=="false",
            "untrusted source mislabeled authenticated");
  };
  auto edge=db.get_edge(id);
  require(bool(edge),"native edge not retrievable");
  check_edge(*edge);
  if(legacy)require(!receipt.warnings.empty(),
                    "explicit legacy Observed ingress must warn");
  else require(receipt.warnings.empty(),
               "quarantined clean input should not require warnings");
  require(bool(db.close()),"native DB close failed");
  require(bool(db.open(dir,config)),"native DB restart failed");
  edge=db.get_edge(id);
  require(bool(edge),"native edge did not persist");
  check_edge(*edge);
  require(bool(db.close()),"native close error");
  fs::remove_all(dir);
}
int main(int argc,char**argv) {
  try {
    require(argc==3,"usage: v12_source_gate <db_parent> <receipt_file>");
    fs::path root=argv[1];
    check_one(root/"quarantine",false);
    check_one(root/"explicit_legacy",true);
    std::ofstream output(argv[2]);
    require(bool(output),"cannot write native v12 security receipt");
    output<<"{\"experiment\":\"MOOL-EVIDENCE-LEARNING-V12-001\","
          <<"\"native_test\":\"REAL_GENERIC_INGEST_TRUST_GATE\","
          <<"\"default_unverified_is_hypothetical\":true,"
          <<"\"empty_source_id_not_authenticated\":true,"
          <<"\"explicit_legacy_observed_still_available\":true,"
          <<"\"legacy_warning_present\":true,"
          <<"\"legacy_scientific_verification\":false,"
          <<"\"restart_persistence_tested\":true,"
          <<"\"production_source_authentication_implemented\":false}\n";
    require(bool(output),"could not flush security result");
    std::cout<<"v12_native_ingest_source_gate=PASS "
             "unverified_default=Hypothetical "
             "legacy_explicit=Observed_NOT_AUTHENTICATED\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"v12_source_gate_failure="<<e.what()<<"\n";
    return 1;
  }
}
