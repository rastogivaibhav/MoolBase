  while (pos < headers.size()) {
    size_t end = headers.find("\r\n", pos);
    if (end == std::string::npos) end = headers.size();
    const std::string line = headers.substr(pos, end - pos);
    const auto colon = line.find(':');
    if (colon != std::string::npos) {
      std::string key = lower_copy(trim_copy(line.substr(0, colon)));
      if (key == wanted) values.push_back(trim_copy(line.substr(colon + 1)));
    }
    pos = end + 2;
  }
  return values;
}

struct ContentLengthResult {
  bool present{false};
  bool valid{true};
  size_t value{0};
};

static ContentLengthResult parse_content_length(const std::string& headers) {
  ContentLengthResult result;
  const auto values = header_values(headers, "content-length");
  if (values.empty()) return result;
  result.present = true;
  std::optional<size_t> parsed;
  for (const auto& raw : values) {
    if (raw.empty() || !std::all_of(raw.begin(), raw.end(), [](unsigned char c){ return std::isdigit(c); })) {
      result.valid = false;
      return result;
    }
    try {
      const auto value = static_cast<size_t>(std::stoull(raw));
      if (parsed && *parsed != value) {
        result.valid = false;
        return result;
      }
      parsed = value;
    } catch (...) {
      result.valid = false;
      return result;
    }
  }
  result.value = parsed.value_or(0);
  return result;
}

static bool endpoint_requires_json_body(const std::string& method, const std::string& path) {
  if (method != "POST") return false;
  return path == "/v1/nodes" || path == "/v1/nodes/bulk" || path == "/v1/facts" ||
         path == "/v1/extractions" ||
         path == "/v1/edges" || path == "/v1/edges/provenance" ||
         path == "/v1/retrieve/bundle" || path == "/v1/retrieve/explain" ||
         path == "/v1/retrieve/temporal" || path == "/v1/search/hybrid" ||
         path == "/v1/reason/dialectic" || path == "/v1/reason/hypokosh" ||
         path == "/v1/reason/runtime" ||
         path == "/v1/learning/episodes" ||
         path == "/v1/learning/policies/evaluate" ||
         path == "/v1/learning/policies/decisions" ||
         path == "/v1/learning/episodes/quarantine" ||
         path == "/v1/admin/backup";
}

static bool application_json_content_type(const std::string& headers) {
  std::string value = lower_copy(header_value(headers, "content-type"));
  const auto semicolon = value.find(';');
  if (semicolon != std::string::npos) value.resize(semicolon);
  return trim_copy(value) == "application/json";
}

static bool constant_time_equal(const std::string& a, const std::string& b) {
  const size_t n = std::max(a.size(), b.size());
  unsigned char diff = static_cast<unsigned char>(a.size() ^ b.size());
  for (size_t i = 0; i < n; ++i) {
    const unsigned char av = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
    const unsigned char bv = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
    diff = static_cast<unsigned char>(diff | (av ^ bv));
  }
  return diff == 0;
}

static bool authorized_request(const std::string& headers, const std::string& api_key) {
  if (api_key.empty()) return true;
  const std::string direct = header_value(headers, "x-api-key");
  if (!direct.empty() && constant_time_equal(direct, api_key)) return true;
  const std::string authorization = header_value(headers, "authorization");
  constexpr const char* prefix = "Bearer ";
  if (authorization.rfind(prefix, 0) == 0) {
    return constant_time_equal(authorization.substr(std::strlen(prefix)), api_key);
  }
  return false;
}

static std::string first_forwarded_ip(const std::string& headers) {
  std::string value = header_value(headers, "x-forwarded-for");
  auto comma = value.find(',');
  if (comma != std::string::npos) value.resize(comma);
  return trim_copy(value);
}

static uint32_t query_u32(const std::string& path, const std::string& key, uint32_t def = 0) {
  auto p = path.find(key + "=");
  if (p == std::string::npos) return def;
  p += key.size() + 1;
  size_t e = p;
  while (e < path.size() && std::isdigit(static_cast<unsigned char>(path[e]))) ++e;
  try {
    const auto parsed = std::stoull(path.substr(p, e - p));
    if (parsed > UINT32_MAX) return def;
    return static_cast<uint32_t>(parsed);
  } catch (...) { return def; }
}

static std::string query_string(const std::string& path,
                                const std::string& key) {
  const auto question = path.find('?');
  if (question == std::string::npos) return {};
  size_t position = question + 1;
  while (position < path.size()) {
    const size_t ampersand = path.find('&', position);
    const size_t end =
        ampersand == std::string::npos ? path.size() : ampersand;
    const size_t equals = path.find('=', position);
    if (equals != std::string::npos && equals < end &&
        path.substr(position, equals - position) == key) {
      std::string output;
      const std::string encoded = path.substr(equals + 1, end - equals - 1);
      output.reserve(encoded.size());
      for (size_t index = 0; index < encoded.size(); ++index) {
        if (encoded[index] == '+') {
          output.push_back(' ');
        } else if (encoded[index] == '%' && index + 2 < encoded.size() &&
                   std::isxdigit(static_cast<unsigned char>(encoded[index + 1])) &&
                   std::isxdigit(static_cast<unsigned char>(encoded[index + 2]))) {
          const auto hex_value = [](char value) -> unsigned char {
            if (value >= '0' && value <= '9') return value - '0';
            value = static_cast<char>(std::tolower(
                static_cast<unsigned char>(value)));
            return static_cast<unsigned char>(10 + value - 'a');
          };
          output.push_back(static_cast<char>(
              (hex_value(encoded[index + 1]) << 4) |
              hex_value(encoded[index + 2])));
          index += 2;
        } else {
          output.push_back(encoded[index]);
        }
      }
      return output;
    }
