#pragma once

#include "graphene/db.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace graphene {

enum class ModelWorldNodeType : uint8_t {
  Fact = 0,
  Concept = 1,
  Hypothesis = 2,
  Contradiction = 3,
  Abstraction = 4,
  Opposition = 5,
  Decision = 6,
  Experiment = 7,
  Implementation = 8,
  Outcome = 9,
  Failure = 10,
  Reinforcement = 11,
  Model = 12,
  Event = 13
};

enum class ModelWorldStatus : uint8_t {
  Observed = 0,
  Discovered = 1,
  Inferred = 2,
  Hypothetical = 3,
  Contested = 4,
  Superseded = 5,
  Rejected = 6
};

struct ModelWorldNodeInput {
  std::string object_id;
  ModelWorldNodeType type{ModelWorldNodeType::Fact};
  ModelWorldStatus status{ModelWorldStatus::Hypothetical};
  std::string content;
  std::vector<float> vector;
  uint64_t signature{0};
  std::string source_id;
  std::vector<uint32_t> parent_nodes;
  std::map<std::string, std::string> metadata;
};

struct ModelWorldNode {
  uint32_t node_id{0};
  std::string object_id;
  ModelWorldNodeType type{ModelWorldNodeType::Fact};
  ModelWorldStatus status{ModelWorldStatus::Hypothetical};
  std::string content;
  std::string source_id;
  std::vector<uint32_t> parent_nodes;
};

struct ModelWorldPutResult {
  ModelWorldNode node;
  uint32_t event_node_id{0};
  bool idempotent_replay{false};
};

struct ModelWorldAuditReport {
  size_t objects{0};
  size_t events{0};
  size_t hypotheses{0};
  size_t contradictions{0};
  size_t unsupported_observed{0};
  size_t illegal_promotions{0};
  std::vector<std::string> findings;
  bool durable_writes{false};
};

const char* model_world_type_name(ModelWorldNodeType type);
const char* model_world_status_name(ModelWorldStatus status);

class ModelWorldStore {
 public:
  explicit ModelWorldStore(GrapheneDB& db);

  Status put(const ModelWorldNodeInput& input,
             ModelWorldPutResult* out = nullptr);
  std::optional<ModelWorldNode> get(const std::string& object_id) const;
  std::vector<ModelWorldNode> list(
      std::optional<ModelWorldNodeType> type = std::nullopt) const;
  ModelWorldAuditReport audit() const;

 private:
  GrapheneDB& db_;
};

}  // namespace graphene
