#pragma once

#include "graphene/dialectic.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

struct HypothesisProposal {
  uint32_t root_node{0};
  std::string statement;
  EdgeOrigin proposal_origin{EdgeOrigin::Hypothetical};
  double plausibility{0.0};
  std::vector<uint32_t> evidence_edges;
  std::vector<std::string> discriminating_tests;
  bool eligible_for_truth_promotion{false};
};

struct HypothesisSet {
  uint64_t snapshot_version{0};
  std::vector<HypothesisProposal> proposals;
  std::vector<std::string> discriminating_tests;
  std::vector<std::string> warnings;
  std::string epistemic_status{"abstain"};
  bool durable_writes{false};
};

class HypoKoshEngine {
 public:
  explicit HypoKoshEngine(const GrapheneDB& db);

  HypothesisSet propose(const std::vector<float>& query,
                        uint64_t query_signature,
                        const DialecticOptions& options = {},
                        size_t max_hypotheses = 8,
                        uint64_t snapshot_version = kInfVersion) const;

 private:
  const GrapheneDB& db_;
};

}  // namespace graphene
