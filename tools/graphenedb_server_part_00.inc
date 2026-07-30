#include "graphene/db.hpp"
#include "graphene/dialectic.hpp"
#include "graphene/hypokosh.hpp"
#include "graphene/hypokosh_runtime.hpp"
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
