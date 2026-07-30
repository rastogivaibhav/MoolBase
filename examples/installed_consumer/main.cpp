#include "graphene/db.hpp"

#include <filesystem>
#include <iostream>
#include <vector>

int main() {
  namespace fs = std::filesystem;
  using namespace graphene;

  const fs::path directory = fs::temp_directory_path() / "graphenedb_installed_consumer";
  std::error_code ec;
  fs::remove_all(directory, ec);

  GrapheneDB db;
  DBOptions options;
  options.dimension = 3;
  options.fsync_on_commit = false;

  const Status opened = db.open(directory, options);
  if (!opened) {
    std::cerr << "open failed: " << opened.message << "\n";
    return 1;
  }

  NodeInput release;
  release.content = "release changed connection pool timeout";
  release.vector = {1.0F, 0.0F, 0.0F};
  release.signature = 7;
  release.incident = 1;
  release.root = true;
  release.metadata["source"] = "release-log";

  NodeInput failure;
  failure.content = "checkout failures increased";
  failure.vector = {0.0F, 1.0F, 0.0F};
  failure.signature = 7;
  failure.incident = 1;
  failure.symptom = true;
  failure.metadata["source"] = "monitoring";

  uint32_t release_id = 0;
  uint32_t failure_id = 0;
  if (const Status status = db.put_node(release, &release_id); !status) {
    std::cerr << "put release failed: " << status.message << "\n";
    return 1;
  }
  if (const Status status = db.put_node(failure, &failure_id); !status) {
    std::cerr << "put failure failed: " << status.message << "\n";
    return 1;
  }

  EdgeInput edge;
  edge.from = release_id;
  edge.to = failure_id;
  edge.origin = EdgeOrigin::Observed;
  edge.role = EdgeRole::Causal;
  edge.confidence = 0.94;
  edge.metadata["source_id"] = "incident-review";
  if (const Status status = db.put_edge(edge); !status) {
    std::cerr << "put edge failed: " << status.message << "\n";
    return 1;
  }

  const std::vector<SearchResult> results =
      db.vector_search({0.0F, 1.0F, 0.0F}, 3);
  if (results.empty()) {
    std::cerr << "search returned no results\n";
    return 1;
  }

  const auto top = db.get_node(results.front().node_id);
  if (!top) {
    std::cerr << "top result could not be loaded\n";
    return 1;
  }

  std::cout << "GrapheneDB consumer example passed\n";
  std::cout << "nodes=" << db.node_count() << " edges=" << db.edge_count() << "\n";
  std::cout << "top_result=\"" << top->content << "\" score="
            << results.front().score << "\n";

  if (const Status status = db.close(); !status) {
    std::cerr << "close failed: " << status.message << "\n";
    return 1;
  }
  fs::remove_all(directory, ec);
  return 0;
}
