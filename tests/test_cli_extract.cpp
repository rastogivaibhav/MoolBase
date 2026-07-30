#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

static std::string quote(const fs::path& p) {
  return "\"" + p.string() + "\"";
}

static std::string run_capture(const std::string& command) {
  std::string tmp = (fs::temp_directory_path() / "graphenedb_cli_extract_capture.txt").string();
#ifdef _WIN32
  std::string wrapped = "cmd /C \"" + command + " > " + quote(tmp) + " 2>&1\"";
#else
  std::string wrapped = command + " > " + quote(tmp) + " 2>&1";
#endif
  int rc = std::system(wrapped.c_str());
  std::ifstream in(tmp);
  std::stringstream ss;
  ss << in.rdbuf();
  if (rc != 0) {
    std::cerr << ss.str() << "\n";
    std::abort();
  }
  return ss.str();
}

int main(int argc, char** argv) {
  assert(argc >= 2);
  fs::path cli = argv[1];
  fs::path base = fs::temp_directory_path() / "graphenedb_cli_extract_test";
  fs::remove_all(base);
  fs::create_directories(base);
  fs::path db = base / "db";
  fs::path tsv = base / "extraction.tsv";
  uint64_t sig = (1ull << 3) | (1ull << (16 + 4));
  {
    std::ofstream out(tsv);
    out << "root-a\troot extracted claim\t0.1,0.2,0.3\t" << sig << "\t9\troot\n";
    out << "symptom-a\tsymptom extracted claim\t0.11,0.21,0.29\t" << sig << "\t9\tsymptom\n";
    out << "impact-a\timpact extracted claim\t0.12,0.22,0.28\t" << sig << "\t9\timpact\n";
  }

  auto first = run_capture(quote(cli) + " extract-tsv " + quote(db) + " 3 research-pack " + quote(tsv));
  assert(first.find("inserted_nodes=3") != std::string::npos);
  assert(first.find("existing_nodes=0") != std::string::npos);
  assert(first.find("inserted_edges=2") != std::string::npos);

  auto second = run_capture(quote(cli) + " extract-tsv " + quote(db) + " 3 research-pack " + quote(tsv));
  assert(second.find("inserted_nodes=0") != std::string::npos);
  assert(second.find("existing_nodes=3") != std::string::npos);
  assert(second.find("inserted_edges=0") != std::string::npos);

  auto inspect = run_capture(quote(cli) + " inspect " + quote(db) + " 3");
  assert(inspect.find("nodes_visible=3") != std::string::npos);
  assert(inspect.find("edges_visible=2") != std::string::npos);
  assert(inspect.find("lattice_nodes=3") != std::string::npos);

  std::cout << "graphenedb_cli_extract_tests_passed=true\n";
  return 0;
}
