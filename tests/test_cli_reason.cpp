#include "graphene/db.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace graphene;
namespace fs = std::filesystem;

static std::string quote(const fs::path& path) { return "\"" + path.string() + "\""; }

static std::string run_capture(const std::string& command) {
  const fs::path output = fs::temp_directory_path() / "graphenedb_cli_reason_output.txt";
#ifdef _WIN32
  const std::string wrapped = "cmd /C \"" + command + " > " + quote(output) + " 2>&1\"";
#else
  const std::string wrapped = command + " > " + quote(output) + " 2>&1";
#endif
  const int code = std::system(wrapped.c_str());
  std::ifstream input(output);
  std::stringstream buffer;
  buffer << input.rdbuf();
  if (code != 0) {
    std::cerr << buffer.str();
    std::abort();
  }
  return buffer.str();
}

int main(int argc, char** argv) {
  assert(argc >= 2);
  const fs::path cli = argv[1];
  const fs::path directory = fs::temp_directory_path() / "graphenedb_cli_reason_db";
  fs::remove_all(directory);

  GrapheneDB db;
  DBOptions options;
  options.dimension = 3;
  options.fsync_on_commit = false;
  assert(db.open(directory, options));
  const uint64_t signature = signature_for(4, 9);
  NodeInput root;
  root.content = "release root";
  root.vector = {1, 0, 0};
  root.signature = signature;
  root.root = true;
  NodeInput middle;
  middle.content = "pool exhaustion";
  middle.vector = {0.2f, 0.8f, 0};
  middle.signature = signature;
  NodeInput symptom;
  symptom.content = "checkout failures";
  symptom.vector = {0, 1, 0};
  symptom.signature = signature;
  symptom.symptom = true;
  uint32_t a = 0, b = 0, c = 0;
  assert(db.put_node(root, &a));
  assert(db.put_node(middle, &b));
  assert(db.put_node(symptom, &c));
  assert(db.put_edge({a, b, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.96,
                      {{"source_id", "release-log"}}}));
  assert(db.put_edge({b, c, EdgeOrigin::Discovered, EdgeRole::Causal, 0.95,
                      {{"source_id", "heap-profile"}}}));
  assert(db.close());

  const std::string command = quote(cli) + " reason " + quote(directory) +
      " 3 0,1,0 " + std::to_string(signature) + " --mode empirical --json";
  const std::string output = run_capture(command);
  assert(output.find("\"fiber_bundle_built\":true") != std::string::npos);
  assert(output.find("\"stability_critic_executed\":true") != std::string::npos);
  assert(output.find("\"opposition_executed\":true") != std::string::npos);
  assert(output.find("\"no_silent_promotion\":true") != std::string::npos);
  assert(output.find("\"primary_node\":0") != std::string::npos);
  assert(output.find("\"status\":\"") != std::string::npos);

  fs::remove_all(directory);
  std::cout << "graphenedb_cli_reason_tests_passed=true\n";
  return 0;
}
