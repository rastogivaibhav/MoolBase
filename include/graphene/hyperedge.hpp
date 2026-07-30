#pragma once

#include "graphene/db.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace graphene {

struct HyperedgeInput {
  std::string external_id;
  std::vector<uint32_t> sources;
  uint32_t target{0};
  EdgeOrigin origin{EdgeOrigin::Observed};
  EdgeRole role{EdgeRole::Causal};
  double confidence{0.9};
  std::map<std::string, std::string> metadata;
};

struct HyperedgeResult {
  std::vector<uint32_t> edge_ids;
};

Status put_hyperedge(GrapheneDB& db,
                     const HyperedgeInput& input,
                     HyperedgeResult* out = nullptr);

} // namespace graphene
