#include "graphene/hypokosh_runtime.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace graphene;
namespace fs = std::filesystem;

namespace {
void expect(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
void require(Status status) {
  if (!status) throw std::runtime_error(status.message);
}
struct Certificate : PathVerifier {
  PathVerificationResult verify(const DialecticPath& path,
                               const PathVerificationContext&) const override {
    PathVerificationResult result;
    if (!path.contains_contradiction)
      result.semantic_verification = SemanticVerificationStatus::Verified;
    return result;
  }
};

void scenario(const fs::path& dir, bool oppose_selected, bool reverse,
              bool support_selected, bool dwm, bool research) {
  GrapheneDB db;
  DBOptions config; config.dimension = 3;
  require(db.open(dir.string(), config));
  const auto signature = signature_for(21, 34);
  auto put = [&](const char* text, std::vector<float> vector, bool root) {
    NodeInput n; n.content = text; n.vector = std::move(vector);
    n.signature = signature; n.root = root; n.incident = 7001;
    uint32_t id; require(db.put_node(n, &id)); return id;
  };
  put("sentinel", {0,-1,0}, false);
  const auto a = put("Alternative", {1,0,0}, true);
  const auto b = put("Supported answer", {-1,0,0}, true);
  const auto support = put("Supporting observation", {0,1,0}, false);
  const auto opposition = put("Opposing observation", {0,1,0}, false);
  auto add_support = [&] {
    if (!support_selected) return;
    require(db.put_edge({b, support, EdgeOrigin::Observed, EdgeRole::Supports, .9,
      {{"source_id","support"},{"evidence_family_id","shared-family"},
       {"semantic_verification","verified"}}}));
  };
  auto add_opposition = [&] {
    require(db.put_edge({oppose_selected ? b : a, opposition,
      EdgeOrigin::Observed, EdgeRole::Contradicts, .95,
      {{"source_id","opposition"},{"evidence_family_id","shared-family"},
       {"material","true"}}}));
  };
  if (reverse) { add_opposition(); add_support(); }
  else { add_support(); add_opposition(); }

  Certificate verifier;
  RuntimeOptions options;
  options.path_verifier = &verifier;
  options.enable_dwm = dwm;
  options.enable_opposition_research = research;
  options.update_model_world = false;
  options.dialectic.mode = QueryMode::Empirical;
  options.dialectic.semantic_candidates = 1024;
  options.dialectic.max_hops = 2;
  options.dialectic.max_paths = 1024;
  options.dialectic.max_paths_per_root = 512;
  options.dialectic.max_visited_states = 32768;
  options.dialectic.minimum_confidence = .10;
  options.dialectic.reexpansion_threshold = .25;
  options.max_recursive_cycles = 2;
  const auto result = CompleteHypoKoshRuntime(db).reason({0,1,0}, signature, options);
  if (support_selected && !oppose_selected) {
    expect(result.final_convergence.has_answer, "supported answer must remain selected");
    expect(result.final_convergence.primary_node == b, "wrong selected target");
    expect(result.status == GovernedEpistemicStatus::ProvisionallyResolved,
           "opposition to another target must not contest the selected answer");
  }
  if (support_selected && oppose_selected) {
    expect(result.status == GovernedEpistemicStatus::Contested,
           "material opposition to the selected candidate must remain contested");
  }
  if (!support_selected) {
    expect(!result.final_convergence.has_answer, "opposition alone must not create an answer");
  }
  bool challenge = false;
  for (const auto& e : result.receipt.epistemic_events) {
    if (e.type != EpistemicEventType::Challenge) continue;
    challenge = true;
    const auto target = oppose_selected ? b : a;
    expect(e.hypothesis_node == target, "challenge must identify the opposed target");
    expect(e.reason == "material opposition challenges target " + std::to_string(target),
           "challenge reason and target must agree");
    expect(!e.evidence_edges.empty(), "challenge evidence must be retained");
  }
  expect(challenge == dwm, "DWM challenge capability or alternative exploration lost");
  require(db.close());
}
}

int main() {
  const auto root = fs::temp_directory_path() /
      ("moolbase-opposition-scope-" + std::to_string(
       std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    unsigned n = 0;
    for (bool reverse : {false, true}) {
      for (bool research : {false, true}) {
        scenario(root / std::to_string(n++), false, reverse, true, true, research);
        scenario(root / std::to_string(n++), true, reverse, true, true, research);
        scenario(root / std::to_string(n++), false, reverse, false, true, research);
        scenario(root / std::to_string(n++), false, reverse, true, false, research);
      }
    }
    fs::remove_all(root);
    std::cout << "PASS: 16 opposition-scope cases\n";
    return 0;
  } catch (const std::exception& e) {
    fs::remove_all(root);
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
