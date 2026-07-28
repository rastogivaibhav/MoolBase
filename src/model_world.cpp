#include "graphene/model_world.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace graphene {
namespace {

std::string metadata_value(const std::map<std::string, std::string>& metadata,
                           const std::string& key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

std::string encode_ids(const std::vector<uint32_t>& values) {
  std::ostringstream out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  return out.str();
}

std::vector<uint32_t> decode_ids(const std::string& value) {
  std::vector<uint32_t> out;
  std::stringstream in(value);
  std::string token;
  while (std::getline(in, token, ',')) {
    try {
      size_t consumed = 0;
      const auto parsed = std::stoul(token, &consumed);
      if (consumed == token.size()) out.push_back(static_cast<uint32_t>(parsed));
    } catch (...) {
    }
  }
  return out;
}

ModelWorldNodeType parse_type(const std::string& value) {
  for (int i = 0; i <= static_cast<int>(ModelWorldNodeType::Event); ++i) {
    const auto type = static_cast<ModelWorldNodeType>(i);
    if (value == model_world_type_name(type)) return type;
  }
  return ModelWorldNodeType::Fact;
}

ModelWorldStatus parse_status(const std::string& value) {
  for (int i = 0; i <= static_cast<int>(ModelWorldStatus::Rejected); ++i) {
    const auto status = static_cast<ModelWorldStatus>(i);
    if (value == model_world_status_name(status)) return status;
  }
  return ModelWorldStatus::Hypothetical;
}

ModelWorldNode from_node(const Node& node) {
  ModelWorldNode out;
  out.node_id = node.id;
  out.object_id = metadata_value(node.metadata, "model_world_object_id");
  out.type = parse_type(metadata_value(node.metadata, "model_world_type"));
  out.status = parse_status(metadata_value(node.metadata, "model_world_status"));
  out.content = node.content;
  out.source_id = metadata_value(node.metadata, "model_world_source_id");
  out.parent_nodes = decode_ids(metadata_value(node.metadata, "model_world_parent_nodes"));
  return out;
}

bool valid_identity(const std::string& value) {
  return !value.empty() && value.size() <= 256 &&
         value.find('\n') == std::string::npos &&
         value.find('\r') == std::string::npos;
}

}  // namespace

const char* model_world_type_name(ModelWorldNodeType type) {
  switch (type) {
    case ModelWorldNodeType::Fact: return "fact";
    case ModelWorldNodeType::Concept: return "concept";
    case ModelWorldNodeType::Hypothesis: return "hypothesis";
    case ModelWorldNodeType::Contradiction: return "contradiction";
    case ModelWorldNodeType::Abstraction: return "abstraction";
    case ModelWorldNodeType::Opposition: return "opposition";
    case ModelWorldNodeType::Decision: return "decision";
    case ModelWorldNodeType::Experiment: return "experiment";
    case ModelWorldNodeType::Implementation: return "implementation";
    case ModelWorldNodeType::Outcome: return "outcome";
    case ModelWorldNodeType::Failure: return "failure";
    case ModelWorldNodeType::Reinforcement: return "reinforcement";
    case ModelWorldNodeType::Model: return "model";
    case ModelWorldNodeType::Event: return "event";
  }
  return "fact";
}

const char* model_world_status_name(ModelWorldStatus status) {
  switch (status) {
    case ModelWorldStatus::Observed: return "observed";
    case ModelWorldStatus::Discovered: return "discovered";
    case ModelWorldStatus::Inferred: return "inferred";
    case ModelWorldStatus::Hypothetical: return "hypothetical";
    case ModelWorldStatus::Contested: return "contested";
    case ModelWorldStatus::Superseded: return "superseded";
    case ModelWorldStatus::Rejected: return "rejected";
  }
  return "hypothetical";
}

ModelWorldStore::ModelWorldStore(GrapheneDB& db) : db_(db) {}

Status ModelWorldStore::put(const ModelWorldNodeInput& input,
                            ModelWorldPutResult* out) {
  if (!valid_identity(input.object_id)) {
    return Status::error(ErrorCode::InvalidInput,
                         "model-world object_id is required and bounded");
  }
  if (input.content.empty() || input.content.size() > 1024 * 1024) {
    return Status::error(ErrorCode::InvalidInput,
                         "model-world content is required and bounded");
  }
  if ((input.status == ModelWorldStatus::Observed ||
       input.status == ModelWorldStatus::Discovered) &&
      input.source_id.empty()) {
    return Status::error(
        ErrorCode::InvalidInput,
        "observed/discovered model-world nodes require source evidence");
  }
  if (!input.vector.empty() && input.vector.size() != db_.dimension()) {
    return Status::error(ErrorCode::DimensionMismatch,
                         "model-world vector dimension mismatch");
  }

  const auto existing_ids =
      db_.metadata_search("model_world_object_id", input.object_id);
  if (!existing_ids.empty()) {
    const auto existing = db_.get_node(existing_ids.front());
    if (existing) {
      const ModelWorldNode decoded = from_node(*existing);
      if (decoded.type != input.type || decoded.status != input.status ||
          decoded.content != input.content ||
          decoded.source_id != input.source_id ||
          decoded.parent_nodes != input.parent_nodes) {
        return Status::error(
            ErrorCode::InvalidInput,
            "model-world object_id already exists with different content or epistemic status");
      }
      if (out) {
        out->node = decoded;
        const auto event_ids = db_.metadata_search(
            "model_world_object_id", "event:create:" + input.object_id);
        if (!event_ids.empty()) out->event_node_id = event_ids.front();
        out->idempotent_replay = true;
      }
      return Status::ok();
    }
  }

  std::set<uint32_t> unique_parents;
  std::vector<std::string> parent_external_ids;
  for (uint32_t parent : input.parent_nodes) {
    if (!unique_parents.insert(parent).second) {
      return Status::error(ErrorCode::InvalidInput,
                           "duplicate model-world parent node");
    }
    const auto parent_node = db_.get_node(parent);
    if (!parent_node) {
      return Status::error(ErrorCode::NodeNotFound,
                           "model-world parent node does not exist");
    }
    const std::string parent_object_id =
        metadata_value(parent_node->metadata, "model_world_object_id");
    if (parent_object_id.empty() ||
        metadata_value(parent_node->metadata, "model_world_record") != "object") {
      return Status::error(
          ErrorCode::InvalidInput,
          "model-world parent must reference a model-world object node");
    }
    parent_external_ids.push_back("object:" + parent_object_id);
  }

  // Object, event and all parent relationships are committed in one WAL
  // transaction through extraction ingest. This prevents the historical
  // failure mode where an object could be durable but its audit event was not.
  ExtractionInput extraction;
  extraction.source_id = "graphenedb-model-world";
  extraction.extraction_run_id = "create:" + input.object_id;
  extraction.signature = input.signature;
  extraction.idempotent = true;
  extraction.place_missing_lattice = true;

  ExtractionNode object;
  object.external_id = "object:" + input.object_id;
  object.content = input.content;
  object.vector = input.vector.empty()
                      ? std::vector<float>(db_.dimension(), 0.0f)
                      : input.vector;
  object.signature = input.signature;
  object.metadata = input.metadata;
  object.metadata["model_world_record"] = "object";
  object.metadata["model_world_object_id"] = input.object_id;
  object.metadata["model_world_type"] = model_world_type_name(input.type);
  object.metadata["model_world_status"] = model_world_status_name(input.status);
  object.metadata["model_world_source_id"] = input.source_id;
  object.metadata["model_world_parent_nodes"] = encode_ids(input.parent_nodes);
  object.metadata["model_world_event_type"] = "create";
  object.metadata["model_world_event_source"] = "graphenedb-model-world";
  extraction.nodes.push_back(std::move(object));

  ExtractionNode event;
  event.external_id = "event:create:" + input.object_id;
  event.content = "model-world create event for " + input.object_id;
  event.vector.assign(db_.dimension(), 0.0f);
  event.signature = input.signature;
  event.metadata = {
      {"model_world_record", "event"},
      {"model_world_object_id", "event:create:" + input.object_id},
      {"model_world_type", "event"},
      {"model_world_status", "observed"},
      {"model_world_source_id", "graphenedb-model-world"},
      {"model_world_event_type", "create"},
      {"model_world_event_object_id", input.object_id}};
  extraction.nodes.push_back(std::move(event));

  ExtractionRelation records;
  records.from_external_id = "event:create:" + input.object_id;
  records.to_external_id = "object:" + input.object_id;
  records.origin = EdgeOrigin::Observed;
  records.role = EdgeRole::Supports;
  records.confidence = 1.0;
  records.evidence_id = "model-world-event:create:" + input.object_id;
  records.evidence_uri = "graphenedb-model-world";
  records.evidence_text = "append-only creation event";
  records.bond_type = BondType::Synthetic;
  records.layer_coupling = LayerCoupling::Synthetic;
  records.bond_strength = 1.0;
  records.metadata = {{"model_world_relation", "records"}};
  extraction.relations.push_back(std::move(records));

  for (const std::string& parent_external_id : parent_external_ids) {
    ExtractionRelation derived;
    derived.from_external_id = parent_external_id;
    derived.to_external_id = "object:" + input.object_id;
    derived.origin = EdgeOrigin::Inferred;
    derived.role = EdgeRole::Supports;
    derived.confidence = 0.8;
    derived.evidence_id = "model-world-parent:" + parent_external_id +
                          ":" + input.object_id;
    derived.evidence_uri = "graphenedb-model-world";
    derived.evidence_text = "declared model-world parent relationship";
    derived.bond_type = BondType::Synthetic;
    derived.layer_coupling = LayerCoupling::Synthetic;
    derived.bond_strength = 0.8;
    derived.metadata = {
        {"derived_from", "model_world_parent_nodes"},
        {"model_world_relation", "derived_from"}};
    extraction.relations.push_back(std::move(derived));
  }

  ExtractionResult committed;
  const Status status = db_.put_extraction(extraction, &committed);
  if (!status) return status;

  const auto object_it =
      committed.external_to_node_id.find("object:" + input.object_id);
  const auto event_it = committed.external_to_node_id.find(
      "event:create:" + input.object_id);
  if (object_it == committed.external_to_node_id.end() ||
      event_it == committed.external_to_node_id.end()) {
    return Status::error(ErrorCode::DataCorrupt,
                         "atomic model-world commit omitted required nodes");
  }
  if (out) {
    const auto persisted = db_.get_node(object_it->second);
    if (persisted) out->node = from_node(*persisted);
    out->event_node_id = event_it->second;
    out->idempotent_replay = committed.inserted_node_ids.empty();
  }
  return Status::ok();
}

std::optional<ModelWorldNode> ModelWorldStore::get(
    const std::string& object_id) const {
  const auto ids = db_.metadata_search("model_world_object_id", object_id);
  for (uint32_t id : ids) {
    const auto node = db_.get_node(id);
    if (!node || metadata_value(node->metadata, "model_world_record") != "object") {
      continue;
    }
    return from_node(*node);
  }
  return std::nullopt;
}

std::vector<ModelWorldNode> ModelWorldStore::list(
    std::optional<ModelWorldNodeType> type) const {
  std::vector<ModelWorldNode> output;
  const std::string key = type ? "model_world_type" : "model_world_record";
  const std::string value = type ? model_world_type_name(*type) : "object";
  for (uint32_t id : db_.metadata_search(key, value)) {
    const auto node = db_.get_node(id);
    if (!node || metadata_value(node->metadata, "model_world_record") != "object") {
      continue;
    }
    output.push_back(from_node(*node));
  }
  std::sort(output.begin(), output.end(),
            [](const ModelWorldNode& a, const ModelWorldNode& b) {
              return a.node_id < b.node_id;
            });
  return output;
}

ModelWorldAuditReport ModelWorldStore::audit() const {
  ModelWorldAuditReport report;
  report.objects = db_.metadata_search("model_world_record", "object").size();
  report.events = db_.metadata_search("model_world_record", "event").size();
  report.hypotheses =
      db_.metadata_search("model_world_type", "hypothesis").size();
  report.contradictions =
      db_.metadata_search("model_world_type", "contradiction").size();

  for (const auto& object : list()) {
    if ((object.status == ModelWorldStatus::Observed ||
         object.status == ModelWorldStatus::Discovered) &&
        object.source_id.empty()) {
      ++report.unsupported_observed;
      report.findings.push_back("unsupported observed/discovered object: " +
                                object.object_id);
    }
    const auto node = db_.get_node(object.node_id);
    if (node && metadata_value(node->metadata, "promoted_from") == "reinforced") {
      ++report.illegal_promotions;
      report.findings.push_back("reinforcement-to-truth promotion detected: " +
                                object.object_id);
    }
  }
  report.durable_writes = false;
  return report;
}

}  // namespace graphene
