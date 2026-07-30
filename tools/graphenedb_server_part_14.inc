      }
      if (relation.bond_strength < 0.0 || relation.bond_strength > 1.0) {
        *error = context + ": bond_strength must be between 0 and 1";
        return false;
      }
      input.relations.push_back(std::move(relation));
    }
  }
  *output = std::move(input);
  return true;
}

static std::string extraction_result_json(const ExtractionResult& result,
                                          bool idempotent) {
  std::vector<std::pair<std::string, uint32_t>> mapping(
      result.external_to_node_id.begin(), result.external_to_node_id.end());
  std::sort(mapping.begin(), mapping.end(),
            [](const auto& left, const auto& right) {
              return left.first < right.first;
            });
  auto ids_json = [](const auto& ids) {
    std::ostringstream array;
    array << '[';
    for (size_t index = 0; index < ids.size(); ++index) {
      if (index) array << ',';
      array << ids[index];
    }
    array << ']';
    return array.str();
  };
  std::ostringstream json;
  json << "{\"atomic\":true,\"idempotent\":"
       << (idempotent ? "true" : "false")
       << ",\"idempotent_replay\":"
       << (result.inserted_node_ids.empty() &&
                   result.inserted_edge_ids.empty()
               ? "true"
               : "false")
       << ",\"inserted_node_ids\":" << ids_json(result.inserted_node_ids)
       << ",\"existing_node_ids\":" << ids_json(result.existing_node_ids)
       << ",\"inserted_edge_ids\":" << ids_json(result.inserted_edge_ids)
       << ",\"external_to_node_id\":{";
  for (size_t index = 0; index < mapping.size(); ++index) {
    if (index) json << ',';
    json << '"' << json_escape(mapping[index].first) << "\":"
         << mapping[index].second;
  }
  json << "}}";
  return json.str();
}

static bool edge_allowed(EdgeOrigin o, const std::string& mode) {
  std::string m = lower_copy(mode);
  if (m == "empirical") return o == EdgeOrigin::Observed || o == EdgeOrigin::Discovered;
  if (m == "exploratory" || m == "theoretical") return true;
  // balanced: allow inferred/reinforced with labels, reject hypothetical by default.
  return o != EdgeOrigin::Hypothetical;
}

static std::string metadata_json(const std::map<std::string, std::string>& md) {
  std::ostringstream js; js << "{";
  bool first = true;
  for (const auto& kv : md) {
    if (!first) js << ',';
    first = false;
    js << "\"" << json_escape(kv.first) << "\":\"" << json_escape(kv.second) << "\"";
  }
  js << "}";
  return js.str();
}


static bool metadata_valid_at(
    const std::map<std::string, std::string>& metadata,
    const std::string& as_of) {
  TemporalValidity validity;
  if (!parse_temporal_validity(metadata, &validity)) return false;
  if (as_of.empty()) return true;
  Rfc3339Instant instant;
  if (!parse_rfc3339(as_of, &instant)) return false;
  return valid_at(validity, instant);
}

static bool node_valid_at(const Node& n, const std::string& as_of) {
  return metadata_valid_at(n.metadata, as_of);
}

static bool edge_valid_at(GrapheneDB& db, const Edge& e, const std::string& as_of) {
  auto from = db.get_node(e.from);
  auto to = db.get_node(e.to);
  if (from && !node_valid_at(*from, as_of)) return false;
  if (to && !node_valid_at(*to, as_of)) return false;
  return metadata_valid_at(e.metadata, as_of);
}

static std::string build_bundle_json(GrapheneDB& db, uint32_t anchor, uint32_t max_hops, const std::string& mode, const std::string& as_of = "") {
  struct State { uint32_t node; std::vector<uint32_t> nodes; std::vector<uint32_t> edges; double confidence; bool contradiction; bool hypothetical; };
  std::queue<State> q;
  std::unordered_map<uint32_t, std::vector<State>> by_target;
  std::set<std::pair<uint32_t,uint32_t>> seen_at_depth;
  auto anchor_node = db.get_node(anchor);
  if (!anchor_node || !node_valid_at(*anchor_node, as_of)) {
    return "{\"anchor\":" + std::to_string(anchor) + ",\"mode\":\"" + json_escape(mode.empty()?"balanced":mode) + "\",\"as_of\":\"" + json_escape(as_of) + "\",\"targets\":[],\"warnings\":[\"anchor_not_found_or_not_temporally_valid\"]}";
  }
  q.push({anchor, {anchor}, {}, 1.0, false, false});
  seen_at_depth.insert({anchor, 0});
  const size_t edge_total = db.edge_count();
  while (!q.empty()) {
    State cur = q.front(); q.pop();
    uint32_t depth = static_cast<uint32_t>(cur.edges.size());
    if (depth >= max_hops) continue;
    for (uint32_t eid = 0; eid < edge_total + 1000; ++eid) {
      auto e = db.get_edge(eid);
      if (!e) {
        if (eid > edge_total + 16) break;
        continue;
      }
      if (e->from != cur.node) continue;
      if (!edge_allowed(e->origin, mode)) continue;
      if (!edge_valid_at(db, *e, as_of)) continue;
      uint32_t next = e->to;
      if (std::find(cur.nodes.begin(), cur.nodes.end(), next) != cur.nodes.end()) continue;
      auto next_node = db.get_node(next);
      if (next_node && !node_valid_at(*next_node, as_of)) continue;
      State ns = cur;
      ns.node = next;
      ns.nodes.push_back(next);
      ns.edges.push_back(eid);
      ns.confidence *= e->confidence;
      ns.contradiction = ns.contradiction || e->role == EdgeRole::Contradicts;
      ns.hypothetical = ns.hypothetical || e->origin == EdgeOrigin::Hypothetical;
      by_target[next].push_back(ns);
      if (seen_at_depth.insert({next, depth + 1}).second) q.push(ns);
    }
  }

  std::ostringstream js;
  js << "{\"anchor\":" << anchor << ",\"mode\":\"" << json_escape(mode.empty()?"balanced":mode) << "\",\"as_of\":\"" << json_escape(as_of) << "\",\"max_hops\":" << max_hops << ",\"targets\":[";
  bool first_target = true;
  for (auto& kv : by_target) {
    if (!first_target) js << ',';
    first_target = false;
    bool has_contra = false; double best = 0.0;
    for (const auto& st : kv.second) { has_contra = has_contra || st.contradiction; best = std::max(best, st.confidence); }
    std::vector<Edge> attached_contradictions;
    std::set<uint32_t> bundle_nodes;
    for (const auto& st : kv.second) for (uint32_t n : st.nodes) bundle_nodes.insert(n);
    std::set<uint32_t> attached_ids;
    for (uint32_t eid = 0; eid < edge_total + 1000; ++eid) {
      auto e = db.get_edge(eid);