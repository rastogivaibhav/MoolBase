#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace graphene {

struct EntityRecord {
  uint64_t id{0};
  std::string canonical_name;
  std::string type;
  std::vector<std::string> aliases;
  std::vector<std::string> context_terms;
};

struct EntityMention {
  std::string text;
  std::string expected_type;
  std::vector<std::string> context_terms;
};

struct EntityCandidate {
  uint64_t entity_id{0};
  double lexical_score{0.0};
  double type_score{0.0};
  double context_score{0.0};
  double total_score{0.0};
};

struct EntityResolution {
  std::vector<EntityCandidate> candidates;
  std::optional<uint64_t> resolved;
  bool ambiguous{false};
};

class EntityResolver {
 public:
  bool add(EntityRecord record);
  EntityResolution resolve(const EntityMention& mention,
                           double minimum_score = 0.65,
                           double ambiguity_margin = 0.05) const;
  std::optional<EntityRecord> get(uint64_t id) const;
  size_t size() const { return records_.size(); }

 private:
  std::vector<EntityRecord> records_;
};

std::string normalise_entity_name(const std::string& value);

}  // namespace graphene
