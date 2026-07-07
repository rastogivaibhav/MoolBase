#include "graphene/db.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

static void require(Status st, const char* what) {
  if (!st) {
    std::cerr << what << ": " << st.message << "\n";
    std::abort();
  }
}

int main() {
  fs::path dir = fs::temp_directory_path() / "graphenedb_package_consumer_smoke";
  fs::remove_all(dir);

  DBOptions opt;
  opt.dimension = 3;
  opt.require_lattice = true;
  opt.tuning.enable_lattice_retrieval = true;

  GrapheneDB db;
  require(db.open(dir, opt), "open");

  ExtractionInput input;
  input.source_id = "package-consumer";
  input.signature = signature_for(1, 2);
  input.incident = 12;

  ExtractionNode root;
  root.external_id = "root";
  root.content = "installed package root";
  root.vector = {0.1f, 0.2f, 0.3f};
  root.role = ExtractionRole::Root;
  input.nodes.push_back(root);

  ExtractionNode child;
  child.external_id = "child";
  child.content = "installed package child";
  child.vector = {0.11f, 0.21f, 0.31f};
  child.role = ExtractionRole::Symptom;
  input.nodes.push_back(child);

  ExtractionRelation rel;
  rel.from_external_id = "root";
  rel.to_external_id = "child";
  rel.role = EdgeRole::Supports;
  input.relations.push_back(rel);

  ExtractionResult result;
  require(db.put_extraction(input, &result), "put_extraction");
  if (result.inserted_node_ids.size() != 2 || result.inserted_edge_ids.size() != 1) {
    std::cerr << "unexpected extraction result\n";
    return 2;
  }
  require(db.validate(), "validate");
  require(db.close(), "close");

  std::cout << "graphenedb_package_consumer_passed=true\n";
  return 0;
}
