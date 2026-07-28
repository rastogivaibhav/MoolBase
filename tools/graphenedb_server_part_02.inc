      if (!output->object.emplace(std::move(key), std::move(value)).second) {
        return fail("duplicate object key");
      }
      skip_space();
      if (consume('}')) return true;
      if (!consume(',')) return fail("expected ',' or '}' in object");
      skip_space();
    }
  }

  bool parse_value(JsonDocumentValue* output, size_t depth) {
    if (!output) return fail("missing JSON output");
    if (depth > kMaxDepth) return fail("JSON nesting is too deep");
    skip_space();
    if (position_ >= input_.size()) return fail("unexpected end of JSON");
    const char c = input_[position_];
    if (c == '{') return parse_object(output, depth);
    if (c == '[') return parse_array(output, depth);
    if (c == '"') {
      output->kind = JsonDocumentValue::Kind::String;
      return parse_string(&output->scalar);
    }
    if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
      return parse_number(output);
    }
    if (input_.compare(position_, 4, "true") == 0) {
      position_ += 4;
      output->kind = JsonDocumentValue::Kind::Boolean;
      output->boolean = true;
      return true;
    }
    if (input_.compare(position_, 5, "false") == 0) {
      position_ += 5;
      output->kind = JsonDocumentValue::Kind::Boolean;
      output->boolean = false;
      return true;
    }
    if (input_.compare(position_, 4, "null") == 0) {
      position_ += 4;
      output->kind = JsonDocumentValue::Kind::Null;
      return true;
    }
    return fail("invalid JSON value");
  }
};

static std::string json_value(const std::string& body, const std::string& key) {
  std::string needle = "\"" + key + "\"";
  size_t key_pos = std::string::npos;
  size_t search_from = 0;
  while (true) {
    auto p0 = body.find(needle, search_from);
    if (p0 == std::string::npos) return {};
    size_t p = p0 + needle.size();
    while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) ++p;
    if (p < body.size() && body[p] == ':') { key_pos = p; break; }
    search_from = p0 + 1;
  }
  size_t p = key_pos + 1;
  while (p < body.size() && std::isspace(static_cast<unsigned char>(body[p]))) ++p;
  if (p >= body.size()) return {};
  if (body[p] == '"') {
    ++p; std::string out;
    while (p < body.size() && body[p] != '"') {
      if (body[p] == '\\' && p + 1 < body.size()) { out.push_back(body[p + 1]); p += 2; }
      else out.push_back(body[p++]);
    }
    return out;
  }
  size_t e = p;
  while (e < body.size() && body[e] != ',' && body[e] != '}' && body[e] != ']') ++e;
  std::string out = body.substr(p, e - p);
  while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
  return out;
}

static bool parse_vector_index_kind(const std::string& value, VectorIndexKind* out) {
  if (!out) return false;
  if (value == "auto") *out = VectorIndexKind::Auto;
  else if (value == "flat") *out = VectorIndexKind::Flat;
  else if (value == "kdtree") *out = VectorIndexKind::KDTree;
  else if (value == "faiss") *out = VectorIndexKind::Faiss;
  else return false;
  return true;
}


static uint32_t json_u32(const std::string& body, const std::string& key, uint32_t def = 0) {
  try {
    std::string v = json_value(body, key);
    if (v.empty()) return def;
    const auto parsed = std::stoull(v);
    if (parsed > UINT32_MAX) return def;
    return static_cast<uint32_t>(parsed);
  } catch (...) { return def; }
}

static uint64_t json_u64(const std::string& body,
                         const std::string& key,
                         uint64_t def = 0) {
  try {
    const std::string value = json_value(body, key);
    if (value.empty()) return def;
    size_t consumed = 0;
    const uint64_t parsed = std::stoull(value, &consumed);
    return consumed == value.size() ? parsed : def;
  } catch (...) {
    return def;
  }
}

static double json_double(const std::string& body, const std::string& key, double def = 0.0) {
  try {
    std::string v = json_value(body, key);
    if (v.empty()) return def;
    return std::stod(v);
  } catch (...) { return def; }
}

static std::string trim_copy(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
  return value;
}

static std::string lower_copy(std::string v);

static std::string header_value(const std::string& headers, const std::string& name) {
  std::string wanted = name;
  std::transform(wanted.begin(), wanted.end(), wanted.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
  size_t pos = 0;
  while (pos < headers.size()) {
    size_t end = headers.find("\r\n", pos);
    if (end == std::string::npos) end = headers.size();
    std::string line = headers.substr(pos, end - pos);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
      std::string key = line.substr(0, colon);
      std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
      if (key == wanted) return trim_copy(line.substr(colon + 1));
    }
    pos = end + 2;
  }
  return {};
}

static std::vector<std::string> header_values(const std::string& headers, const std::string& name) {
  std::vector<std::string> values;
  std::string wanted = lower_copy(name);
  size_t pos = 0;
