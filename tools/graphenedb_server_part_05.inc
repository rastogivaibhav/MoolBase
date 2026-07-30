}

static void append_dialectic_opposition(std::ostringstream& output,
                                        const OppositionReport& opposition) {
  output << "{\"opposition_score\":" << opposition.opposition_score
         << ",\"requests_reexpansion\":"
         << (opposition.requests_reexpansion ? "true" : "false")
         << ",\"challenged_claims\":";
  append_string_array(output, opposition.challenged_claims);
  output << ",\"falsification_questions\":";
  append_string_array(output, opposition.falsification_questions);
  output << ",\"reopen_nodes\":";
  append_u32_array(output, opposition.reopen_nodes);
  output << '}';
}

static std::string dialectic_json(const DialecticResult& result) {
  std::ostringstream output;
  output << "{\"initial_bundle\":";
  append_dialectic_bundle(output, result.initial_bundle);
  output << ",\"initial_convergence\":";
  append_dialectic_convergence(output, result.initial_convergence);
  output << ",\"initial_opposition\":";
  append_dialectic_opposition(output, result.initial_opposition);
  output << ",\"has_reopened_bundle\":"
         << (result.has_reopened_bundle ? "true" : "false");
  if (result.has_reopened_bundle) {
    output << ",\"reopened_bundle\":";
    append_dialectic_bundle(output, result.reopened_bundle);
  }
  output << ",\"final_convergence\":";
  append_dialectic_convergence(output, result.final_convergence);
  output << ",\"final_opposition\":";
  append_dialectic_opposition(output, result.final_opposition);
  output << ",\"synthesis\":{\"has_answer\":"
         << (result.synthesis.has_answer ? "true" : "false")
         << ",\"primary_node\":" << result.synthesis.primary_node
         << ",\"confidence\":" << result.synthesis.confidence
         << ",\"epistemic_status\":\""
         << json_escape(result.synthesis.epistemic_status)
         << "\",\"evidence_edges\":";
  append_u32_array(output, result.synthesis.evidence_edges);
  output << ",\"residual_uncertainty\":";
  append_string_array(output, result.synthesis.residual_uncertainty);
  output << "},\"rounds\":" << result.rounds << ",\"durable_writes\":"
         << (result.durable_writes ? "true" : "false") << '}';
  return output.str();
}

static LatticeCoord spiral_coord(uint32_t id) {
  return graphene::hex_spiral_coord(id, 0);
}

static double cosine_vec(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.empty() || b.empty() || a.size() != b.size()) return 0.0;
  double dot = 0.0, na = 0.0, nb = 0.0;
  for (size_t i = 0; i < a.size(); ++i) { dot += a[i] * b[i]; na += a[i] * a[i]; nb += b[i] * b[i]; }
  if (na <= 0.0 || nb <= 0.0) return 0.0;
  return dot / (std::sqrt(na) * std::sqrt(nb));
}

static int hex_distance(const LatticeCoord& a, const LatticeCoord& b) {
  if (a.layer != b.layer) return 1000000 + std::abs(a.layer - b.layer);
  int32_t as = -a.q - a.r;
  int32_t bs = -b.q - b.r;
  return std::max({std::abs(a.q - b.q), std::abs(a.r - b.r), std::abs(as - bs)});
}

static std::string coord_key(const LatticeCoord& c) {
  return std::to_string(c.q) + ":" + std::to_string(c.r) + ":" + std::to_string(c.layer);
}

static std::vector<LatticeCoord> coord_ring_candidates(const LatticeCoord& c, int radius) {
  std::vector<LatticeCoord> out;
  out.push_back(c);
  for (int q = -radius; q <= radius; ++q) {
    for (int r = -radius; r <= radius; ++r) {
      LatticeCoord x{c.q + q, c.r + r, c.layer};
      if (hex_distance(c, x) <= radius) out.push_back(x);
    }
  }
  return out;
}

static LatticeCoord choose_semantic_causal_temporal_cell(GrapheneDB& db,
                                                        const std::string& content,
                                                        const std::vector<float>& vector,
                                                        const std::map<std::string,std::string>& metadata,
                                                        std::atomic<uint32_t>& next_lattice_slot) {
  (void)content;
  std::unordered_set<uint32_t> candidate_ids;
  for (const auto& hit : db.vector_search(vector, 32)) candidate_ids.insert(hit.node_id);
  for (const auto& key : {"account", "service", "incident", "repo", "source"}) {
    auto it = metadata.find(key);
    if (it == metadata.end()) continue;
    auto matches = db.metadata_search(key, it->second);
    for (size_t i = 0; i < matches.size() && i < 16; ++i) candidate_ids.insert(matches[i]);
  }

  std::unordered_set<std::string> occupied_near_anchors;
  struct Anchor { uint32_t id; LatticeCoord coord; double semantic; double meta; double temporal; };
  std::vector<Anchor> anchors;
  anchors.reserve(candidate_ids.size());
  for (uint32_t id : candidate_ids) {
    auto n = db.get_node(id);
    if (!n || !n->lattice) continue;
    occupied_near_anchors.insert(coord_key(*n->lattice));
    for (uint32_t neighbor : db.lattice_neighbors(id, 3)) {
      auto nn = db.get_node(neighbor);
      if (nn && nn->lattice) occupied_near_anchors.insert(coord_key(*nn->lattice));
    }
    const double sem = cosine_vec(vector, n->vector);
    double meta = 0.0;
    size_t comparable = 0;
    for (const auto& kv : metadata) {
      if (kv.first == "placement_policy" || kv.first == "valid_from" || kv.first == "valid_until") continue;
      auto found = n->metadata.find(kv.first);
      if (found != n->metadata.end()) {
        ++comparable;
        if (found->second == kv.second) meta += 1.0;
      }
    }
    if (comparable) meta /= static_cast<double>(comparable);
    double temporal = 0.0;
    auto vf = metadata.find("valid_from"); auto vu = metadata.find("valid_until");
    auto of = n->metadata.find("valid_from"); auto ou = n->metadata.find("valid_until");
    if (vf != metadata.end() && of != n->metadata.end() && vf->second.size() >= 7 && of->second.size() >= 7)
      temporal += vf->second.substr(0,7) == of->second.substr(0,7) ? 0.6 : 0.0;
    if (vu != metadata.end() && ou != n->metadata.end() && vu->second.size() >= 7 && ou->second.size() >= 7)
      temporal += vu->second.substr(0,7) == ou->second.substr(0,7) ? 0.4 : 0.0;
    const double anchor_score = 0.65 * sem + 0.25 * meta + 0.10 * temporal;
    if (anchor_score > 0.05) anchors.push_back({id, *n->lattice, sem, meta, temporal});
  }
  std::sort(anchors.begin(), anchors.end(), [](const Anchor& a, const Anchor& b){
    return (0.65*a.semantic + 0.25*a.meta + 0.10*a.temporal) > (0.65*b.semantic + 0.25*b.meta + 0.10*b.temporal);
  });
  if (anchors.size() > 24) anchors.resize(24);

  struct Candidate { LatticeCoord coord; double score; };
  Candidate best{{0,0,0}, -1e9};
  for (const auto& a : anchors) {
    for (const auto& c : coord_ring_candidates(a.coord, 3)) {
      if (occupied_near_anchors.count(coord_key(c))) continue;
      double local_score = 0.0;
      int local_count = 0;
      for (const auto& b : anchors) {
        const int d = hex_distance(c, b.coord);
        if (d > 4) continue;
        const double decay = 1.0 / (1.0 + d);
        local_score += decay * (0.45 * b.semantic + 0.25 * b.meta + 0.15 * b.temporal);