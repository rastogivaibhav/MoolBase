#include "graphene/db.hpp"
#include "graphene/dialectic.hpp"
#include "graphene/hypokosh.hpp"
#include "graphene/lattice_placement.hpp"
#include "graphene/learning.hpp"
#include "server_runtime.hpp"
#include <arpa/inet.h>
#include <charconv>
#include <cctype>
#include <cmath>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <vector>
#include <iomanip>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <limits>
#include <set>
#include <queue>
#include <unordered_set>

using namespace graphene;
namespace fs = std::filesystem;

static constexpr const char* kServerVersion = "0.6.0-rc1";
static constexpr uint32_t kApiVersion = 1;
static constexpr size_t kMaxNodeTextBytes = 1024 * 1024;
static constexpr size_t kMaxBulkPrefixBytes = 4096;
static constexpr uint32_t kMaxLatticeHops = 16;
static constexpr uint32_t kMaxBundleHops = 8;
static constexpr size_t kMaxSearchTopK = 1000;
static constexpr size_t kMaxTemporalResults = 1000;
static constexpr uint32_t kMaxDialecticHops = 16;
static constexpr uint32_t kMaxDialecticPaths = 256;
static constexpr uint32_t kMaxDialecticVisitedStates = 200000;
static constexpr size_t kMaxExtractionRelations = 50000;
static constexpr size_t kMaxExtractionMetadataEntries = 128;
static constexpr size_t kMaxExtractionIdentifierBytes = 4096;
static constexpr size_t kMaxExtractionMetadataKeyBytes = 1024;
static constexpr size_t kMaxExtractionMetadataValueBytes = 64 * 1024;

static volatile std::sig_atomic_t g_stop = 0;
static volatile std::sig_atomic_t g_listen_fd = -1;
static std::mutex g_structured_log_mutex;

static void emit_structured_log(const std::string& line) {
  std::lock_guard<std::mutex> lock(g_structured_log_mutex);
  std::cerr << line << "\n";
}
static void on_sig(int) {
  g_stop = 1;
  // close(2) is async-signal-safe and wakes a blocking accept(2). This makes
  // SIGTERM predictable under systemd/Kubernetes rather than waiting for a
  // new connection to arrive before shutdown can begin.
  const int fd = static_cast<int>(g_listen_fd);
  if (fd >= 0) {
    ::close(fd);
    g_listen_fd = -1;
  }
}

using ServerMetrics = graphenedb::server::RuntimeMetrics;
using ConnectionTask = graphenedb::server::ConnectionTask;

static std::string json_escape(const std::string& s) {
  std::ostringstream os;
  for (unsigned char c : s) {
    switch (c) {
      case '"': os << "\\\""; break;
      case '\\': os << "\\\\"; break;
      case '\b': os << "\\b"; break;
      case '\f': os << "\\f"; break;
      case '\n': os << "\\n"; break;
      case '\r': os << "\\r"; break;
      case '\t': os << "\\t"; break;
      default:
        if (c < 0x20) {
          os << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<unsigned int>(c) << std::dec << std::setfill(' ');
        } else {
          os << static_cast<char>(c);
        }
    }
  }
  return os.str();
}

struct JsonDocumentValue {
  enum class Kind { Null, Boolean, Number, String, Object, Array };
  Kind kind{Kind::Null};
  bool boolean{false};
  std::string scalar;
  std::map<std::string, JsonDocumentValue> object;
  std::vector<JsonDocumentValue> array;
};

class JsonDocumentParser {
 public:
  explicit JsonDocumentParser(const std::string& input) : input_(input) {}

  bool parse(JsonDocumentValue* output, std::string* error) {
    error_ = error;
    skip_space();
    if (!parse_value(output, 0)) return false;
    skip_space();
    if (position_ != input_.size()) return fail("unexpected trailing JSON data");
    return true;
  }

 private:
  static constexpr size_t kMaxDepth = 16;
  const std::string& input_;
  size_t position_{0};
  std::string* error_{nullptr};

  bool fail(const std::string& message) {
    if (error_) *error_ = message + " at byte " + std::to_string(position_);
    return false;
  }

  void skip_space() {
    while (position_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[position_]))) {
      ++position_;
    }
  }

  bool consume(char expected) {
    if (position_ >= input_.size() || input_[position_] != expected) return false;
    ++position_;
    return true;
  }

  static void append_utf8(uint32_t point, std::string* output) {
    if (point <= 0x7f) {
      output->push_back(static_cast<char>(point));
    } else if (point <= 0x7ff) {
      output->push_back(static_cast<char>(0xc0 | (point >> 6)));
      output->push_back(static_cast<char>(0x80 | (point & 0x3f)));
    } else if (point <= 0xffff) {
      output->push_back(static_cast<char>(0xe0 | (point >> 12)));
      output->push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3f)));
      output->push_back(static_cast<char>(0x80 | (point & 0x3f)));
    } else {
      output->push_back(static_cast<char>(0xf0 | (point >> 18)));
      output->push_back(static_cast<char>(0x80 | ((point >> 12) & 0x3f)));
      output->push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3f)));
      output->push_back(static_cast<char>(0x80 | (point & 0x3f)));
    }
  }

  bool parse_hex_quad(uint32_t* output) {
    if (position_ + 4 > input_.size()) return fail("truncated unicode escape");
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
      const unsigned char c = static_cast<unsigned char>(input_[position_++]);
      value <<= 4;
      if (c >= '0' && c <= '9') value |= c - '0';
      else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
      else return fail("invalid unicode escape");
    }
    *output = value;
    return true;
  }

  bool parse_string(std::string* output) {
    if (!consume('"')) return fail("expected JSON string");
    output->clear();
    while (position_ < input_.size()) {
      const unsigned char c = static_cast<unsigned char>(input_[position_++]);
      if (c == '"') return true;
      if (c < 0x20) return fail("unescaped control character in string");
      if (c != '\\') {
        output->push_back(static_cast<char>(c));
        continue;
      }
      if (position_ >= input_.size()) return fail("truncated string escape");
      const char escape = input_[position_++];
      switch (escape) {
        case '"': output->push_back('"'); break;
        case '\\': output->push_back('\\'); break;
        case '/': output->push_back('/'); break;
        case 'b': output->push_back('\b'); break;
        case 'f': output->push_back('\f'); break;
        case 'n': output->push_back('\n'); break;
        case 'r': output->push_back('\r'); break;
        case 't': output->push_back('\t'); break;
        case 'u': {
          uint32_t point = 0;
          if (!parse_hex_quad(&point)) return false;
          if (point >= 0xd800 && point <= 0xdbff) {
            if (position_ + 2 > input_.size() || input_[position_] != '\\' ||
                input_[position_ + 1] != 'u') {
              return fail("high surrogate without low surrogate");
            }
            position_ += 2;
            uint32_t low = 0;
            if (!parse_hex_quad(&low)) return false;
            if (low < 0xdc00 || low > 0xdfff) {
              return fail("invalid low surrogate");
            }
            point = 0x10000 + ((point - 0xd800) << 10) + (low - 0xdc00);
          } else if (point >= 0xdc00 && point <= 0xdfff) {
            return fail("low surrogate without high surrogate");
          }
          append_utf8(point, output);
          break;
        }
        default: return fail("invalid string escape");
      }
    }
    return fail("unterminated JSON string");
  }

  bool parse_number(JsonDocumentValue* output) {
    const size_t start = position_;
    if (position_ < input_.size() && input_[position_] == '-') ++position_;
    if (position_ >= input_.size()) return fail("invalid JSON number");
    if (input_[position_] == '0') {
      ++position_;
    } else if (input_[position_] >= '1' && input_[position_] <= '9') {
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) {
        ++position_;
      }
    } else {
      return fail("invalid JSON number");
    }
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_;
      const size_t fraction = position_;
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) {
        ++position_;
      }
      if (fraction == position_) return fail("invalid JSON fraction");
    }
    if (position_ < input_.size() &&
        (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_;
      if (position_ < input_.size() &&
          (input_[position_] == '+' || input_[position_] == '-')) {
        ++position_;
      }
      const size_t exponent = position_;
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) {
        ++position_;
      }
      if (exponent == position_) return fail("invalid JSON exponent");
    }
    output->kind = JsonDocumentValue::Kind::Number;
    output->scalar = input_.substr(start, position_ - start);
    return true;
  }

  bool parse_array(JsonDocumentValue* output, size_t depth) {
    consume('[');
    output->kind = JsonDocumentValue::Kind::Array;
    skip_space();
    if (consume(']')) return true;
    while (true) {
      JsonDocumentValue item;
      if (!parse_value(&item, depth + 1)) return false;
      output->array.push_back(std::move(item));
      skip_space();
      if (consume(']')) return true;
      if (!consume(',')) return fail("expected ',' or ']' in array");
      skip_space();
    }
  }

  bool parse_object(JsonDocumentValue* output, size_t depth) {
    consume('{');
    output->kind = JsonDocumentValue::Kind::Object;
    skip_space();
    if (consume('}')) return true;
    while (true) {
      std::string key;
      if (!parse_string(&key)) return false;
      skip_space();
      if (!consume(':')) return fail("expected ':' after object key");
      skip_space();
      JsonDocumentValue value;
      if (!parse_value(&value, depth + 1)) return false;
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
    position = end + 1;
  }
  return {};
}

static int http_status_for(const Status& status) {
  switch (status.code) {
    case ErrorCode::Ok: return 200;
    case ErrorCode::InvalidOption:
    case ErrorCode::DimensionMismatch:
    case ErrorCode::EdgeInvalid:
    case ErrorCode::InvalidInput: return 400;
    case ErrorCode::NodeNotFound: return 404;
    case ErrorCode::LockBusy: return 409;
    case ErrorCode::NotOpen: return 503;
    case ErrorCode::UnsupportedMode: return 501;
    case ErrorCode::IoError:
    case ErrorCode::WalCorrupt:
    case ErrorCode::DataCorrupt:
    case ErrorCode::TransactionIncomplete: return 500;
  }
  return 500;
}

static std::vector<float> embed_text(const std::string& text, uint32_t dim) {
  std::vector<float> v(dim, 0.0f);
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : text) {
    h ^= c; h *= 1099511628211ull;
    v[h % dim] += ((h >> 8) & 1) ? 1.0f : -1.0f;
  }
  float norm = 0.0f;
  for (float x : v) norm += x * x;
  norm = std::sqrt(norm);
  if (norm > 0) for (float& x : v) x /= norm;
  return v;
}

static bool parse_query_mode(const std::string& value, QueryMode* out) {
  if (!out) return false;
  if (value.empty() || value == "balanced") {
    *out = QueryMode::Balanced;
  } else if (value == "empirical") {
    *out = QueryMode::Empirical;
  } else if (value == "theoretical") {
    *out = QueryMode::Theoretical;
  } else {
    return false;
  }
  return true;
}

static void append_u32_array(std::ostringstream& output,
                             const std::vector<uint32_t>& values) {
  output << '[';
  for (size_t index = 0; index < values.size(); ++index) {
    if (index != 0) output << ',';
    output << values[index];
  }
  output << ']';
}

static void append_string_array(std::ostringstream& output,
                                const std::vector<std::string>& values) {
  output << '[';
  for (size_t index = 0; index < values.size(); ++index) {
    if (index != 0) output << ',';
    output << '"' << json_escape(values[index]) << '"';
  }
  output << ']';
}

static void append_dialectic_bundle(std::ostringstream& output,
                                    const BundleSet& bundle) {
  output << "{\"snapshot_version\":" << bundle.snapshot_version
         << ",\"semantic_candidates\":";
  append_u32_array(output, bundle.semantic_candidates);
  output << ",\"visited_states\":" << bundle.visited_states
         << ",\"truncated\":" << (bundle.truncated ? "true" : "false")
         << ",\"warnings\":";
  append_string_array(output, bundle.warnings);
  output << ",\"roots\":[";
  for (size_t root_index = 0; root_index < bundle.roots.size(); ++root_index) {
    if (root_index != 0) output << ',';
    const RootBundle& root = bundle.roots[root_index];
    output << "{\"root_node\":" << root.root_node
           << ",\"confidence\":" << root.confidence
           << ",\"degeneracy\":" << root.degeneracy
           << ",\"diversity\":" << root.diversity
           << ",\"contradiction_ratio\":" << root.contradiction_ratio
           << ",\"evidence_coverage\":" << root.evidence_coverage
           << ",\"provenance_risk\":" << root.provenance_risk
           << ",\"paths\":[";
    for (size_t path_index = 0; path_index < root.paths.size(); ++path_index) {
      if (path_index != 0) output << ',';
      const DialecticPath& path = root.paths[path_index];
      output << "{\"anchor_node\":" << path.anchor_node << ",\"nodes\":";
      append_u32_array(output, path.nodes);
      output << ",\"edges\":";
      append_u32_array(output, path.edges);
      output << ",\"score\":" << path.score
             << ",\"contains_contradiction\":"
             << (path.contains_contradiction ? "true" : "false")
             << ",\"contains_hypothetical\":"
             << (path.contains_hypothetical ? "true" : "false")
             << ",\"joint_requirements\":[";
      for (size_t joint_index = 0;
           joint_index < path.joint_requirements.size(); ++joint_index) {
        if (joint_index != 0) output << ',';
        const JointRequirement& joint = path.joint_requirements[joint_index];
        output << "{\"hyperedge_id\":\"" << json_escape(joint.hyperedge_id)
               << "\",\"target_node\":" << joint.target_node
               << ",\"source_nodes\":";
        append_u32_array(output, joint.source_nodes);
        output << ",\"member_edges\":";
        append_u32_array(output, joint.member_edges);
        output << ",\"all_sources_present\":"
               << (joint.all_sources_present ? "true" : "false") << '}';
      }
      output << "],\"provenance_findings\":[";
      for (size_t finding_index = 0;
           finding_index < path.provenance_findings.size(); ++finding_index) {
        if (finding_index != 0) output << ',';
        const ProvenanceFinding& finding =
            path.provenance_findings[finding_index];
        output << "{\"edge_id\":" << finding.edge_id << ",\"code\":\""
               << json_escape(finding.code) << "\",\"detail\":\""
               << json_escape(finding.detail) << "\"}";
      }
      output << "]}";
    }
    output << "]}";
  }
  output << "]}";
}

static void append_dialectic_convergence(std::ostringstream& output,
                                         const ConvergedAnswer& convergence) {
  output << "{\"has_answer\":"
         << (convergence.has_answer ? "true" : "false")
         << ",\"primary_node\":" << convergence.primary_node
         << ",\"confidence\":" << convergence.confidence
         << ",\"false_promotion_risk\":"
         << convergence.false_promotion_risk << ",\"evidence_edges\":";
  append_u32_array(output, convergence.evidence_edges);
  output << ",\"residual_uncertainty\":";
  append_string_array(output, convergence.residual_uncertainty);
  output << ",\"selected_path_count\":" << convergence.selected_paths.size()
         << ",\"discarded_path_count\":" << convergence.discarded_paths.size()
         << '}';
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
                                    std::string* output,
                                    std::string* error) {
  const JsonDocumentValue* value = json_member(object, key);
  if (!value) {
    if (required) {
      *error = key + " is required";
      return false;
    }
    output->clear();
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::String) {
    *error = key + " must be a string";
    return false;
  }
  if (required && value->scalar.empty()) {
    *error = key + " must not be empty";
    return false;
  }
  if (value->scalar.size() > maximum_bytes) {
    *error = key + " exceeds " + std::to_string(maximum_bytes) + " bytes";
    return false;
  }
  *output = value->scalar;
  return true;
}

template <typename Integer>
static bool extraction_integer_field(const JsonDocumentValue& object,
                                     const std::string& key,
                                     Integer default_value,
                                     Integer* output,
                                     std::string* error) {
  const JsonDocumentValue* value = json_member(object, key);
  if (!value) {
    *output = default_value;
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::Number ||
      value->scalar.find_first_of(".eE") != std::string::npos) {
    *error = key + " must be an integer";
    return false;
  }
  Integer parsed{};
  const char* begin = value->scalar.data();
  const char* end = begin + value->scalar.size();
  const auto result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc{} || result.ptr != end) {
    *error = key + " is outside its supported integer range";
    return false;
  }
  *output = parsed;
  return true;
}

static bool extraction_double_field(const JsonDocumentValue& object,
                                    const std::string& key,
                                    double default_value,
                                    double* output,
                                    std::string* error) {
  const JsonDocumentValue* value = json_member(object, key);
  if (!value) {
    *output = default_value;
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::Number) {
    *error = key + " must be a number";
    return false;
  }
  try {
    size_t consumed = 0;
    const double parsed = std::stod(value->scalar, &consumed);
    if (consumed != value->scalar.size() || !std::isfinite(parsed)) {
      *error = key + " must be a finite number";
      return false;
    }
    *output = parsed;
    return true;
  } catch (...) {
    *error = key + " is outside its supported numeric range";
    return false;
  }
}

static bool extraction_bool_field(const JsonDocumentValue& object,
                                  const std::string& key,
                                  bool default_value,
                                  bool* output,
                                  std::string* error) {
  const JsonDocumentValue* value = json_member(object, key);
  if (!value) {
    *output = default_value;
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::Boolean) {
    *error = key + " must be a boolean";
    return false;
  }
  *output = value->boolean;
  return true;
}

static bool parse_json_object(const std::string& body,
                              JsonDocumentValue* root,
                              std::string* error) {
  JsonDocumentParser parser(body);
  if (!parser.parse(root, error)) return false;
  if (root->kind != JsonDocumentValue::Kind::Object) {
    *error = "request body must be a JSON object";
    return false;
  }
  return true;
}

static bool parse_retrieval_policy(const JsonDocumentValue& parent,
                                   const std::string& field,
                                   bool required,
                                   RetrievalPolicy* output,
                                   std::string* error) {
  const JsonDocumentValue* value = json_member(parent, field);
  if (!value) {
    if (required) {
      *error = field + " is required";
      return false;
    }
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::Object) {
    *error = field + " must be an object";
    return false;
  }
  if (!reject_unknown_json_fields(
          *value,
          {"version", "semantic_candidates", "max_hops", "max_paths",
           "max_paths_per_root", "max_visited_states",
           "max_opposition_rounds", "minimum_confidence",
           "reexpansion_threshold"},
          field, error)) {
    return false;
  }
  RetrievalPolicy policy;
  if (!extraction_string_field(*value, "version", 256, true,
                               &policy.version, error) ||
      !extraction_integer_field(*value, "semantic_candidates",
                                uint32_t{12},
                                &policy.semantic_candidates, error) ||
      !extraction_integer_field(*value, "max_hops", uint32_t{6},
                                &policy.max_hops, error) ||
      !extraction_integer_field(*value, "max_paths", uint32_t{32},
                                &policy.max_paths, error) ||
      !extraction_integer_field(*value, "max_paths_per_root", uint32_t{8},
                                &policy.max_paths_per_root, error) ||
      !extraction_integer_field(*value, "max_visited_states",
                                uint32_t{20000},
                                &policy.max_visited_states, error) ||
      !extraction_integer_field(*value, "max_opposition_rounds",
                                uint32_t{1},
                                &policy.max_opposition_rounds, error) ||
      !extraction_double_field(*value, "minimum_confidence", 0.45,
                               &policy.minimum_confidence, error) ||
      !extraction_double_field(*value, "reexpansion_threshold", 0.25,
                               &policy.reexpansion_threshold, error)) {
    *error = field + ": " + *error;
    return false;
  }
  *output = std::move(policy);
  return true;
}

static void append_retrieval_policy(std::ostringstream& output,
                                    const RetrievalPolicy& policy) {
  output << "{\"version\":\"" << json_escape(policy.version)
         << "\",\"semantic_candidates\":" << policy.semantic_candidates
         << ",\"max_hops\":" << policy.max_hops
         << ",\"max_paths\":" << policy.max_paths
         << ",\"max_paths_per_root\":" << policy.max_paths_per_root
         << ",\"max_visited_states\":" << policy.max_visited_states
         << ",\"max_opposition_rounds\":" << policy.max_opposition_rounds
         << ",\"minimum_confidence\":" << policy.minimum_confidence
         << ",\"reexpansion_threshold\":" << policy.reexpansion_threshold
         << '}';
}

static DialecticOptions dialectic_options_for_policy(
    const RetrievalPolicy& policy) {
  DialecticOptions options;
  options.semantic_candidates = policy.semantic_candidates;
  options.max_hops = policy.max_hops;
  options.max_paths = policy.max_paths;
  options.max_paths_per_root = policy.max_paths_per_root;
  options.max_visited_states = policy.max_visited_states;
  options.max_opposition_rounds = policy.max_opposition_rounds;
  options.minimum_confidence = policy.minimum_confidence;
  options.reexpansion_threshold = policy.reexpansion_threshold;
  return options;
}

struct HypoKoshRequest {
  std::string query;
  std::string tenant_id;
  uint64_t signature{0};
  size_t max_hypotheses{8};
  bool use_active_policy{false};
  DialecticOptions options;
};

static bool parse_hypokosh_request(const std::string& body,
                                   HypoKoshRequest* output,
                                   std::string* error) {
  JsonDocumentValue root;
  if (!parse_json_object(body, &root, error) ||
      !reject_unknown_json_fields(
          root,
          {"query", "tenant_id", "signature", "max_hypotheses",
           "use_active_policy", "mode", "as_of", "semantic_candidates",
           "max_hops", "max_paths", "max_paths_per_root",
           "max_visited_states", "max_opposition_rounds",
           "minimum_confidence", "reexpansion_threshold"},
          "request", error)) {
    return false;
  }
  HypoKoshRequest request;
  std::string mode;
  uint32_t max_hypotheses = 8;
  if (!extraction_string_field(root, "query", kMaxNodeTextBytes, true,
                               &request.query, error) ||
      !extraction_string_field(root, "tenant_id", 256, false,
                               &request.tenant_id, error) ||
      !extraction_integer_field(root, "signature", uint64_t{0},
                                &request.signature, error) ||
      !extraction_integer_field(root, "max_hypotheses", uint32_t{8},
                                &max_hypotheses, error) ||
      !extraction_bool_field(root, "use_active_policy", false,
                             &request.use_active_policy, error) ||
      !extraction_string_field(root, "mode", 32, false, &mode, error) ||
      !extraction_string_field(root, "as_of", 128, false,
                               &request.options.as_of, error) ||
      !extraction_integer_field(root, "semantic_candidates", size_t{12},
                                &request.options.semantic_candidates, error) ||
      !extraction_integer_field(root, "max_hops", uint32_t{6},
                                &request.options.max_hops, error) ||
      !extraction_integer_field(root, "max_paths", size_t{32},
                                &request.options.max_paths, error) ||
      !extraction_integer_field(root, "max_paths_per_root", size_t{8},
                                &request.options.max_paths_per_root, error) ||
      !extraction_integer_field(root, "max_visited_states",
                                size_t{20000},
                                &request.options.max_visited_states, error) ||
      !extraction_integer_field(root, "max_opposition_rounds", uint32_t{1},
                                &request.options.max_opposition_rounds,
                                error) ||
      !extraction_double_field(root, "minimum_confidence", 0.45,
                               &request.options.minimum_confidence, error) ||
      !extraction_double_field(root, "reexpansion_threshold", 0.25,
                               &request.options.reexpansion_threshold,
                               error)) {
    return false;
  }
  if (!parse_query_mode(mode, &request.options.mode)) {
    *error = "mode must be empirical, balanced, or theoretical";
    return false;
  }
  if (max_hypotheses < 1 || max_hypotheses > 16) {
    *error = "max_hypotheses must be between 1 and 16";
    return false;
  }
  if (request.options.semantic_candidates < 1 ||
      request.options.semantic_candidates > 64 ||
      request.options.max_hops < 1 ||
      request.options.max_hops > kMaxDialecticHops ||
      request.options.max_paths < 1 ||
      request.options.max_paths > kMaxDialecticPaths ||
      request.options.max_paths_per_root < 1 ||
      request.options.max_paths_per_root > 64 ||
      request.options.max_visited_states < 1 ||
      request.options.max_visited_states > kMaxDialecticVisitedStates ||
      request.options.max_opposition_rounds > 2 ||
      request.options.minimum_confidence < 0.0 ||
      request.options.minimum_confidence > 1.0 ||
      request.options.reexpansion_threshold < 0.0 ||
      request.options.reexpansion_threshold > 1.0) {
    *error = "dialectic options exceed governed safety bounds";
    return false;
  }
  if (request.use_active_policy && request.tenant_id.empty()) {
    *error = "tenant_id is required when use_active_policy is true";
    return false;
  }
  request.max_hypotheses = max_hypotheses;
  *output = std::move(request);
  return true;
}

static std::string hypokosh_json(const HypothesisSet& hypotheses,
                                 const std::optional<PolicyState>& active) {
  std::ostringstream output;
  output << "{\"snapshot_version\":" << hypotheses.snapshot_version
         << ",\"epistemic_status\":\""
         << json_escape(hypotheses.epistemic_status)
         << "\",\"durable_writes\":"
         << (hypotheses.durable_writes ? "true" : "false")
         << ",\"active_policy\":";
  if (active) append_retrieval_policy(output, active->policy);
  else output << "null";
  output << ",\"discriminating_tests\":";
  append_string_array(output, hypotheses.discriminating_tests);
  output << ",\"warnings\":";
  append_string_array(output, hypotheses.warnings);
  output << ",\"proposals\":[";
  for (size_t index = 0; index < hypotheses.proposals.size(); ++index) {
    if (index != 0) output << ',';
    const HypothesisProposal& proposal = hypotheses.proposals[index];
    output << "{\"root_node\":" << proposal.root_node
           << ",\"statement\":\"" << json_escape(proposal.statement)
           << "\",\"origin\":\"hypothetical\",\"plausibility\":"
           << proposal.plausibility << ",\"evidence_edges\":";
    append_u32_array(output, proposal.evidence_edges);
    output << ",\"discriminating_tests\":";
    append_string_array(output, proposal.discriminating_tests);
    output << ",\"eligible_for_truth_promotion\":"
           << (proposal.eligible_for_truth_promotion ? "true" : "false")
           << '}';
  }
  output << "]}";
  return output.str();
}

static bool parse_episode_split(const std::string& value,
                                EpisodeSplit* output) {
  if (value == "training") *output = EpisodeSplit::Training;
  else if (value == "development") *output = EpisodeSplit::Development;
  else if (value == "evaluation") *output = EpisodeSplit::Evaluation;
  else return false;
  return true;
}

static bool parse_learning_episode_request(
    const std::string& body,
    LearningEpisodeInput* output,
    std::string* error) {
  JsonDocumentValue root;
  if (!parse_json_object(body, &root, error) ||
      !reject_unknown_json_fields(
          root,
          {"schema_version", "tenant_id", "episode_id", "family", "domain",
           "split", "query", "signature", "model_version", "policy",
           "outcome_verified", "verifier_id", "outcome_evidence_id",
           "task_success", "causal_f1", "evidence_coverage",
           "calibration_error", "latency_ms", "token_cost", "action_cost",
           "false_promotion", "harmful_action", "unauthorized_action",
           "harmful_memory_activation", "expired_truth_activation",
           "intermediate_trace_bytes", "retained_trace_bytes",
           "decisive_evidence_total", "decisive_evidence_retained",
           "useful_evidence_total", "useful_evidence_retrieved_at_20",
           "evidence_node_ids", "legal_hold"},
          "request", error)) {
    return false;
  }
  LearningEpisodeInput input;
  std::string split;
  if (!extraction_integer_field(root, "schema_version",
                                kLearningSchemaVersion,
                                &input.schema_version, error) ||
      !extraction_string_field(root, "tenant_id", 256, true,
                               &input.tenant_id, error) ||
      !extraction_string_field(root, "episode_id", 256, true,
                               &input.episode_id, error) ||
      !extraction_string_field(root, "family", 256, true,
                               &input.family, error) ||
      !extraction_string_field(root, "domain", 256, true,
                               &input.domain, error) ||
      !extraction_string_field(root, "split", 32, true, &split, error) ||
      !extraction_string_field(root, "query", kMaxNodeTextBytes, true,
                               &input.query, error) ||
      !extraction_integer_field(root, "signature", uint64_t{0},
                                &input.signature, error) ||
      !extraction_string_field(root, "model_version", 256, true,
                               &input.model_version, error) ||
      !parse_retrieval_policy(root, "policy", true, &input.policy, error) ||
      !extraction_bool_field(root, "outcome_verified", false,
                             &input.outcome_verified, error) ||
      !extraction_string_field(root, "verifier_id", 256, false,
                               &input.verifier_id, error) ||
      !extraction_string_field(root, "outcome_evidence_id", 256, false,
                               &input.outcome_evidence_id, error) ||
      !extraction_bool_field(root, "task_success", false,
                             &input.task_success, error) ||
      !extraction_double_field(root, "causal_f1", 0.0,
                               &input.causal_f1, error) ||
      !extraction_double_field(root, "evidence_coverage", 0.0,
                               &input.evidence_coverage, error) ||
      !extraction_double_field(root, "calibration_error", 0.0,
                               &input.calibration_error, error) ||
      !extraction_double_field(root, "latency_ms", 0.0,
                               &input.latency_ms, error) ||
      !extraction_double_field(root, "token_cost", 0.0,
                               &input.token_cost, error) ||
      !extraction_double_field(root, "action_cost", 0.0,
                               &input.action_cost, error) ||
      !extraction_bool_field(root, "false_promotion", false,
                             &input.false_promotion, error) ||
      !extraction_bool_field(root, "harmful_action", false,
                             &input.harmful_action, error) ||
      !extraction_bool_field(root, "unauthorized_action", false,
                             &input.unauthorized_action, error) ||
      !extraction_bool_field(root, "harmful_memory_activation", false,
                             &input.harmful_memory_activation, error) ||
      !extraction_bool_field(root, "expired_truth_activation", false,
                             &input.expired_truth_activation, error) ||
      !extraction_integer_field(root, "intermediate_trace_bytes", uint64_t{0},
                                &input.intermediate_trace_bytes, error) ||
      !extraction_integer_field(root, "retained_trace_bytes", uint64_t{0},
                                &input.retained_trace_bytes, error) ||
      !extraction_integer_field(root, "decisive_evidence_total", uint32_t{0},
                                &input.decisive_evidence_total, error) ||
      !extraction_integer_field(root, "decisive_evidence_retained",
                                uint32_t{0},
                                &input.decisive_evidence_retained, error) ||
      !extraction_integer_field(root, "useful_evidence_total", uint32_t{0},
                                &input.useful_evidence_total, error) ||
      !extraction_integer_field(root, "useful_evidence_retrieved_at_20",
                                uint32_t{0},
                                &input.useful_evidence_retrieved_at_20,
                                error) ||
      !extraction_bool_field(root, "legal_hold", false,
                             &input.legal_hold, error)) {
    return false;
  }
  if (!parse_episode_split(split, &input.split)) {
    *error = "split must be training, development, or evaluation";
    return false;
  }
  const JsonDocumentValue* evidence = json_member(root, "evidence_node_ids");
  if (evidence) {
    if (evidence->kind != JsonDocumentValue::Kind::Array ||
        evidence->array.size() > 256) {
      *error = "evidence_node_ids must be an array of at most 256 integers";
      return false;
    }
    for (const JsonDocumentValue& value : evidence->array) {
      if (value.kind != JsonDocumentValue::Kind::Number ||
          value.scalar.find_first_of(".eE") != std::string::npos) {
        *error = "evidence_node_ids values must be integers";
        return false;
      }
      uint32_t node_id = 0;
      const auto parsed = std::from_chars(
          value.scalar.data(), value.scalar.data() + value.scalar.size(),
          node_id);
      if (parsed.ec != std::errc{} ||
          parsed.ptr != value.scalar.data() + value.scalar.size()) {
        *error = "evidence_node_ids value is outside uint32 range";
        return false;
      }
      input.evidence_node_ids.push_back(node_id);
    }
  }
  *output = std::move(input);
  return true;
}

static bool parse_policy_evaluation_request(
    const std::string& body,
    std::string* tenant_id,
    RetrievalPolicy* baseline,
    PolicyLearningOptions* options,
    std::string* error) {
  JsonDocumentValue root;
  if (!parse_json_object(body, &root, error) ||
      !reject_unknown_json_fields(root, {"tenant_id", "baseline", "options"},
                                  "request", error) ||
      !extraction_string_field(root, "tenant_id", 256, true, tenant_id,
                               error) ||
      !parse_retrieval_policy(root, "baseline", true, baseline, error)) {
    return false;
  }
  const JsonDocumentValue* value = json_member(root, "options");
  if (!value) return true;
  if (value->kind != JsonDocumentValue::Kind::Object ||
      !reject_unknown_json_fields(
          *value,
          {"minimum_training_samples", "minimum_development_samples",
           "minimum_development_utility_improvement",
           "maximum_domain_regression", "maximum_retention_decisions"},
          "options", error)) {
    if (value->kind != JsonDocumentValue::Kind::Object) {
      *error = "options must be an object";
    }
    return false;
  }
  uint64_t retention_limit = options->maximum_retention_decisions;
  if (!extraction_integer_field(*value, "minimum_training_samples",
                                options->minimum_training_samples,
                                &options->minimum_training_samples, error) ||
      !extraction_integer_field(*value, "minimum_development_samples",
                                options->minimum_development_samples,
                                &options->minimum_development_samples, error) ||
      !extraction_double_field(
          *value, "minimum_development_utility_improvement",
          options->minimum_development_utility_improvement,
          &options->minimum_development_utility_improvement, error) ||
      !extraction_double_field(*value, "maximum_domain_regression",
                               options->maximum_domain_regression,
                               &options->maximum_domain_regression, error) ||
      !extraction_integer_field(*value, "maximum_retention_decisions",
                                retention_limit, &retention_limit, error)) {
    return false;
  }
  if (retention_limit > 10000) {
    *error = "maximum_retention_decisions exceeds 10000";
    return false;
  }
  options->maximum_retention_decisions =
      static_cast<size_t>(retention_limit);
  return true;
}

static bool parse_policy_decision_request(
    const std::string& body,
    PolicyDecisionInput* output,
    std::string* error) {
  JsonDocumentValue root;
  if (!parse_json_object(body, &root, error) ||
      !reject_unknown_json_fields(
          root,
          {"schema_version", "tenant_id", "event_id", "action", "policy",
           "approved", "approver_id", "evaluation_reference", "reason"},
          "request", error)) {
    return false;
  }
  PolicyDecisionInput input;
  std::string action;
  if (!extraction_integer_field(root, "schema_version",
                                kLearningSchemaVersion,
                                &input.schema_version, error) ||
      !extraction_string_field(root, "tenant_id", 256, true,
                               &input.tenant_id, error) ||
      !extraction_string_field(root, "event_id", 256, true,
                               &input.event_id, error) ||
      !extraction_string_field(root, "action", 32, true, &action, error) ||
      !parse_retrieval_policy(root, "policy", true, &input.policy, error) ||
      !extraction_bool_field(root, "approved", false, &input.approved,
                             error) ||
      !extraction_string_field(root, "approver_id", 256, true,
                               &input.approver_id, error) ||
      !extraction_string_field(root, "evaluation_reference", 256, true,
                               &input.evaluation_reference, error) ||
      !extraction_string_field(root, "reason", 4096, true,
                               &input.reason, error)) {
    return false;
  }
  if (action == "promote") input.action = PolicyDecisionAction::Promote;
  else if (action == "rollback") input.action = PolicyDecisionAction::Rollback;
  else {
    *error = "action must be promote or rollback";
    return false;
  }
  *output = std::move(input);
  return true;
}

static std::string policy_state_json(const PolicyState& state,
                                     bool replay) {
  std::ostringstream output;
  output << "{\"node_id\":" << state.node_id
         << ",\"event_id\":\"" << json_escape(state.event_id)
         << "\",\"action\":\"" << policy_decision_action_name(state.action)
         << "\",\"policy\":";
  append_retrieval_policy(output, state.policy);
  output << ",\"previous_policy_version\":\""
         << json_escape(state.previous_policy_version)
         << "\",\"approver_id\":\"" << json_escape(state.approver_id)
         << "\",\"evaluation_reference\":\""
         << json_escape(state.evaluation_reference)
         << "\",\"reason\":\"" << json_escape(state.reason)
         << "\",\"idempotent_replay\":" << (replay ? "true" : "false")
         << '}';
  return output.str();
}

static std::string policy_evaluation_json(
    const PolicyEvaluationReport& report) {
  std::ostringstream output;
  output << "{\"baseline\":";
  append_retrieval_policy(output, report.baseline);
  output << ",\"has_recommendation\":"
         << (report.has_recommendation ? "true" : "false")
         << ",\"recommended\":";
  if (report.has_recommendation) append_retrieval_policy(output, report.recommended);
  else output << "null";
  output << ",\"development_utility_improvement\":"
         << report.development_utility_improvement
         << ",\"evaluation_episodes_excluded\":"
         << report.evaluation_episodes_excluded
         << ",\"training_episode_node_ids\":";
  append_u32_array(output, report.training_episode_node_ids);
  output << ",\"data_policy\":{\"episodes\":"
         << report.data_policy.episodes
         << ",\"trace_retention_ratio\":"
         << report.data_policy.trace_retention_ratio
         << ",\"decisive_evidence_retention\":"
         << report.data_policy.decisive_evidence_retention
         << ",\"useful_evidence_recall_at_20\":"
         << report.data_policy.useful_evidence_recall_at_20
         << ",\"harmful_memory_activation_rate\":"
         << report.data_policy.harmful_memory_activation_rate
         << ",\"expired_truth_activation_rate\":"
         << report.data_policy.expired_truth_activation_rate << '}';
  output << ",\"candidates\":[";
  for (size_t index = 0; index < report.candidates.size(); ++index) {
    if (index != 0) output << ',';
    const PolicyMetrics& metrics = report.candidates[index];
    output << "{\"policy\":";
    append_retrieval_policy(output, metrics.policy);
    output << ",\"training_samples\":" << metrics.training_samples
           << ",\"development_samples\":" << metrics.development_samples
           << ",\"safety_violations\":" << metrics.safety_violations
           << ",\"mean_training_utility\":"
           << metrics.mean_training_utility
           << ",\"mean_development_utility\":"
           << metrics.mean_development_utility
           << ",\"development_task_success\":"
           << metrics.development_task_success
           << ",\"worst_domain_regression\":"
           << metrics.worst_domain_regression
           << ",\"eligible\":" << (metrics.eligible ? "true" : "false")
           << ",\"ineligibility_reason\":\""
           << json_escape(metrics.ineligibility_reason) << "\"}";
  }
  output << "],\"retention_decisions\":[";
  for (size_t index = 0; index < report.retention_decisions.size(); ++index) {
    if (index != 0) output << ',';
    const DataRetentionDecision& decision =
        report.retention_decisions[index];
    output << "{\"episode_id\":\"" << json_escape(decision.episode_id)
           << "\",\"utility_class\":\""
           << data_utility_class_name(decision.utility_class)
           << "\",\"retain_for_training\":"
           << (decision.retain_for_training ? "true" : "false")
           << ",\"archive_intermediate_trace\":"
           << (decision.archive_intermediate_trace ? "true" : "false")
           << ",\"reason\":\"" << json_escape(decision.reason) << "\"}";
  }
  output << "],\"warnings\":";
  append_string_array(output, report.warnings);
  output << ",\"durable_writes\":"
         << (report.durable_writes ? "true" : "false") << '}';
  return output.str();
}

static bool extraction_metadata(const JsonDocumentValue& object,
                                std::map<std::string, std::string>* output,
                                std::string* error) {
  const JsonDocumentValue* metadata = json_member(object, "metadata");
  if (!metadata) return true;
  if (metadata->kind != JsonDocumentValue::Kind::Object) {
    *error = "metadata must be an object of string values";
    return false;
  }
  if (metadata->object.size() > kMaxExtractionMetadataEntries) {
    *error = "metadata exceeds " +
             std::to_string(kMaxExtractionMetadataEntries) + " entries";
    return false;
  }
  for (const auto& [key, value] : metadata->object) {
    if (key.empty() || key.size() > kMaxExtractionMetadataKeyBytes) {
      *error = "metadata key must be between 1 and " +
               std::to_string(kMaxExtractionMetadataKeyBytes) + " bytes";
      return false;
    }
    if (key.rfind("graphene_", 0) == 0) {
      *error = "metadata key '" + key +
               "' uses the reserved graphene_ namespace";
      return false;
    }
    if (value.kind != JsonDocumentValue::Kind::String) {
      *error = "metadata value for '" + key + "' must be a string";
      return false;
    }
    if (value.scalar.size() > kMaxExtractionMetadataValueBytes) {
      *error = "metadata value for '" + key + "' exceeds " +
               std::to_string(kMaxExtractionMetadataValueBytes) + " bytes";
      return false;
    }
    output->emplace(key, value.scalar);
  }
  return true;
}

static bool parse_extraction_role(const std::string& raw,
                                  ExtractionRole* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "node") *output = ExtractionRole::Node;
  else if (value == "root") *output = ExtractionRole::Root;
  else if (value == "symptom") *output = ExtractionRole::Symptom;
  else if (value == "impact") *output = ExtractionRole::Impact;
  else return false;
  return true;
}

static bool parse_strict_origin(const std::string& raw, EdgeOrigin* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "observed") *output = EdgeOrigin::Observed;
  else if (value == "discovered") *output = EdgeOrigin::Discovered;
  else if (value == "inferred") *output = EdgeOrigin::Inferred;
  else if (value == "reinforced") *output = EdgeOrigin::Reinforced;
  else if (value == "hypothetical") *output = EdgeOrigin::Hypothetical;
  else return false;
  return true;
}

static bool parse_strict_edge_role(const std::string& raw, EdgeRole* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "supports" || value == "support") {
    *output = EdgeRole::Supports;
  } else if (value == "mechanistic") *output = EdgeRole::Mechanistic;
  else if (value == "compressed") *output = EdgeRole::Compressed;
  else if (value == "analogical") *output = EdgeRole::Analogical;
  else if (value == "predictive") *output = EdgeRole::Predictive;
  else if (value == "causal") *output = EdgeRole::Causal;
  else if (value == "contradicts" || value == "contradiction") {
    *output = EdgeRole::Contradicts;
  } else if (value == "supersedes" || value == "supersession") {
    *output = EdgeRole::Supersedes;
  } else {
    return false;
  }
  return true;
}

static bool parse_bond_type(const std::string& raw, BondType* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "none") *output = BondType::None;
  else if (value == "sigma") *output = BondType::Sigma;
  else if (value == "pi") *output = BondType::Pi;
  else if (value == "van_der_waals" || value == "vanderwaals") {
    *output = BondType::VanDerWaals;
  } else if (value == "defect") *output = BondType::Defect;
  else if (value == "synthetic") *output = BondType::Synthetic;
  else return false;
  return true;
}

static bool parse_defect_type(const std::string& raw, DefectType* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "none") *output = DefectType::None;
  else if (value == "vacancy") *output = DefectType::Vacancy;
  else if (value == "substitution") *output = DefectType::Substitution;
  else if (value == "stone_wales" || value == "stonewales") {
    *output = DefectType::StoneWales;
  } else if (value == "strain") *output = DefectType::Strain;
  else if (value == "doped") *output = DefectType::Doped;
  else if (value == "boundary") *output = DefectType::Boundary;
  else return false;
  return true;
}

static bool parse_layer_coupling(const std::string& raw,
                                 LayerCoupling* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "same_layer") {
    *output = LayerCoupling::SameLayer;
  } else if (value == "none") *output = LayerCoupling::None;
  else if (value == "van_der_waals" || value == "vanderwaals") {
    *output = LayerCoupling::VanDerWaals;
  } else if (value == "bernal_stacked") *output = LayerCoupling::BernalStacked;
  else if (value == "twisted") *output = LayerCoupling::Twisted;
  else if (value == "synthetic") *output = LayerCoupling::Synthetic;
  else return false;
  return true;
}

static bool parse_extraction_request(const std::string& body,
                                     uint32_t dimension,
                                     size_t maximum_nodes,
                                     ExtractionInput* output,
                                     std::string* error) {
  JsonDocumentValue root;
  JsonDocumentParser parser(body);
  if (!parser.parse(&root, error)) return false;
  if (root.kind != JsonDocumentValue::Kind::Object) {
    *error = "request body must be a JSON object";
    return false;
  }
  if (!reject_unknown_json_fields(
          root,
          {"schema_version", "source_id", "source_uri", "extraction_run_id",
           "layer", "incident", "signature", "place_missing_lattice",
           "idempotent", "nodes", "relations"},
          "request", error)) {
    return false;
  }

  ExtractionInput input;
  if (!extraction_integer_field(root, "schema_version",
                                kExtractionSchemaVersion,
                                &input.schema_version, error) ||
      !extraction_string_field(root, "source_id",
                               kMaxExtractionIdentifierBytes, true,
                               &input.source_id, error) ||
      !extraction_string_field(root, "source_uri",
                               kMaxExtractionMetadataValueBytes, false,
                               &input.source_uri, error) ||
      !extraction_string_field(root, "extraction_run_id",
                               kMaxExtractionIdentifierBytes, false,
                               &input.extraction_run_id, error) ||
      !extraction_integer_field(root, "layer", int32_t{0}, &input.layer,
                                error) ||
      !extraction_integer_field(root, "incident", uint32_t{0},
                                &input.incident, error) ||
      !extraction_integer_field(root, "signature", uint64_t{0},
                                &input.signature, error) ||
      !extraction_bool_field(root, "place_missing_lattice", true,
                             &input.place_missing_lattice, error) ||
      !extraction_bool_field(root, "idempotent", true, &input.idempotent,
                             error)) {
    return false;
  }
  if (input.schema_version != kExtractionSchemaVersion) {
    *error = "unsupported extraction schema_version";
    return false;
  }

  const JsonDocumentValue* nodes = json_member(root, "nodes");
  const JsonDocumentValue* relations = json_member(root, "relations");
  if (nodes && nodes->kind != JsonDocumentValue::Kind::Array) {
    *error = "nodes must be an array";
    return false;
  }
  if (relations && relations->kind != JsonDocumentValue::Kind::Array) {
    *error = "relations must be an array";
    return false;
  }
  const size_t node_count = nodes ? nodes->array.size() : 0;
  const size_t relation_count = relations ? relations->array.size() : 0;
  if (node_count == 0 && relation_count == 0) {
    *error = "at least one node or relation is required";
    return false;
  }
  if (node_count > maximum_nodes) {
    *error = "nodes exceeds configured maximum of " +
             std::to_string(maximum_nodes);
    return false;
  }
  if (relation_count > kMaxExtractionRelations) {
    *error = "relations exceeds maximum of " +
             std::to_string(kMaxExtractionRelations);
    return false;
  }

  input.nodes.reserve(node_count);
  if (nodes) {
    for (size_t index = 0; index < nodes->array.size(); ++index) {
      const JsonDocumentValue& value = nodes->array[index];
      const std::string context = "nodes[" + std::to_string(index) + "]";
      if (value.kind != JsonDocumentValue::Kind::Object) {
        *error = context + " must be an object";
        return false;
      }
      if (!reject_unknown_json_fields(
              value,
              {"external_id", "content", "signature", "incident", "role",
               "metadata", "lattice", "defect_type"},
              context, error)) {
        return false;
      }
      ExtractionNode node;
      std::string role;
      std::string defect;
      if (!extraction_string_field(value, "external_id",
                                   kMaxExtractionIdentifierBytes, true,
                                   &node.external_id, error) ||
          !extraction_string_field(value, "content", kMaxNodeTextBytes, true,
                                   &node.content, error) ||
          !extraction_integer_field(value, "signature", uint64_t{0},
                                    &node.signature, error) ||
          !extraction_integer_field(value, "incident", uint32_t{0},
                                    &node.incident, error) ||
          !extraction_string_field(value, "role", 32, false, &role, error) ||
          !extraction_string_field(value, "defect_type", 32, false, &defect,
                                   error) ||
          !extraction_metadata(value, &node.metadata, error)) {
        *error = context + ": " + *error;
        return false;
      }
      if (!parse_extraction_role(role, &node.role)) {
        *error = context + ": role must be node, root, symptom, or impact";
        return false;
      }
      if (!parse_defect_type(defect, &node.defect_type)) {
        *error = context + ": unsupported defect_type";
        return false;
      }
      const JsonDocumentValue* lattice = json_member(value, "lattice");
      if (lattice) {
        if (lattice->kind != JsonDocumentValue::Kind::Object ||
            !reject_unknown_json_fields(*lattice, {"q", "r", "layer"},
                                        context + ".lattice", error)) {
          if (error->empty()) *error = context + ": lattice must be an object";
          return false;
        }
        if (!json_member(*lattice, "q") || !json_member(*lattice, "r")) {
          *error = context + ".lattice: q and r are required";
          return false;
        }
        LatticeCoord coordinate;
        if (!extraction_integer_field(*lattice, "q", int32_t{0},
                                      &coordinate.q, error) ||
            !extraction_integer_field(*lattice, "r", int32_t{0},
                                      &coordinate.r, error) ||
            !extraction_integer_field(*lattice, "layer", input.layer,
                                      &coordinate.layer, error)) {
          *error = context + ".lattice: " + *error;
          return false;
        }
        node.lattice = coordinate;
      }
      node.vector = embed_text(node.content, dimension);
      input.nodes.push_back(std::move(node));
    }
  }

  input.relations.reserve(relation_count);
  if (relations) {
    for (size_t index = 0; index < relations->array.size(); ++index) {
      const JsonDocumentValue& value = relations->array[index];
      const std::string context = "relations[" + std::to_string(index) + "]";
      if (value.kind != JsonDocumentValue::Kind::Object) {
        *error = context + " must be an object";
        return false;
      }
      if (!reject_unknown_json_fields(
              value,
              {"from_external_id", "to_external_id", "origin", "role",
               "confidence", "evidence_id", "evidence_uri", "evidence_text",
               "metadata", "bond_type", "defect_type", "layer_coupling",
               "bond_strength"},
              context, error)) {
        return false;
      }
      ExtractionRelation relation;
      relation.bond_type = BondType::None;
      std::string origin;
      std::string role;
      std::string bond;
      std::string defect;
      std::string coupling;
      if (!extraction_string_field(value, "from_external_id",
                                   kMaxExtractionIdentifierBytes, true,
                                   &relation.from_external_id, error) ||
          !extraction_string_field(value, "to_external_id",
                                   kMaxExtractionIdentifierBytes, true,
                                   &relation.to_external_id, error) ||
          !extraction_string_field(value, "origin", 32, false, &origin,
                                   error) ||
          !extraction_string_field(value, "role", 32, false, &role, error) ||
          !extraction_double_field(value, "confidence", 0.9,
                                   &relation.confidence, error) ||
          !extraction_string_field(value, "evidence_id",
                                   kMaxExtractionIdentifierBytes, false,
                                   &relation.evidence_id, error) ||
          !extraction_string_field(value, "evidence_uri",
                                   kMaxExtractionMetadataValueBytes, false,
                                   &relation.evidence_uri, error) ||
          !extraction_string_field(value, "evidence_text",
                                   kMaxNodeTextBytes, false,
                                   &relation.evidence_text, error) ||
          !extraction_string_field(value, "bond_type", 32, false, &bond,
                                   error) ||
          !extraction_string_field(value, "defect_type", 32, false, &defect,
                                   error) ||
          !extraction_string_field(value, "layer_coupling", 32, false,
                                   &coupling, error) ||
          !extraction_double_field(value, "bond_strength", 0.85,
                                   &relation.bond_strength, error) ||
          !extraction_metadata(value, &relation.metadata, error)) {
        *error = context + ": " + *error;
        return false;
      }
      if (!parse_strict_origin(origin, &relation.origin)) {
        *error = context + ": unsupported origin";
        return false;
      }
      if (!parse_strict_edge_role(role, &relation.role)) {
        *error = context + ": unsupported role";
        return false;
      }
      if (!parse_bond_type(bond, &relation.bond_type)) {
        *error = context + ": unsupported bond_type";
        return false;
      }
      if (!parse_defect_type(defect, &relation.defect_type)) {
        *error = context + ": unsupported defect_type";
        return false;
      }
      if (!parse_layer_coupling(coupling, &relation.layer_coupling)) {
        *error = context + ": unsupported layer_coupling";
        return false;
      }
      if (relation.confidence < 0.0 || relation.confidence > 1.0) {
        *error = context + ": confidence must be between 0 and 1";
        return false;
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
      if (!e) { if (eid > edge_total + 16) break; continue; }
      if (!edge_valid_at(db, *e, as_of)) continue;
      if (e->role == EdgeRole::Contradicts && (bundle_nodes.count(e->from) || bundle_nodes.count(e->to))) {
        if (attached_ids.insert(e->id).second) attached_contradictions.push_back(*e);
        has_contra = true;
      }
    }
    js << "{\"target\":" << kv.first << ",\"degeneracy\":" << kv.second.size() << ",\"best_confidence\":" << best << ",\"has_contradiction\":" << (has_contra?"true":"false") << ",\"attached_contradictions\":[";
    for (size_t ci=0; ci<attached_contradictions.size(); ++ci) {
      const auto& ce = attached_contradictions[ci];
      if (ci) js << ',';
      js << "{\"edge\":" << ce.id << ",\"from\":" << ce.from << ",\"to\":" << ce.to << ",\"origin\":\"" << origin_name(ce.origin) << "\",\"role\":\"" << role_name(ce.role) << "\",\"confidence\":" << ce.confidence << "}";
    }
    js << "],\"paths\":[";
    bool first_path = true;
    for (const auto& st : kv.second) {
      if (!first_path) js << ',';
      first_path = false;
      js << "{\"nodes\":[";
      for (size_t i=0;i<st.nodes.size();++i){ if(i) js << ','; js << st.nodes[i]; }
      js << "],\"edges\":[";
      for (size_t i=0;i<st.edges.size();++i){ if(i) js << ','; js << st.edges[i]; }
      js << "],\"roles\":[";
      for (size_t i=0;i<st.edges.size();++i){ auto e=db.get_edge(st.edges[i]); if(i) js << ','; js << "\"" << (e?role_name(e->role):"") << "\""; }
      js << "],\"origins\":[";
      for (size_t i=0;i<st.edges.size();++i){ auto e=db.get_edge(st.edges[i]); if(i) js << ','; js << "\"" << (e?origin_name(e->origin):"") << "\""; }
      js << "],\"confidence\":" << st.confidence << ",\"temporal_consistent\":" << (as_of.empty()?"true":"true") << ",\"contains_contradiction\":" << (st.contradiction?"true":"false") << ",\"contains_hypothetical\":" << (st.hypothetical?"true":"false") << "}";
    }
    js << "]}";
  }
  js << "]}";
  return js.str();
}

static uint64_t physical_capacity(uint32_t radius, uint32_t layers = 1) {
  const uint64_t per_layer = 1ull + 3ull * radius * (static_cast<uint64_t>(radius) + 1ull);
  return per_layer * std::max<uint32_t>(1, layers);
}

static uint32_t minimum_radius_for_nodes(uint64_t nodes) {
  if (nodes <= 1) return 0;
  const double root = (-3.0 + std::sqrt(std::max(0.0, 12.0 * static_cast<double>(nodes) - 3.0))) / 6.0;
  return static_cast<uint32_t>(std::ceil(root));
}

static bool loopback_address(const std::string& address) {
  return address == "127.0.0.1" || address == "::1" || address == "localhost";
}

static std::string status_text(int code) {
  switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 411: return "Length Required";
    case 413: return "Payload Too Large";
    case 415: return "Unsupported Media Type";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 503: return "Service Unavailable";
    case 507: return "Insufficient Storage";
    default: return "Error";
  }
}

static std::string response(int code, const std::string& body,
                            const std::string& request_id = {},
                            const std::string& content_type = "application/json",
                            int retry_after_seconds = 0) {
  std::ostringstream os;
  os << "HTTP/1.1 " << code << ' ' << status_text(code)
     << "\r\nContent-Type: " << content_type
     << "\r\nContent-Length: " << body.size()
     << "\r\nConnection: close"
     << "\r\nCache-Control: no-store"
     << "\r\nX-Content-Type-Options: nosniff"
     << "\r\nX-GrapheneDB-Version: " << kServerVersion
     << "\r\nX-GrapheneDB-API-Version: " << kApiVersion;
  if (!request_id.empty()) os << "\r\nX-Request-ID: " << request_id;
  if (retry_after_seconds > 0) os << "\r\nRetry-After: " << retry_after_seconds;
  os << "\r\n\r\n" << body;
  return os.str();
}

static bool write_all(int fd, const std::string& payload) {
  size_t sent = 0;
  while (sent < payload.size()) {
    const ssize_t n = ::write(fd, payload.data() + sent, payload.size() - sent);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) return false;
    sent += static_cast<size_t>(n);
  }
  return true;
}

static void structured_request_log(const std::string& request_id,
                                   const std::string& client_ip,
                                   const std::string& method,
                                   const std::string& path,
                                   int status,
                                   uint64_t duration_us,
                                   size_t bytes_in,
                                   size_t bytes_out,
                                   bool rate_limited) {
  std::ostringstream log;
  log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
      << "\",\"level\":\"" << (status >= 500 ? "error" : status >= 400 ? "warn" : "info")
      << "\",\"event\":\"http_request\",\"request_id\":\"" << json_escape(request_id)
      << "\",\"client_ip\":\"" << json_escape(client_ip)
      << "\",\"method\":\"" << json_escape(method)
      << "\",\"path\":\"" << json_escape(path)
      << "\",\"status\":" << status
      << ",\"duration_us\":" << duration_us
      << ",\"bytes_in\":" << bytes_in
      << ",\"bytes_out\":" << bytes_out
      << ",\"rate_limited\":" << (rate_limited ? "true" : "false") << "}";
  emit_structured_log(log.str());
}


int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "usage: graphenedb_server <db_dir> <dimension> <port> [options]\n"
              << "  --bind-address IP\n"
              << "  --physical-lattice-primary --physical-lattice-radius N\n"
              << "  --expected-max-nodes N --wal-rotate-bytes N\n"
              << "  --workers N --queue-capacity N\n"
              << "  --max-bulk-nodes N\n"
              << "  --rate-limit-rps N --rate-limit-burst N\n"
              << "  --max-request-bytes N --socket-timeout-seconds N\n"
              << "  --vector-index auto|flat|kdtree|faiss\n"
              << "  --api-key KEY --trust-proxy --behind-tls-proxy\n"
              << "  --require-forwarded-https --allow-insecure-public-bind\n"
              << "  --no-shutdown-checkpoint\n";
    return 2;
  }
  std::signal(SIGINT, on_sig); std::signal(SIGTERM, on_sig);
#ifdef SIGPIPE
  // A client may disconnect before reading the response. Ignore SIGPIPE so one
  // reset connection cannot terminate the entire database server.
  std::signal(SIGPIPE, SIG_IGN);
#endif
  const fs::path dir = argv[1];
  const uint32_t dim = static_cast<uint32_t>(std::stoul(argv[2]));
  const int port = std::stoi(argv[3]);
  DBOptions opt; opt.dimension = dim; opt.create_if_missing = true; opt.vector_index_kind = VectorIndexKind::Auto;
  std::string api_key;
  if (const char* env_key = std::getenv("GRAPHENEDB_API_KEY")) api_key = env_key;
  std::string bind_address = "127.0.0.1";
  size_t worker_count = std::clamp<size_t>(std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4, 4, 32);
  size_t queue_capacity = 1024;
  size_t max_bulk_nodes = 10000;
  double rate_limit_rps = 200.0;
  double rate_limit_burst = 400.0;
  size_t max_request_bytes = 4 * 1024 * 1024;
  int socket_timeout_seconds = 30;
  uint64_t expected_max_nodes = 0;
  bool trust_proxy = false;
  bool behind_tls_proxy = false;
  bool require_forwarded_https = false;
  bool allow_insecure_public_bind = false;
  bool shutdown_checkpoint = true;
  for (int i = 4; i < argc; ++i) {
    std::string f = argv[i];
    if (f == "--bind-address" && i + 1 < argc) bind_address = argv[++i];
    else if (f == "--physical-lattice-primary") { opt.require_lattice = true; opt.physical_lattice_storage = true; opt.physical_lattice_primary = true; }
    else if (f == "--physical-lattice-radius" && i + 1 < argc) opt.physical_lattice_radius = static_cast<uint32_t>(std::stoul(argv[++i]));
    else if (f == "--expected-max-nodes" && i + 1 < argc) expected_max_nodes = std::stoull(argv[++i]);
    else if (f == "--wal-rotate-bytes" && i + 1 < argc) opt.wal_rotate_bytes = std::stoull(argv[++i]);
    else if (f == "--vector-index" && i + 1 < argc) {
      if (!parse_vector_index_kind(argv[++i], &opt.vector_index_kind)) {
        std::cerr << "invalid --vector-index; expected auto|flat|kdtree|faiss\n";
        return 2;
      }
    }
    else if (f == "--api-key" && i + 1 < argc) api_key = argv[++i];
    else if (f == "--workers" && i + 1 < argc) worker_count = std::max<size_t>(1, std::stoull(argv[++i]));
    else if (f == "--queue-capacity" && i + 1 < argc) queue_capacity = std::max<size_t>(1, std::stoull(argv[++i]));
    else if (f == "--max-bulk-nodes" && i + 1 < argc) max_bulk_nodes = std::clamp<size_t>(std::stoull(argv[++i]), 1, 50000);
    else if (f == "--rate-limit-rps" && i + 1 < argc) rate_limit_rps = std::stod(argv[++i]);
    else if (f == "--rate-limit-burst" && i + 1 < argc) rate_limit_burst = std::stod(argv[++i]);
    else if (f == "--max-request-bytes" && i + 1 < argc) max_request_bytes = std::max<size_t>(1024, std::stoull(argv[++i]));
    else if (f == "--socket-timeout-seconds" && i + 1 < argc) socket_timeout_seconds = std::max(1, std::stoi(argv[++i]));
    else if (f == "--trust-proxy") trust_proxy = true;
    else if (f == "--behind-tls-proxy") behind_tls_proxy = true;
    else if (f == "--require-forwarded-https") { behind_tls_proxy = true; require_forwarded_https = true; }
    else if (f == "--allow-insecure-public-bind") allow_insecure_public_bind = true;
    else if (f == "--no-shutdown-checkpoint") shutdown_checkpoint = false;
    else { std::cerr << "unknown or incomplete option: " << f << "\n"; return 2; }
  }
  if (!loopback_address(bind_address) && !allow_insecure_public_bind) {
    if (api_key.empty()) {
      std::cerr << "refusing non-loopback bind without GRAPHENEDB_API_KEY/--api-key\n";
      return 2;
    }
    if (!behind_tls_proxy) {
      std::cerr << "refusing non-loopback bind without --behind-tls-proxy; use a TLS reverse proxy or explicitly pass --allow-insecure-public-bind\n";
      return 2;
    }
  }
  if (trust_proxy && !behind_tls_proxy) {
    std::cerr << "--trust-proxy requires --behind-tls-proxy\n";
    return 2;
  }
  if (opt.physical_lattice_primary) {
    const uint64_t capacity = physical_capacity(opt.physical_lattice_radius);
    if (expected_max_nodes > capacity) {
      std::cerr << "physical lattice radius " << opt.physical_lattice_radius << " has capacity " << capacity
                << ", below --expected-max-nodes " << expected_max_nodes
                << "; minimum radius is " << minimum_radius_for_nodes(expected_max_nodes) << "\n";
      return 2;
    }
  }
  GrapheneDB db;
  auto st = db.open(dir, opt);
  if (!st) { std::cerr << st.message << "\n"; return 1; }

  int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) { perror("socket"); return 1; }
  int yes = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bind_address.c_str(), &addr.sin_addr) != 1) {
    std::cerr << "invalid --bind-address: " << bind_address << "\n"; return 2;
  }
  if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) { perror("bind"); return 1; }
  const int listen_backlog = static_cast<int>(std::min<size_t>(queue_capacity, 4096));
  if (listen(server_fd, std::max(16, listen_backlog)) < 0) { perror("listen"); return 1; }
  g_listen_fd = server_fd;
  const uint64_t lattice_capacity = opt.physical_lattice_primary ? physical_capacity(opt.physical_lattice_radius) : 0;
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"info\",\"event\":\"server_started\",\"bind_address\":\"" << json_escape(bind_address)
        << "\",\"port\":" << port
        << ",\"workers\":" << worker_count
        << ",\"queue_capacity\":" << queue_capacity
        << ",\"max_bulk_nodes\":" << max_bulk_nodes
        << ",\"rate_limit_rps\":" << rate_limit_rps
        << ",\"rate_limit_burst\":" << rate_limit_burst
        << ",\"max_request_bytes\":" << max_request_bytes
        << ",\"shutdown_checkpoint\":" << (shutdown_checkpoint ? "true" : "false")
        << ",\"physical_lattice_radius\":" << opt.physical_lattice_radius
        << ",\"physical_lattice_capacity\":" << lattice_capacity << "}";
    emit_structured_log(log.str());
  }

  ServerMetrics metrics;
  graphenedb::server::TokenBucketRateLimiter rate_limiter(rate_limit_rps, rate_limit_burst);
  std::atomic<uint64_t> request_sequence{1};
  std::atomic<uint32_t> next_lattice_slot{static_cast<uint32_t>(db.node_count())};
  std::mutex idempotency_mutex;
  auto handle_client = [&](ConnectionTask task) {
    const int fd = task.fd;
    metrics.requests.fetch_add(1, std::memory_order_relaxed);
    metrics.active.fetch_add(1, std::memory_order_relaxed);
    const auto request_started = std::chrono::steady_clock::now();
    const std::string request_id = std::to_string(::getpid()) + "-" + std::to_string(request_sequence.fetch_add(1, std::memory_order_relaxed));
    timeval socket_timeout{}; socket_timeout.tv_sec = socket_timeout_seconds; socket_timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &socket_timeout, sizeof(socket_timeout));
    std::string req;
    char buf[8192];
    size_t content_length = 0;
    size_t header_end = std::string::npos;
    bool too_large = false;
    bool read_timeout = false;
    bool malformed_content_length = false;
    bool unsupported_transfer_encoding = false;
    bool content_length_present = false;
    while (true) {
      ssize_t n = ::read(fd, buf, sizeof(buf));
      if (n < 0 && errno == EINTR) continue;
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) { read_timeout = true; break; }
      if (n <= 0) break;
      req.append(buf, static_cast<size_t>(n));
      if (req.size() > max_request_bytes) { too_large = true; break; }
      header_end = req.find("\r\n\r\n");
      if (header_end != std::string::npos) {
        std::string headers = req.substr(0, header_end + 4);
        const std::string transfer_encoding = lower_copy(header_value(headers, "transfer-encoding"));
        if (!transfer_encoding.empty() && transfer_encoding != "identity") {
          unsupported_transfer_encoding = true;
          break;
        }
        const auto cl = parse_content_length(headers);
        content_length_present = cl.present;
        if (!cl.valid) {
          malformed_content_length = true;
          break;
        }
        content_length = cl.value;
        if (header_end + 4 + content_length > max_request_bytes) { too_large = true; break; }
        const size_t have_body = req.size() - (header_end + 4);
        if (!content_length_present || have_body >= content_length) break;
      }
    }
    if (req.empty() && !too_large && !read_timeout) {
      ::close(fd);
      metrics.active.fetch_sub(1, std::memory_order_relaxed);
      metrics.completed.fetch_add(1, std::memory_order_relaxed);
      return;
    }
    auto first_end = req.find("\r\n");
    auto bp = req.find("\r\n\r\n");
    std::string first = first_end == std::string::npos ? std::string{} : req.substr(0, first_end);
    std::string header_text = bp == std::string::npos ? std::string{} : req.substr(0, bp + 4);
    std::istringstream fsline(first); std::string method, path, http_version; fsline >> method >> path >> http_version;
    std::string client_ip = task.peer_ip;
    if (trust_proxy) {
      const std::string forwarded = first_forwarded_ip(header_text);
      if (!forwarded.empty()) client_ip = forwarded;
    }
    const bool public_probe = path == "/v1/health" || path == "/v1/ready" || path == "/v1/version";
    const bool authorized = public_probe || authorized_request(header_text, api_key);
    const bool forwarded_https_ok = !require_forwarded_https || public_probe || lower_copy(header_value(header_text, "x-forwarded-proto")) == "https";
    const bool rate_allowed = public_probe || rate_limiter.allow(client_ip);
    const std::string idempotency_key = header_value(header_text, "idempotency-key");
    std::string body;
    if (bp != std::string::npos) {
      body = req.substr(bp + 4);
      if (content_length > 0 && body.size() > content_length) body.resize(content_length);
    }
    metrics.request_bytes.fetch_add(req.size(), std::memory_order_relaxed);
    std::string out;
    std::string response_content_type = "application/json";
    int retry_after = 0;
    int code = 200;
    try {
      if (too_large) {
        code = 413; out = "{\"error\":\"request_too_large\",\"max_request_bytes\":" + std::to_string(max_request_bytes) + "}";
      } else if (read_timeout) {
        code = 408; out = "{\"error\":\"request_timeout\"}";
      } else if (first.empty() || method.empty() || path.empty() || http_version.rfind("HTTP/1.", 0) != 0) {
        code = 400; out = "{\"error\":\"malformed_request_line\"}";
      } else if (malformed_content_length) {
        code = 400; out = "{\"error\":\"invalid_content_length\"}";
      } else if (unsupported_transfer_encoding) {
        code = 501; out = "{\"error\":\"transfer_encoding_not_supported\",\"supported\":\"content-length\"}";
      } else if (endpoint_requires_json_body(method, path) && !content_length_present) {
        code = 411; out = "{\"error\":\"content_length_required\"}";
      } else if (endpoint_requires_json_body(method, path) && !application_json_content_type(header_text)) {
        code = 415; out = "{\"error\":\"application_json_required\"}";
      } else if (!rate_allowed) {
        metrics.rate_limited.fetch_add(1, std::memory_order_relaxed);
        code = 429; retry_after = 1; out = "{\"error\":\"rate_limited\"}";
      } else if (!forwarded_https_ok) {
        code = 403; out = "{\"error\":\"https_required_by_proxy_policy\"}";
      } else if (!authorized) {
        metrics.auth_failures.fetch_add(1, std::memory_order_relaxed);
        code = 401; out = "{\"error\":\"unauthorized\"}";
      } else if (method == "GET" && path == "/v1/health") {
        out = "{\"status\":\"ok\",\"engine\":\"physical-hex-lattice\",\"server_version\":\"" + std::string(kServerVersion) + "\",\"api_version\":" + std::to_string(kApiVersion) + "}";
      } else if (method == "GET" && path == "/v1/version") {
        std::ostringstream js;
        js << "{\"server_version\":\"" << kServerVersion
           << "\",\"api_version\":" << kApiVersion
           << ",\"storage_formats\":{\"manifest\":" << kManifestFormatVersion
           << ",\"storage\":" << kStorageFormatVersion
           << ",\"wal_frame\":" << kWalFrameFormatVersion
           << ",\"lattice\":" << kLatticeFormatVersion
           << ",\"extraction\":" << kExtractionFormatVersion
           << "},\"features\":[\"physical_lattice_primary\",\"vector_search\",\"lattice_search\",\"checkpoint\",\"backup\",\"validation\",\"idempotent_node_writes\",\"atomic_extraction_ingest\",\"bounded_dialectic_reasoning\",\"read_only_hypokosh\",\"governed_outcome_learning\"]}";
        out = js.str();
      } else if (method == "GET" && path == "/v1/ready") {
        std::string report; auto ist = db.inspect(&report);
        const uint64_t nodes = db.node_count();
        const bool capacity_ok = lattice_capacity == 0 || nodes < lattice_capacity;
        if (g_stop) {
          code = 503;
          out = "{\"status\":\"not_ready\",\"reason\":\"shutting_down\"}";
        } else if (!ist || !db.is_open() || !capacity_ok) {
          code = 503;
          out = "{\"status\":\"not_ready\",\"reason\":\"" + std::string(capacity_ok ? "storage_not_ready" : "physical_lattice_capacity_exhausted") + "\"}";
        } else {
          const double utilization = lattice_capacity ? static_cast<double>(nodes) / static_cast<double>(lattice_capacity) : 0.0;
          std::ostringstream js;
          js << "{\"status\":\"ready\",\"node_count\":" << nodes
             << ",\"physical_lattice_capacity\":" << lattice_capacity
             << ",\"physical_lattice_utilization\":" << utilization << "}";
          out = js.str();
        }
      } else if (method == "GET" && path == "/v1/metrics") {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - metrics.started).count();
        const uint64_t duration_count = metrics.completed.load();
        const double avg_duration_ms = duration_count ? (static_cast<double>(metrics.duration_us_sum.load()) / duration_count / 1000.0) : 0.0;
        std::ostringstream js;
        js << "{\"uptime_seconds\":" << uptime
           << ",\"requests\":" << metrics.requests.load()
           << ",\"completed\":" << metrics.completed.load()
           << ",\"active\":" << metrics.active.load()
           << ",\"queue_depth\":" << metrics.queue_depth.load()
           << ",\"queue_rejected\":" << metrics.queue_rejected.load()
           << ",\"rate_limited\":" << metrics.rate_limited.load()
           << ",\"auth_failures\":" << metrics.auth_failures.load()
           << ",\"request_bytes\":" << metrics.request_bytes.load()
           << ",\"response_bytes\":" << metrics.response_bytes.load()
           << ",\"request_duration_avg_ms\":" << avg_duration_ms
           << ",\"request_duration_max_ms\":" << (metrics.duration_us_max.load()/1000.0)
           << ",\"status_2xx\":" << metrics.status_2xx.load()
           << ",\"status_4xx\":" << metrics.status_4xx.load()
           << ",\"status_5xx\":" << metrics.status_5xx.load()
           << ",\"node_inserts\":" << metrics.node_inserts.load()
           << ",\"edge_inserts\":" << metrics.edge_inserts.load()
           << ",\"vector_searches\":" << metrics.vector_searches.load()
           << ",\"lattice_searches\":" << metrics.lattice_searches.load()
           << ",\"validation_calls\":" << metrics.validation_calls.load()
           << ",\"checkpoint_calls\":" << metrics.checkpoint_calls.load()
           << ",\"backup_calls\":" << metrics.backup_calls.load()
           << ",\"errors\":" << metrics.errors.load()
           << ",\"node_count\":" << db.node_count()
           << ",\"edge_count\":" << db.edge_count()
           << ",\"physical_lattice_capacity\":" << lattice_capacity << "}";
        out = js.str();
      } else if (method == "GET" && path == "/v1/metrics/prometheus") {
        response_content_type = "text/plain; version=0.0.4";
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - metrics.started).count();
        std::ostringstream pm;
        pm << "# TYPE graphenedb_uptime_seconds gauge\n"
           << "graphenedb_uptime_seconds " << uptime << "\n"
           << "# TYPE graphenedb_http_requests_total counter\n"
           << "graphenedb_http_requests_total " << metrics.requests.load() << "\n"
           << "graphenedb_http_requests_completed_total " << metrics.completed.load() << "\n"
           << "graphenedb_http_active_requests " << metrics.active.load() << "\n"
           << "graphenedb_http_queue_depth " << metrics.queue_depth.load() << "\n"
           << "graphenedb_http_queue_rejected_total " << metrics.queue_rejected.load() << "\n"
           << "graphenedb_http_rate_limited_total " << metrics.rate_limited.load() << "\n"
           << "graphenedb_http_auth_failures_total " << metrics.auth_failures.load() << "\n"
           << "graphenedb_http_request_duration_seconds_sum " << (metrics.duration_us_sum.load()/1000000.0) << "\n"
           << "graphenedb_http_request_duration_seconds_count " << metrics.completed.load() << "\n"
           << "graphenedb_nodes " << db.node_count() << "\n"
           << "graphenedb_edges " << db.edge_count() << "\n"
           << "graphenedb_physical_lattice_capacity " << lattice_capacity << "\n";
        out = pm.str();
      } else if (method == "GET" && path == "/v1/admin/capacity") {
        const uint64_t nodes = db.node_count();
        const uint64_t remaining = lattice_capacity > nodes ? lattice_capacity - nodes : 0;
        const double utilization = lattice_capacity ? static_cast<double>(nodes) / lattice_capacity : 0.0;
        std::ostringstream js;
        js << "{\"physical_lattice_primary\":" << (opt.physical_lattice_primary ? "true" : "false")
           << ",\"radius\":" << opt.physical_lattice_radius
           << ",\"capacity\":" << lattice_capacity
           << ",\"nodes\":" << nodes
           << ",\"remaining\":" << remaining
           << ",\"utilization\":" << utilization
           << ",\"recommended_radius_for_expected_max_nodes\":" << minimum_radius_for_nodes(expected_max_nodes ? expected_max_nodes : nodes) << "}";
        out = js.str();
      } else if (method == "POST" && path == "/v1/admin/validate") {
        metrics.validation_calls.fetch_add(1, std::memory_order_relaxed);
        std::string report; auto vst = db.validate(&report);
        if (!vst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(vst.message) + "\",\"report\":\"" + json_escape(report) + "\"}"; }
        else out = "{\"ok\":true,\"report\":\"" + json_escape(report) + "\"}";
      } else if (method == "POST" && (path == "/v1/admin/compact" || path == "/v1/admin/checkpoint")) {
        metrics.checkpoint_calls.fetch_add(1, std::memory_order_relaxed);
        auto cst = db.compact();
        if (!cst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(cst.message) + "\"}"; }
        else out = "{\"ok\":true}";
      } else if (method == "GET" && path == "/v1/admin/inspect") {
        std::string report; auto ist = db.inspect(&report);
        if (!ist) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(ist.message) + "\"}"; }
        else out = "{\"ok\":true,\"report\":\"" + json_escape(report) + "\"}";
      } else if (method == "POST" && path == "/v1/admin/backup") {
        metrics.backup_calls.fetch_add(1, std::memory_order_relaxed);
        std::string dest = json_value(body, "destination");
        if (dest.empty()) { code = 400; out = "{\"error\":\"destination is required\"}"; }
        else { auto bst = db.backup(dest); if (!bst) { code = 500; out = "{\"ok\":false,\"error\":\"" + json_escape(bst.message) + "\"}"; } else out = "{\"ok\":true,\"destination\":\"" + json_escape(dest) + "\"}"; }
      } else if (method == "POST" && path == "/v1/nodes/bulk") {
        uint32_t count = json_u32(body, "count", 0);
        const uint64_t remaining_capacity = lattice_capacity > db.node_count() ? lattice_capacity - db.node_count() : 0;
        if (lattice_capacity && count > remaining_capacity) { code = 507; out = "{\"error\":\"physical_lattice_capacity_exceeded\",\"remaining\":" + std::to_string(remaining_capacity) + "}"; }
        else
        if (count == 0 || count > max_bulk_nodes) { code = 400; out = "{\"error\":\"count_out_of_range\",\"minimum\":1,\"maximum\":" + std::to_string(max_bulk_nodes) + "}"; }
        else {
          std::string prefix = json_value(body, "prefix");
          if (prefix.empty()) prefix = "bulk node";
          if (prefix.size() > kMaxBulkPrefixBytes) {
            code = 400;
            out = "{\"error\":\"bulk_prefix_too_large\",\"max_bytes\":" + std::to_string(kMaxBulkPrefixBytes) + "}";
          }
          std::string source = json_value(body, "source");
          if (source.empty()) source = "bulk";
          if (code != 200) {
            // Validation has already produced a response.
          } else {
          BatchInput batch;
          batch.nodes.reserve(count);
          const uint32_t base_slot = next_lattice_slot.fetch_add(count, std::memory_order_relaxed);
          for (uint32_t i = 0; i < count; ++i) {
            std::string text = prefix + " " + std::to_string(i);
            NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim); ni.metadata["source"] = source;
            ni.lattice = spiral_coord(base_slot + i);
            batch.nodes.push_back(std::move(ni));
          }
          auto t0 = std::chrono::steady_clock::now();
          BatchResult batch_result;
          st = db.put_batch(batch, &batch_result);
          if (!st) {
            code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : 500;
            out = "{\"error\":\"" + json_escape(st.message) + "\",\"inserted\":0,\"atomic\":true}";
          } else {
            const uint32_t inserted = static_cast<uint32_t>(batch_result.node_ids.size());
            const uint32_t first_id = batch_result.node_ids.empty() ? UINT32_MAX : batch_result.node_ids.front();
            const uint32_t last_id = batch_result.node_ids.empty() ? UINT32_MAX : batch_result.node_ids.back();
            metrics.node_inserts.fetch_add(inserted, std::memory_order_relaxed);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            code = 201; out = "{\"inserted\":" + std::to_string(inserted) + ",\"first_id\":" + std::to_string(first_id) + ",\"last_id\":" + std::to_string(last_id) + ",\"elapsed_ms\":" + std::to_string(ms) + ",\"atomic\":true}";
          }
          }
        }
      } else if (method == "POST" && path == "/v1/extractions") {
        ExtractionInput extraction;
        std::string parse_error;
        if (!parse_extraction_request(body, dim, max_bulk_nodes, &extraction,
                                      &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_extraction\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else {
          if (extraction.place_missing_lattice && !extraction.nodes.empty()) {
            const uint32_t base_slot = next_lattice_slot.fetch_add(
                static_cast<uint32_t>(extraction.nodes.size()),
                std::memory_order_relaxed);
            for (size_t index = 0; index < extraction.nodes.size(); ++index) {
              if (!extraction.nodes[index].lattice) {
                extraction.nodes[index].lattice =
                    spiral_coord(base_slot + static_cast<uint32_t>(index));
              }
            }
            extraction.place_missing_lattice = false;
          }
          ExtractionResult result;
          const auto started = std::chrono::steady_clock::now();
          st = db.put_extraction(extraction, &result);
          if (!st) {
            if (st.message.find("capacity") != std::string::npos ||
                st.message.find("physical lattice") != std::string::npos) {
              code = 507;
            } else if (st.message.find("idempotency conflict") !=
                       std::string::npos) {
              code = 409;
            } else {
              code = http_status_for(st);
            }
            out = "{\"error\":\"extraction_failed\",\"detail\":\"" +
                  json_escape(st.message) + "\",\"atomic\":true,"
                  "\"inserted_nodes\":0,\"inserted_edges\":0}";
          } else {
            metrics.node_inserts.fetch_add(result.inserted_node_ids.size(),
                                           std::memory_order_relaxed);
            metrics.edge_inserts.fetch_add(result.inserted_edge_ids.size(),
                                           std::memory_order_relaxed);
            const bool replay = result.inserted_node_ids.empty() &&
                                result.inserted_edge_ids.empty();
            code = replay ? 200 : 201;
            out = extraction_result_json(result, extraction.idempotent);
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                            started)
                                     .count();
            out.pop_back();
            out += ",\"elapsed_ms\":" + std::to_string(elapsed) + "}";
          }
        }
      } else if (method == "POST" && path == "/v1/facts") {
        if (idempotency_key.size() > 128) { code = 400; out = "{\"error\":\"idempotency_key_too_long\",\"max_length\":128}"; }
        else if (lattice_capacity && db.node_count() >= lattice_capacity && idempotency_key.empty()) { code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; }
        else {
        std::string text = json_value(body, "text");
        if (text.empty()) text = json_value(body, "content");
        if (text.empty()) { code = 400; out = "{\"error\":\"text/content is required\"}"; }
        else if (text.size() > kMaxNodeTextBytes) { code = 400; out = "{\"error\":\"text_too_large\",\"max_bytes\":" + std::to_string(kMaxNodeTextBytes) + "}"; }
        else {
          std::unique_lock<std::mutex> idempotency_lock;
          bool idempotency_handled = false;
          const std::string scoped_idempotency_key = path + ":" + idempotency_key;
          if (!idempotency_key.empty()) {
            idempotency_lock = std::unique_lock<std::mutex>(idempotency_mutex);
            const auto existing = db.metadata_search("_idempotency_key", scoped_idempotency_key);
            if (!existing.empty()) {
              const auto prior = db.get_node(existing.front());
              if (!prior || prior->content != text) {
                code = 409; out = "{\"error\":\"idempotency_conflict\"}"; idempotency_handled = true;
              } else {
                code = 200; out = "{\"id\":" + std::to_string(existing.front()) + ",\"type\":\"temporal_fact\",\"idempotent_replay\":true}"; idempotency_handled = true;
              }
            }
          }
          if (!idempotency_handled && lattice_capacity && db.node_count() >= lattice_capacity) {
            code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; idempotency_handled = true;
          }
          if (!idempotency_handled) {
          NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim);
          ni.metadata["source"] = json_value(body, "source").empty() ? "server" : json_value(body, "source");
          ni.metadata["graphene_type"] = "temporal_fact";
          if (!idempotency_key.empty()) ni.metadata["_idempotency_key"] = scoped_idempotency_key;
          for (const char* k : {"documented_at","valid_from","valid_until","confidence","fact_type","account","service","incident","repo","placement_policy"}) { std::string v = json_value(body, k); if (!v.empty()) ni.metadata[k] = v; }
          std::string placement_policy = json_value(body, "placement_policy");
          if (placement_policy == "semantic_causal_temporal" || placement_policy == "semantic") ni.lattice = choose_semantic_causal_temporal_cell(db, text, ni.vector, ni.metadata, next_lattice_slot);
          else ni.lattice = spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          uint32_t id = 0; st = put_node_with_lattice_retry(db, ni, &id, next_lattice_slot);
          if (!st) { code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
          else { metrics.node_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(id) + ",\"type\":\"temporal_fact\",\"q\":" + std::to_string(ni.lattice->q) + ",\"r\":" + std::to_string(ni.lattice->r) + ",\"layer\":" + std::to_string(ni.lattice->layer) + "}"; }
          }
        }
        }
      } else if (method == "POST" && path == "/v1/nodes") {
        if (idempotency_key.size() > 128) { code = 400; out = "{\"error\":\"idempotency_key_too_long\",\"max_length\":128}"; }
        else if (lattice_capacity && db.node_count() >= lattice_capacity && idempotency_key.empty()) { code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; }
        else {
        std::string text = json_value(body, "text");
        if (text.empty()) text = json_value(body, "content");
        if (text.empty()) { code = 400; out = "{\"error\":\"text/content is required\"}"; }
        else if (text.size() > kMaxNodeTextBytes) { code = 400; out = "{\"error\":\"text_too_large\",\"max_bytes\":" + std::to_string(kMaxNodeTextBytes) + "}"; }
        else {
          std::unique_lock<std::mutex> idempotency_lock;
          bool idempotency_handled = false;
          const std::string scoped_idempotency_key = path + ":" + idempotency_key;
          if (!idempotency_key.empty()) {
            idempotency_lock = std::unique_lock<std::mutex>(idempotency_mutex);
            const auto existing = db.metadata_search("_idempotency_key", scoped_idempotency_key);
            if (!existing.empty()) {
              const auto prior = db.get_node(existing.front());
              if (!prior || prior->content != text) {
                code = 409; out = "{\"error\":\"idempotency_conflict\"}"; idempotency_handled = true;
              } else {
                code = 200; out = "{\"id\":" + std::to_string(existing.front()) + ",\"idempotent_replay\":true}"; idempotency_handled = true;
              }
            }
          }
          if (!idempotency_handled && lattice_capacity && db.node_count() >= lattice_capacity) {
            code = 507; out = "{\"error\":\"physical_lattice_capacity_exhausted\"}"; idempotency_handled = true;
          }
          if (!idempotency_handled) {
          NodeInput ni; ni.content = text; ni.vector = embed_text(text, dim);
          ni.metadata["source"] = json_value(body, "source").empty() ? "server" : json_value(body, "source");
          if (!idempotency_key.empty()) ni.metadata["_idempotency_key"] = scoped_idempotency_key;
          for (const char* k : {"documented_at","valid_from","valid_until","confidence","fact_type","account","service","incident","repo","placement_policy"}) { std::string v = json_value(body, k); if (!v.empty()) ni.metadata[k] = v; }
          std::string placement_policy = json_value(body, "placement_policy");
          if (placement_policy == "semantic_causal_temporal" || placement_policy == "semantic") ni.lattice = choose_semantic_causal_temporal_cell(db, text, ni.vector, ni.metadata, next_lattice_slot);
          else ni.lattice = spiral_coord(next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          uint32_t id = 0; st = put_node_with_lattice_retry(db, ni, &id, next_lattice_slot);
          if (!st) { code = st.message.find("capacity") != std::string::npos || st.message.find("physical lattice") != std::string::npos ? 507 : http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
          else { metrics.node_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(id) + ",\"q\":" + std::to_string(ni.lattice->q) + ",\"r\":" + std::to_string(ni.lattice->r) + ",\"layer\":" + std::to_string(ni.lattice->layer) + "}"; }
          }
        }
        }
      } else if (method == "POST" && path == "/v1/edges") {
        EdgeInput ei; ei.from = json_u32(body, "from", UINT32_MAX); ei.to = json_u32(body, "to", UINT32_MAX); ei.confidence = json_double(body, "confidence", 0.9);
        std::string role = json_value(body, "role");
        std::string origin = json_value(body, "origin");
        ei.role = parse_role(role);
        ei.origin = parse_origin(origin);
        for (const char* k : {"source_id","span","observed_at","derived_from","promotion_status","evidence_ref","reinforcement_count","salience_boost"}) { std::string v = json_value(body, k); if (!v.empty()) ei.metadata[k] = v; }
        uint32_t eid = 0; st = db.put_edge(ei, &eid);
        if (!st) { code = http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
        else { metrics.edge_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(eid) + "}"; }
      } else if (method == "POST" && path == "/v1/edges/provenance") {
        EdgeInput ei; ei.from = json_u32(body, "from", UINT32_MAX); ei.to = json_u32(body, "to", UINT32_MAX); ei.confidence = json_double(body, "confidence", 0.9);
        ei.role = parse_role(json_value(body, "role"));
        ei.origin = parse_origin(json_value(body, "origin"));
        for (const char* k : {"source_id","span","observed_at","derived_from","promotion_status","evidence_ref","reinforcement_count","salience_boost"}) { std::string v = json_value(body, k); if (!v.empty()) ei.metadata[k] = v; }
        uint32_t eid = 0; st = db.put_edge(ei, &eid);
        if (!st) { code = http_status_for(st); out = "{\"error\":\"" + json_escape(st.message) + "\"}"; }
        else { metrics.edge_inserts.fetch_add(1, std::memory_order_relaxed); code = 201; out = "{\"id\":" + std::to_string(eid) + ",\"origin\":\"" + origin_name(ei.origin) + "\",\"role\":\"" + role_name(ei.role) + "\"}"; }
      } else if (method == "POST" && (path == "/v1/retrieve/bundle" || path == "/v1/retrieve/explain")) {
        uint32_t anchor = json_u32(body, "anchor", json_u32(body, "anchor_node_id", 0));
        uint32_t hops = std::clamp<uint32_t>(json_u32(body, "max_hops", json_u32(body, "hops", 3)), 1, kMaxBundleHops);
        std::string mode = json_value(body, "mode"); if (mode.empty()) mode = "balanced";
        std::string as_of = json_value(body, "as_of"); if (as_of.empty()) as_of = json_value(body, "at");
        out = build_bundle_json(db, anchor, hops, mode, as_of);
      } else if (method == "POST" && path == "/v1/reason/dialectic") {
        const std::string query = json_value(body, "query");
        QueryMode mode = QueryMode::Balanced;
        const std::string requested_mode = json_value(body, "mode");
        if (query.empty()) {
          code = 400;
          out = "{\"error\":\"query is required\"}";
        } else if (query.size() > kMaxNodeTextBytes) {
          code = 400;
          out = "{\"error\":\"query_too_large\",\"max_bytes\":" +
                std::to_string(kMaxNodeTextBytes) + "}";
        } else if (!parse_query_mode(requested_mode, &mode)) {
          code = 400;
          out = "{\"error\":\"mode must be empirical, balanced, or theoretical\"}";
        } else {
          DialecticOptions reasoning;
          reasoning.mode = mode;
          reasoning.semantic_candidates =
              std::clamp<uint32_t>(json_u32(body, "semantic_candidates", 12),
                                   1, 64);
          reasoning.max_hops =
              std::clamp<uint32_t>(json_u32(body, "max_hops", 6), 1,
                                   kMaxDialecticHops);
          reasoning.max_paths =
              std::clamp<uint32_t>(json_u32(body, "max_paths", 32), 1,
                                   kMaxDialecticPaths);
          reasoning.max_paths_per_root = std::clamp<uint32_t>(
              json_u32(body, "max_paths_per_root", 8), 1, 64);
          reasoning.max_visited_states = std::clamp<uint32_t>(
              json_u32(body, "max_visited_states", 20000), 1,
              kMaxDialecticVisitedStates);
          reasoning.max_opposition_rounds = std::clamp<uint32_t>(
              json_u32(body, "max_opposition_rounds", 1), 0, 2);
          reasoning.minimum_confidence = std::clamp(
              json_double(body, "minimum_confidence", 0.45), 0.0, 1.0);
          reasoning.reexpansion_threshold = std::clamp(
              json_double(body, "reexpansion_threshold", 0.25), 0.0, 1.0);
          reasoning.as_of = json_value(body, "as_of");
          DialecticEngine engine(db);
          const DialecticResult result =
              engine.reason(embed_text(query, dim),
                            json_u64(body, "signature", 0), reasoning);
          out = dialectic_json(result);
        }
      } else if (method == "POST" && path == "/v1/reason/hypokosh") {
        HypoKoshRequest request;
        std::string parse_error;
        if (!parse_hypokosh_request(body, &request, &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_hypokosh_request\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else {
          OutcomeLearningEngine learner(db);
          std::optional<PolicyState> active;
          if (request.use_active_policy) {
            active = learner.current_policy(request.tenant_id);
            if (!active) {
              code = 404;
              out = "{\"error\":\"active_policy_not_found\"}";
            } else {
              const QueryMode mode = request.options.mode;
              const std::string as_of = request.options.as_of;
              request.options =
                  dialectic_options_for_policy(active->policy);
              request.options.mode = mode;
              request.options.as_of = as_of;
            }
          }
          if (code == 200) {
            const HypothesisSet hypotheses = HypoKoshEngine(db).propose(
                embed_text(request.query, dim), request.signature,
                request.options, request.max_hypotheses);
            out = hypokosh_json(hypotheses, active);
          }
        }
      } else if (method == "POST" && path == "/v1/learning/episodes") {
        LearningEpisodeInput episode;
        std::string parse_error;
        if (!parse_learning_episode_request(body, &episode, &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_learning_episode\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else if (lattice_capacity &&
                   db.node_count() >= lattice_capacity) {
          code = 507;
          out = "{\"error\":\"physical_lattice_capacity_exhausted\"}";
        } else {
          episode.vector = embed_text(episode.query, dim);
          episode.place_missing_lattice = false;
          episode.lattice = spiral_coord(
              next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          LearningEpisodeResult result;
          st = OutcomeLearningEngine(db).record_episode(episode, &result);
          if (!st) {
            code = st.message.find("idempotency conflict") != std::string::npos
                       ? 409
                       : http_status_for(st);
            out = "{\"error\":\"learning_episode_failed\",\"detail\":\"" +
                  json_escape(st.message) + "\"}";
          } else {
            metrics.node_inserts.fetch_add(
                result.idempotent_replay ? 0 : 1,
                std::memory_order_relaxed);
            code = result.idempotent_replay ? 200 : 201;
            std::ostringstream response_body;
            response_body
                << "{\"node_id\":" << result.node_id
                << ",\"idempotent_replay\":"
                << (result.idempotent_replay ? "true" : "false")
                << ",\"training_eligible\":"
                << (result.training_eligible ? "true" : "false")
                << ",\"safety_negative\":"
                << (result.safety_negative ? "true" : "false")
                << ",\"utility\":" << result.utility
                << ",\"utility_class\":\""
                << data_utility_class_name(result.utility_class) << "\"}";
            out = response_body.str();
          }
        }
      } else if (method == "POST" &&
                 path == "/v1/learning/policies/evaluate") {
        std::string tenant_id;
        RetrievalPolicy baseline;
        PolicyLearningOptions learning_options;
        std::string parse_error;
        if (!parse_policy_evaluation_request(
                body, &tenant_id, &baseline, &learning_options,
                &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_policy_evaluation\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else {
          const size_t nodes_before = db.node_count();
          const PolicyEvaluationReport report =
              OutcomeLearningEngine(db).evaluate_policies(
                  tenant_id, baseline, learning_options);
          if (std::find(report.warnings.begin(), report.warnings.end(),
                        "INVALID_BASELINE_POLICY") != report.warnings.end() ||
              std::find(report.warnings.begin(), report.warnings.end(),
                        "INVALID_LEARNING_OPTIONS") !=
                  report.warnings.end()) {
            code = 400;
          }
          out = policy_evaluation_json(report);
          if (db.node_count() != nodes_before) {
            code = 500;
            out = "{\"error\":\"read_only_evaluation_wrote_data\"}";
          }
        }
      } else if (method == "POST" &&
                 path == "/v1/learning/policies/decisions") {
        PolicyDecisionInput decision_input;
        std::string parse_error;
        if (!parse_policy_decision_request(
                body, &decision_input, &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_policy_decision\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else if (lattice_capacity &&
                   db.node_count() >= lattice_capacity) {
          code = 507;
          out = "{\"error\":\"physical_lattice_capacity_exhausted\"}";
        } else {
          decision_input.vector =
              embed_text(decision_input.reason, dim);
          decision_input.place_missing_lattice = false;
          decision_input.lattice = spiral_coord(
              next_lattice_slot.fetch_add(1, std::memory_order_relaxed));
          PolicyDecisionResult result;
          st = OutcomeLearningEngine(db).record_policy_decision(
              decision_input, &result);
          if (!st) {
            code = st.message.find("idempotency conflict") != std::string::npos
                       ? 409
                       : http_status_for(st);
            out = "{\"error\":\"policy_decision_failed\",\"detail\":\"" +
                  json_escape(st.message) + "\"}";
          } else {
            metrics.node_inserts.fetch_add(
                result.idempotent_replay ? 0 : 1,
                std::memory_order_relaxed);
            code = result.idempotent_replay ? 200 : 201;
            out = policy_state_json(result.state,
                                    result.idempotent_replay);
          }
        }
      } else if (method == "GET" &&
                 (path == "/v1/learning/policies/current" ||
                  (path.rfind("/v1/learning/policies/current", 0) == 0 &&
                   path.size() >
                       std::strlen("/v1/learning/policies/current") &&
                   path[std::strlen("/v1/learning/policies/current")] ==
                       '?'))) {
        const std::string tenant_id = query_string(path, "tenant_id");
        if (tenant_id.empty() || tenant_id.size() > 256) {
          code = 400;
          out = "{\"error\":\"tenant_id query parameter is required\"}";
        } else {
          const auto current =
              OutcomeLearningEngine(db).current_policy(tenant_id);
          if (!current) {
            code = 404;
            out = "{\"error\":\"active_policy_not_found\"}";
          } else {
            out = policy_state_json(*current, false);
          }
        }
      } else if (method == "POST" &&
                 path == "/v1/learning/episodes/quarantine") {
        JsonDocumentValue quarantine;
        std::string parse_error;
        std::string tenant_id;
        std::string episode_id;
        if (!parse_json_object(body, &quarantine, &parse_error) ||
            !reject_unknown_json_fields(
                quarantine, {"tenant_id", "episode_id"}, "request",
                &parse_error) ||
            !extraction_string_field(quarantine, "tenant_id", 256, true,
                                     &tenant_id, &parse_error) ||
            !extraction_string_field(quarantine, "episode_id", 256, true,
                                     &episode_id, &parse_error)) {
          code = 400;
          out = "{\"error\":\"invalid_quarantine_request\",\"detail\":\"" +
                json_escape(parse_error) + "\"}";
        } else {
          st = OutcomeLearningEngine(db).quarantine_episode(tenant_id,
                                                            episode_id);
          if (!st) {
            code = http_status_for(st);
            out = "{\"error\":\"quarantine_failed\",\"detail\":\"" +
                  json_escape(st.message) + "\"}";
          } else {
            out = "{\"quarantined\":true,\"episode_id\":\"" +
                  json_escape(episode_id) + "\"}";
          }
        }
      } else if (method == "POST" && path == "/v1/admin/validate/provenance") {
        uint32_t missing = 0;
        uint32_t inferred_without_derived = 0;
        uint32_t reinforced_truth_risk = 0;
        uint32_t compressed_without_mechanism = 0;
        uint32_t invalid_observed_at = 0;
        uint32_t invalid_temporal_metadata = 0;
        const size_t total = db.edge_count();
        for (uint32_t eid = 0; eid < total + 1000; ++eid) {
          auto e = db.get_edge(eid); if (!e) { if (eid > total + 16) break; continue; }
          const EdgeProvenance provenance = assess_edge_provenance(*e);
          for (const auto& finding : provenance.findings) {
            if (finding.code == "MISSING_EVIDENCE") ++missing;
            else if (finding.code == "INFERRED_WITHOUT_DERIVATION") ++inferred_without_derived;
            else if (finding.code == "REINFORCED_TRUTH_PROMOTION") ++reinforced_truth_risk;
            else if (finding.code == "COMPRESSED_WITHOUT_MECHANISM") ++compressed_without_mechanism;
            else if (finding.code == "INVALID_OBSERVED_AT") ++invalid_observed_at;
            else if (finding.code == "INVALID_TEMPORAL_METADATA") ++invalid_temporal_metadata;
          }
        }
        const bool valid = missing == 0 && inferred_without_derived == 0 &&
                           reinforced_truth_risk == 0 &&
                           compressed_without_mechanism == 0 &&
                           invalid_observed_at == 0 &&
                           invalid_temporal_metadata == 0;
        out = "{\"ok\":" + std::string(valid ? "true" : "false") +
              ",\"edges_checked\":" + std::to_string(total) +
              ",\"missing_evidence\":" + std::to_string(missing) +
              ",\"inferred_without_derived_from\":" +
              std::to_string(inferred_without_derived) +
              ",\"reinforced_truth_promotion_risk\":" +
              std::to_string(reinforced_truth_risk) +
              ",\"compressed_without_mechanism\":" +
              std::to_string(compressed_without_mechanism) +
              ",\"invalid_observed_at\":" +
              std::to_string(invalid_observed_at) +
              ",\"invalid_temporal_metadata\":" +
              std::to_string(invalid_temporal_metadata) + "}";
      } else if (method == "POST" && path == "/v1/admin/lattice/quality") {
        out = lattice_quality_json(db);
      } else if (method == "POST" && path == "/v1/retrieve/temporal") {
        std::string as_of = json_value(body, "as_of"); if (as_of.empty()) as_of = json_value(body, "at");
        std::string key = json_value(body, "metadata_key");
        std::string val = json_value(body, "metadata_value");
        const uint32_t limit = std::clamp<uint32_t>(json_u32(body, "limit", 100), 1, static_cast<uint32_t>(kMaxTemporalResults));
        const uint32_t offset = json_u32(body, "offset", 0);
        std::ostringstream js; js << "{\"as_of\":\"" << json_escape(as_of) << "\",\"limit\":" << limit << ",\"offset\":" << offset << ",\"results\":[";
        bool first = true;
        uint32_t matched = 0;
        uint32_t returned = 0;
        size_t total = db.node_count();
        for (uint32_t id = 0; id < total + 1000; ++id) {
          auto n0 = db.get_node(id);
          if (!n0) { if (id > total + 16) break; continue; }
          if (!node_valid_at(*n0, as_of)) continue;
          if (!key.empty()) {
            auto it = n0->metadata.find(key);
            if (it == n0->metadata.end() || it->second != val) continue;
          }
          if (matched++ < offset) continue;
          if (returned >= limit) break;
          if (!first) js << ',';
          first = false;
          js << "{\"id\":" << id << ",\"content\":\"" << json_escape(n0->content) << "\",\"metadata\":" << metadata_json(n0->metadata) << "}";
          ++returned;
        }
        js << "],\"returned\":" << returned << ",\"next_offset\":" << (offset + returned) << "}"; out = js.str();
      } else if (method == "POST" && path == "/v1/search/hybrid") {
        metrics.vector_searches.fetch_add(1, std::memory_order_relaxed);
        std::string q = json_value(body, "query");
        if (q.empty()) {
          code = 400;
          out = "{\"error\":\"query is required\"}";
        } else {
        size_t topk = 5;
        try { auto k = json_value(body, "top_k"); if (!k.empty()) topk = static_cast<size_t>(std::stoull(k)); } catch (...) {}
        topk = std::clamp<size_t>(topk, 1, kMaxSearchTopK);
        auto hits = db.vector_search(embed_text(q, dim), topk);
        std::ostringstream js; js << "{\"results\":[";
        for (size_t i = 0; i < hits.size(); ++i) {
          auto n0 = db.get_node(hits[i].node_id);
          if (i) js << ',';
          js << "{\"id\":" << hits[i].node_id << ",\"score\":" << hits[i].score;
          if (n0) js << ",\"content\":\"" << json_escape(n0->content) << "\"";
          js << '}';
        }
        js << "]}"; out = js.str();
        }
      } else if (method == "GET" && path.rfind("/v1/nodes/", 0) == 0) {
        const std::string raw_id = path.substr(10);
        if (raw_id.empty() || !std::all_of(raw_id.begin(), raw_id.end(), [](unsigned char c){ return std::isdigit(c); })) {
          code = 400; out = "{\"error\":\"invalid_node_id\"}";
        } else {
          const auto parsed = std::stoull(raw_id);
          if (parsed > UINT32_MAX) { code = 400; out = "{\"error\":\"invalid_node_id\"}"; }
          else {
            const uint32_t id = static_cast<uint32_t>(parsed);
            auto n0 = db.get_node(id);
            if (!n0) { code = 404; out = "{\"error\":\"not found\"}"; }
            else out = "{\"id\":" + std::to_string(id) + ",\"content\":\"" + json_escape(n0->content) + "\",\"metadata\":" + metadata_json(n0->metadata) + "}";
          }
        }
      } else if (method == "GET" && path.rfind("/v1/search/lattice", 0) == 0) {
        metrics.lattice_searches.fetch_add(1, std::memory_order_relaxed);
        uint32_t id = query_u32(path, "node_id", 0);
        uint32_t hops = std::clamp<uint32_t>(query_u32(path, "hops", 2), 1, kMaxLatticeHops);
        if (!db.get_node(id)) { code = 404; out = "{\"error\":\"not found\"}"; }
        else {
        auto ns = db.lattice_neighbors(id, hops);
        std::ostringstream js; js << "{\"neighbors\":[";
        for (size_t i = 0; i < ns.size(); ++i) { if (i) js << ','; js << ns[i]; }
        js << "],\"hops\":" << hops << "}"; out = js.str();
        }
      } else {
        code = 404; out = "{\"error\":\"unknown endpoint\"}";
      }
    } catch (const std::exception& ex) {
      code = 500; out = "{\"error\":\"" + json_escape(ex.what()) + "\"}";
    }
    if (code >= 400) metrics.errors.fetch_add(1, std::memory_order_relaxed);
    if (code >= 200 && code < 300) metrics.status_2xx.fetch_add(1, std::memory_order_relaxed);
    else if (code >= 400 && code < 500) metrics.status_4xx.fetch_add(1, std::memory_order_relaxed);
    else if (code >= 500) metrics.status_5xx.fetch_add(1, std::memory_order_relaxed);
    auto resp = response(code, out, request_id, response_content_type, retry_after);
    (void)write_all(fd, resp);
    metrics.response_bytes.fetch_add(resp.size(), std::memory_order_relaxed);
    ::close(fd);
    const uint64_t duration_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - request_started).count());
    metrics.duration_us_sum.fetch_add(duration_us, std::memory_order_relaxed);
    graphenedb::server::atomic_max(metrics.duration_us_max, duration_us);
    metrics.active.fetch_sub(1, std::memory_order_relaxed);
    metrics.completed.fetch_add(1, std::memory_order_relaxed);
    structured_request_log(request_id, client_ip, method, path, code, duration_us, req.size(), resp.size(), !rate_allowed);
  };

  graphenedb::server::BoundedWorkerPool pool(worker_count, queue_capacity, handle_client, metrics);
  while (!g_stop) {
    sockaddr_in peer{};
    socklen_t peer_len = sizeof(peer);
    int fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&peer), &peer_len);
    if (fd < 0) {
      if (g_stop) break;
      if (errno == EINTR) continue;
      perror("accept");
      break;
    }
    char ipbuf[INET_ADDRSTRLEN]{};
    const char* ip = inet_ntop(AF_INET, &peer.sin_addr, ipbuf, sizeof(ipbuf));
    ConnectionTask task{fd, ip ? std::string(ip) : std::string("unknown")};
    if (!pool.submit(std::move(task))) {
      const std::string body = "{\"error\":\"server_overloaded\"}";
      const auto resp = response(503, body, {}, "application/json", 1);
      (void)write_all(fd, resp);
      ::close(fd);
    }
  }
  if (g_listen_fd >= 0) {
    ::close(static_cast<int>(g_listen_fd));
    g_listen_fd = -1;
  }
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"info\",\"event\":\"shutdown_started\",\"active_requests\":"
        << metrics.active.load() << ",\"queued_requests\":" << metrics.queue_depth.load() << "}";
    emit_structured_log(log.str());
  }
  pool.shutdown();
  int exit_code = 0;
  if (shutdown_checkpoint) {
    const auto checkpoint_started = std::chrono::steady_clock::now();
    const auto cst = db.compact();
    const auto checkpoint_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - checkpoint_started).count();
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"" << (cst ? "info" : "error")
        << "\",\"event\":\"shutdown_checkpoint\",\"ok\":" << (cst ? "true" : "false")
        << ",\"duration_ms\":" << checkpoint_ms;
    if (!cst) {
      log << ",\"error\":\"" << json_escape(cst.message) << "\"";
      exit_code = 1;
    }
    log << "}";
    emit_structured_log(log.str());
  }
  const auto close_status = db.close();
  if (!close_status) exit_code = 1;
  {
    std::ostringstream log;
    log << "{\"timestamp\":\"" << graphenedb::server::utc_timestamp()
        << "\",\"level\":\"" << (exit_code == 0 ? "info" : "error")
        << "\",\"event\":\"shutdown_complete\",\"exit_code\":" << exit_code << "}";
    emit_structured_log(log.str());
  }
  return exit_code;
}
