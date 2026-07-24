#pragma once

#include "graphene/types.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace graphene {

struct EvidenceRef {
  std::string source_id;
  std::string span;
  std::string observed_at;
};

struct ProvenanceFinding {
  uint32_t edge_id{0};
  std::string code;
  std::string detail;
};

struct Rfc3339Instant {
  int64_t unix_seconds{0};
  uint32_t nanosecond{0};
};

struct TemporalValidity {
  std::optional<Rfc3339Instant> valid_from;
  std::optional<Rfc3339Instant> valid_until;
};

struct EdgeProvenance {
  std::vector<EvidenceRef> evidence;
  std::vector<ProvenanceFinding> findings;
};

Status parse_rfc3339(const std::string& text, Rfc3339Instant* out);
Status parse_temporal_validity(
    const std::map<std::string, std::string>& metadata,
    TemporalValidity* out);
bool valid_at(const TemporalValidity& validity, const Rfc3339Instant& instant);
EdgeProvenance assess_edge_provenance(const Edge& edge);

} // namespace graphene
