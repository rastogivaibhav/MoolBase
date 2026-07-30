#include "graphene/lattice_placement.hpp"
#include <algorithm>
#include <cmath>

namespace graphene {

LatticeCoord hex_spiral_coord(uint32_t index, int32_t layer) {
  if (index == 0) return {0, 0, layer};
  uint32_t ring = 1;
  uint32_t ring_end = 1 + 3 * ring * (ring + 1);
  while (index >= ring_end) {
    ++ring;
    ring_end = 1 + 3 * ring * (ring + 1);
  }
  uint32_t prev_end = 1 + 3 * (ring - 1) * ring;
  uint32_t offset = index - prev_end;
  int32_t q = static_cast<int32_t>(ring);
  int32_t r = 0;
  const int dirs[6][2] = {{0, -1}, {-1, 0}, {-1, 1}, {0, 1}, {1, 0}, {1, -1}};
  for (int side = 0; side < 6; ++side) {
    for (uint32_t step = 0; step < ring; ++step) {
      if (offset == 0) return {q, r, layer};
      q += dirs[side][0];
      r += dirs[side][1];
      --offset;
    }
  }
  return {q, r, layer};
}

Status assign_lattice_batch(std::vector<NodeInput> nodes, const LatticePlacementOptions& options, LatticePlacementResult* out) {
  if (!out) return Status::error(ErrorCode::InvalidInput, "placement output cannot be null");
  if (options.strategy == PlacementStrategy::SemanticGroups) {
    std::stable_sort(nodes.begin(), nodes.end(), [](const NodeInput& a, const NodeInput& b) {
      if (a.incident != b.incident) return a.incident < b.incident;
      if (a.signature != b.signature) return a.signature < b.signature;
      if (a.root != b.root) return a.root && !b.root;
      if (a.symptom != b.symptom) return a.symptom && !b.symptom;
      if (a.impact != b.impact) return a.impact && !b.impact;
      return a.content < b.content;
    });
  }
  out->batch = {};
  out->batch.nodes.reserve(nodes.size());
  out->batch.edges.reserve(nodes.size() > 0 ? nodes.size() - 1 : 0);
  for (uint32_t i = 0; i < nodes.size(); ++i) {
    auto& n = nodes[i];
    if (!n.lattice) n.lattice = hex_spiral_coord(i, options.layer);
    if (n.signature == 0) n.signature = options.signature;
    if (n.incident == 0) n.incident = options.incident;
    out->batch.nodes.push_back(n);
    if (i > 0) {
      const auto& prev = out->batch.nodes[out->batch.nodes.size() - 2];
      const bool same_group = prev.incident == n.incident && prev.signature == n.signature;
      EdgeInput e;
      e.from = options.base_node_id + i - 1;
      e.to = options.base_node_id + i;
      e.origin = EdgeOrigin::Observed;
      e.role = EdgeRole::Supports;
      e.confidence = same_group ? options.default_bond_strength : options.default_bond_strength * 0.5;
      e.bond_type = same_group ? options.default_bond : BondType::Synthetic;
      e.layer_coupling = LayerCoupling::SameLayer;
      e.bond_strength = e.confidence;
      if (!same_group) e.defect_type = DefectType::Boundary;
      out->batch.edges.push_back(e);
    }
  }
  return Status::ok();
}

} // namespace graphene
