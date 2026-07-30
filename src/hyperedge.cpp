#include "graphene/hyperedge.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <sstream>

namespace graphene {
namespace {

std::string join_sources(const std::vector<uint32_t>& sources) {
  std::ostringstream output;
  for (size_t index = 0; index < sources.size(); ++index) {
    if (index != 0) output << ',';
    output << sources[index];
  }
  return output.str();
}

std::string next_group_id(const GrapheneDB& db,
                          const std::string& external_id) {
  static std::atomic<uint64_t> sequence{0};
  return external_id + ":" + std::to_string(db.snapshot()) + ":" +
         std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
}

} // namespace

Status put_hyperedge(GrapheneDB& db,
                     const HyperedgeInput& input,
                     HyperedgeResult* out) {
  if (!db.is_open()) {
    return Status::error(ErrorCode::NotOpen, "database is not open");
  }
  if (input.external_id.empty()) {
    return Status::error(ErrorCode::InvalidInput,
                         "hyperedge external_id must not be empty");
  }
  if (input.sources.size() < 2) {
    return Status::error(
        ErrorCode::InvalidInput,
        "joint-causality hyperedge requires at least two sources");
  }
  if (!std::isfinite(input.confidence) || input.confidence < 0.0 ||
      input.confidence > 1.0) {
    return Status::error(ErrorCode::InvalidInput,
                         "hyperedge confidence must be finite and in [0,1]");
  }

  std::vector<uint32_t> sources = input.sources;
  std::sort(sources.begin(), sources.end());
  if (std::adjacent_find(sources.begin(), sources.end()) != sources.end()) {
    return Status::error(ErrorCode::InvalidInput,
                         "hyperedge sources must be unique");
  }
  if (std::binary_search(sources.begin(), sources.end(), input.target)) {
    return Status::error(ErrorCode::InvalidInput,
                         "hyperedge target cannot also be a source");
  }
  if (!db.get_node(input.target)) {
    return Status::error(ErrorCode::NodeNotFound,
                         "hyperedge target node does not exist");
  }
  for (uint32_t source : sources) {
    if (!db.get_node(source)) {
      return Status::error(ErrorCode::NodeNotFound,
                           "hyperedge source node does not exist");
    }
  }

  const std::string encoded_sources = join_sources(sources);
  const std::string group_id = next_group_id(db, input.external_id);
  BatchInput batch;
  batch.edges.reserve(sources.size());
  for (uint32_t source : sources) {
    EdgeInput edge;
    edge.from = source;
    edge.to = input.target;
    edge.origin = input.origin;
    edge.role = input.role;
    edge.confidence = input.confidence;
    edge.metadata = input.metadata;
    edge.metadata["hyperedge_id"] = input.external_id;
    edge.metadata["hyperedge_group_id"] = group_id;
    edge.metadata["hyperedge_semantics"] = "all_sources";
    edge.metadata["hyperedge_sources"] = encoded_sources;
    edge.metadata["hyperedge_arity"] = std::to_string(sources.size());
    batch.edges.push_back(std::move(edge));
  }

  BatchResult result;
  const Status status = db.put_batch(batch, &result);
  if (!status) return status;
  if (out) out->edge_ids = std::move(result.edge_ids);
  return Status::ok();
}

} // namespace graphene
