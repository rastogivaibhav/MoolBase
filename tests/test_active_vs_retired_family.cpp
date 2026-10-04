// An audit-only path must never suppress an operative path from its family.
#include "graphene/fiber_bundle.hpp"
#include <algorithm>
#include <iostream>
using namespace graphene;

namespace {
DialecticPath make_path(uint32_t edge, const char* source,
                        const char* state, double score) {
  DialecticPath path;
  path.root_node = 1;
  path.anchor_node = 999;
  path.nodes = {1, 999};
  path.edges = {edge};
  path.score = score;
  path.query_relevance = path.target_consistency = path.completeness = 1;
  path.temporal_consistent = true;
  path.semantic_verification = SemanticVerificationStatus::Verified;
  path.evidence.emplace_back(source, "span", "", "shared-family", "", "", state);
  return path;
}
}

int main() {
  int failures = 0;
  for (const char* state : {"superseded", "revoked", "invalidated", "audit_only"}) {
    for (bool opposition : {false, true}) {
      for (bool reverse : {false, true}) {
        RootBundle root;
        root.root_node = 1;
        auto active = make_path(101, "active-copy", "active", .80);
        auto retired = make_path(102, "retired-record", state, .99);
        active.contains_contradiction = retired.contains_contradiction = opposition;
        root.paths = {active, retired};
        if (reverse) std::reverse(root.paths.begin(), root.paths.end());
        BundleSet bundle;
        bundle.snapshot_version = 100;
        bundle.roots = {root};
        const auto fibers = FiberBundleBuilder().build(bundle);
        bool operative_representative = false;
        bool ok = fibers.fibers.size() == 1;
        if (ok) {
          const auto& fiber = fibers.fibers.front();
          ok = fiber.paths.size() == 2 && fiber.correlation_groups.size() == 1;
          for (const auto& group : fiber.correlation_groups) {
            for (const auto& path : fiber.paths) {
              if (path.id == group.representative_path_id)
                operative_representative |= opposition ? path.eligible_for_opposition
                                                       : path.eligible_for_support;
            }
          }
          ok = ok && operative_representative &&
               (opposition || fiber.independent_evidence_family_count == 1);
        }
        std::cout << (ok ? "PASS" : "FAIL") << " lifecycle=" << state
                  << " opposition=" << opposition << " reverse=" << reverse << '\n';
        if (!ok) ++failures;
      }
    }
  }
  std::cout << "failed=" << failures << " total=16\n";
  return failures ? 1 : 0;
}
