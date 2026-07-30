        ++local_count;
      }
      const double overcrowding = std::max(0, local_count - 6) * 0.025;
      const double score = local_score - overcrowding;
      if (score > best.score) best = {c, score};
    }
  }
  if (best.score > 0.02) return best.coord;
  return spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
}


static Status put_node_with_lattice_retry(GrapheneDB& db, NodeInput& ni, uint32_t* out_id, std::atomic<uint32_t>& next_lattice_slot) {
  // Semantic-causal placement is computed from a snapshot of occupied cells. Under concurrent HTTP writes,
  // two requests can choose the same best empty cell. Keep the mathematical placement attempt, but preserve
  // storage correctness by retrying with deterministic frontier cells on coordinate collision.
  Status st = db.put_node(ni, out_id);
  if (st) return st;
  const std::string first_error = st.message;
  for (uint32_t tries = 0; tries < 1024; ++tries) {
    ni.lattice = spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
    st = db.put_node(ni, out_id);
    if (st) return st;
    if (st.message.find("coordinate") == std::string::npos && st.message.find("lattice") == std::string::npos && st.message.find("duplicate") == std::string::npos) {
      return st;
    }
  }
  return Status::error(ErrorCode::InvalidInput, "lattice placement retry exhausted after initial error: " + first_error);
}

static std::string lattice_quality_json(GrapheneDB& db) {
  const size_t total = db.node_count();
  size_t checked = 0, neighbor_pairs = 0;
  double sem_sum = 0.0, causal_local = 0.0, causal_total = 0.0, temporal_pairs = 0.0, temporal_aligned = 0.0;
  for (uint32_t id = 0; id < total + 1024; ++id) {
    auto n = db.get_node(id);
    if (!n) { if (id > total + 16) break; continue; }
    ++checked;
    auto ns = db.lattice_neighbors(id, 1);
    for (uint32_t nid : ns) {
      if (nid <= id) continue;
      auto m = db.get_node(nid); if (!m) continue;
      sem_sum += cosine_vec(n->vector, m->vector);
      neighbor_pairs++;
      auto nf = n->metadata.find("valid_from"); auto mf = m->metadata.find("valid_from");
      if (nf != n->metadata.end() && mf != m->metadata.end()) { temporal_pairs += 1.0; if (nf->second.substr(0,7) == mf->second.substr(0,7)) temporal_aligned += 1.0; }
    }
  }
  const size_t edge_total = db.edge_count();
  for (uint32_t eid = 0; eid < edge_total + 1024; ++eid) {
    auto e = db.get_edge(eid);
    if (!e) { if (eid > edge_total + 16) break; continue; }
    auto a = db.get_node(e->from); auto b = db.get_node(e->to);
    if (!a || !b || !a->lattice || !b->lattice) continue;
    causal_total += 1.0;
    if (hex_distance(*a->lattice, *b->lattice) <= 2) causal_local += 1.0;
  }
  double local_coherence = neighbor_pairs ? sem_sum / neighbor_pairs : 0.0;
  double causal_locality = causal_total ? causal_local / causal_total : 0.0;
  double temporal_consistency = temporal_pairs ? temporal_aligned / temporal_pairs : 0.0;
  std::ostringstream js;
  js << "{\"node_count\":" << checked
     << ",\"neighbor_pairs\":" << neighbor_pairs
     << ",\"local_coherence\":" << local_coherence
     << ",\"causal_locality\":" << causal_locality
     << ",\"temporal_consistency\":" << temporal_consistency
     << ",\"mathematical_claim\":\"hex distance is optimized as a proxy for semantic-causal-temporal locality\"}";
  return js.str();
}

static std::string lower_copy(std::string v) {
  std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  return v;
}

static EdgeOrigin parse_origin(const std::string& raw) {
  std::string v = lower_copy(raw);
  if (v == "discovered") return EdgeOrigin::Discovered;
  if (v == "inferred") return EdgeOrigin::Inferred;
  if (v == "reinforced") return EdgeOrigin::Reinforced;
  if (v == "hypothetical") return EdgeOrigin::Hypothetical;
  return EdgeOrigin::Observed;
}

static EdgeRole parse_role(const std::string& raw) {
  std::string v = lower_copy(raw);
  if (v == "mechanistic") return EdgeRole::Mechanistic;
  if (v == "compressed") return EdgeRole::Compressed;
  if (v == "analogical") return EdgeRole::Analogical;
  if (v == "predictive") return EdgeRole::Predictive;
  if (v == "contradicts" || v == "contradiction") return EdgeRole::Contradicts;
  if (v == "supports" || v == "support") return EdgeRole::Supports;
  if (v == "supersedes" || v == "supersession") return EdgeRole::Supersedes;
  return EdgeRole::Causal;
}

static std::string origin_name(EdgeOrigin o) {
  switch (o) {
    case EdgeOrigin::Observed: return "observed";
    case EdgeOrigin::Discovered: return "discovered";
    case EdgeOrigin::Inferred: return "inferred";
    case EdgeOrigin::Reinforced: return "reinforced";
    case EdgeOrigin::Hypothetical: return "hypothetical";
  }
  return "observed";
}

static std::string role_name(EdgeRole r) {
  switch (r) {
    case EdgeRole::Mechanistic: return "mechanistic";
    case EdgeRole::Compressed: return "compressed";
    case EdgeRole::Analogical: return "analogical";
    case EdgeRole::Predictive: return "predictive";
    case EdgeRole::Causal: return "causal";
    case EdgeRole::Contradicts: return "contradicts";
    case EdgeRole::Supports: return "supports";
    case EdgeRole::Supersedes: return "supersedes";
  }
  return "causal";
}

static const JsonDocumentValue* json_member(const JsonDocumentValue& object,
                                            const std::string& key) {
  if (object.kind != JsonDocumentValue::Kind::Object) return nullptr;
  const auto it = object.object.find(key);
  return it == object.object.end() ? nullptr : &it->second;
}

static bool reject_unknown_json_fields(
    const JsonDocumentValue& object,
    std::initializer_list<const char*> allowed,
    const std::string& context,
    std::string* error) {
  std::unordered_set<std::string> names;
  names.reserve(allowed.size());
  for (const char* name : allowed) names.emplace(name);
  for (const auto& [name, unused] : object.object) {
    (void)unused;
    if (names.count(name) == 0) {
      *error = context + " contains unsupported field '" + name + "'";
      return false;
    }
  }
  return true;
}

static bool extraction_string_field(const JsonDocumentValue& object,
                                    const std::string& key,
                                    size_t maximum_bytes,
                                    bool required,