#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string quote(const fs::path& p) {
  return "\"" + p.string() + "\"";
}

static std::string run_capture(const std::string& command) {
  std::string tmp = (fs::temp_directory_path() / "graphenedb_cli_import_capture.txt").string();
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

struct CommandResult {
  int rc{0};
  std::string output;
};

static CommandResult run_capture_result(const std::string& command) {
  std::string tmp = (fs::temp_directory_path() / "graphenedb_cli_import_capture_result.txt").string();
#ifdef _WIN32
  std::string wrapped = "cmd /C \"" + command + " > " + quote(tmp) + " 2>&1\"";
#else
  std::string wrapped = command + " > " + quote(tmp) + " 2>&1";
#endif
  int rc = std::system(wrapped.c_str());
  std::ifstream in(tmp);
  std::stringstream ss;
  ss << in.rdbuf();
  return {rc, ss.str()};
}

static uint64_t current_pid() {
#ifdef _WIN32
  return static_cast<uint64_t>(::GetCurrentProcessId());
#else
  return static_cast<uint64_t>(::getpid());
#endif
}

int main(int argc, char** argv) {
  assert(argc >= 2);
  fs::path cli = argv[1];
  fs::path base = fs::temp_directory_path() / "graphenedb_cli_import_test";
  fs::remove_all(base);
  fs::create_directories(base);
  fs::path db = base / "db";
  fs::path tsv = base / "memories.tsv";
  uint64_t sig = (1ull << 1) | (1ull << (16 + 1));
  {
    std::ofstream out(tsv);
    out << "root memory\t0.1,0.11,0.12,0.13\t" << sig << "\t42\troot\n";
    out << "symptom memory\t0.2,0.21,0.22,0.23\t" << sig << "\t42\tsymptom\n";
    out << "other root\t0.3,0.31,0.32,0.33\t" << sig << "\t43\troot\n";
  }

  auto imported = run_capture(quote(cli) + " import-tsv " + quote(db) + " 4 " + quote(tsv) + " --wal-rotate-bytes 512");
  assert(imported.find("imported_nodes=3") != std::string::npos);
  assert(imported.find("imported_edges=2") != std::string::npos);

  auto inspect = run_capture(quote(cli) + " inspect " + quote(db) + " 4 --wal-rotate-bytes 512");
  assert(inspect.find("storage_format=2") != std::string::npos);
  assert(inspect.find("wal_frame_format=1") != std::string::npos);
  assert(inspect.find("wal_rotate_bytes=512") != std::string::npos);
  assert(inspect.find("wal_bytes=") != std::string::npos);
  assert(inspect.find("data_bytes=") != std::string::npos);
  assert(inspect.find("nodes_visible=3") != std::string::npos);
  assert(inspect.find("edges_visible=2") != std::string::npos);
  assert(inspect.find("lattice_nodes=3") != std::string::npos);

  auto inspect_json = run_capture(quote(cli) + " inspect " + quote(db) + " 4 --wal-rotate-bytes 512 --json");
  assert(inspect_json.find("\"nodes_visible\":3") != std::string::npos);
  assert(inspect_json.find("\"edges_visible\":2") != std::string::npos);
  assert(inspect_json.find("\"wal_rotate_bytes\":512") != std::string::npos);
  assert(inspect_json.find("\"vector_index_requested\":\"auto\"") != std::string::npos);
  assert(inspect_json.find("\"vector_index\":\"kdtree\"") != std::string::npos);
  assert(inspect_json.find("\"path\":\"") != std::string::npos);

  auto inspect_flat_json = run_capture(quote(cli) + " inspect " + quote(db) + " 4 --vector-index flat --json");
  assert(inspect_flat_json.find("\"vector_index_requested\":\"flat\"") != std::string::npos);
  assert(inspect_flat_json.find("\"vector_index\":\"flat\"") != std::string::npos);

  auto inspect_kdtree_json = run_capture(quote(cli) + " inspect " + quote(db) + " 4 --vector-index kdtree --json");
  assert(inspect_kdtree_json.find("\"vector_index_requested\":\"kdtree\"") != std::string::npos);
  assert(inspect_kdtree_json.find("\"vector_index\":\"kdtree\"") != std::string::npos);

#ifdef GRAPHENEDB_HAS_FAISS
  auto inspect_faiss_json = run_capture(quote(cli) + " inspect " + quote(db) + " 4 --vector-index faiss --json");
  assert(inspect_faiss_json.find("\"vector_index_requested\":\"faiss\"") != std::string::npos);
  assert(inspect_faiss_json.find("\"vector_index\":\"faiss\"") != std::string::npos);
#else
  auto inspect_faiss = run_capture_result(quote(cli) + " inspect " + quote(db) + " 4 --vector-index faiss");
  assert(inspect_faiss.rc != 0);
  assert(inspect_faiss.output.find("without FAISS support") != std::string::npos);
#endif

  fs::path high_dim = base / "high-dim-index";
  auto high_dim_auto = run_capture(quote(cli) + " init " + quote(high_dim) + " 64 --json");
  (void)high_dim_auto;
  auto high_dim_inspect = run_capture(quote(cli) + " inspect " + quote(high_dim) + " 64 --json");
  assert(high_dim_inspect.find("\"vector_index_requested\":\"auto\"") != std::string::npos);
  assert(high_dim_inspect.find("\"vector_index\":\"flat\"") != std::string::npos);

  auto compacted = run_capture(quote(cli) + " compact " + quote(db) + " 4");
  assert(compacted.find("compacted") != std::string::npos);
  auto compacted_json = run_capture(quote(cli) + " compact " + quote(db) + " 4 --json");
  assert(compacted_json.find("\"compacted\":true") != std::string::npos);
  auto compact_inspect = run_capture(quote(cli) + " inspect " + quote(db) + " 4");
  assert(compact_inspect.find("wal_bytes=0") != std::string::npos);

  auto neighbors = run_capture(quote(cli) + " neighbors " + quote(db) + " 4 0 1");
  assert(neighbors.find("1") != std::string::npos);

  auto valid = run_capture(quote(cli) + " validate " + quote(db) + " 4");
  assert(valid.find("validation=ok") != std::string::npos);
  auto valid_json = run_capture(quote(cli) + " validate " + quote(db) + " 4 --json");
  assert(valid_json.find("\"validation\":\"ok\"") != std::string::npos);
  assert(valid_json.find("\"report\":\"OK\\n\"") != std::string::npos);

  fs::path backup = base / "backup";
  auto backed = run_capture(quote(cli) + " backup " + quote(db) + " 4 " + quote(backup));
  assert(backed.find("backup_verified=true") != std::string::npos);
  fs::path backup_json = base / "backup-json";
  auto backed_json = run_capture(quote(cli) + " backup " + quote(db) + " 4 " + quote(backup_json) + " --json");
  assert(backed_json.find("\"backup\":\"") != std::string::npos);
  assert(backed_json.find("\"backup_verified\":true") != std::string::npos);

  auto backup_inspect = run_capture(quote(cli) + " inspect " + quote(backup) + " 4");
  assert(backup_inspect.find("nodes_visible=3") != std::string::npos);
  assert(backup_inspect.find("edges_visible=2") != std::string::npos);

  fs::path stale = base / "stale-lock-db";
  fs::create_directories(stale);
  {
    std::ofstream lock(stale / "LOCK");
    lock << "999999999\n";
  }
  auto stale_recovered = run_capture(quote(cli) + " inspect " + quote(stale) + " 4 --json");
  assert(stale_recovered.find("\"nodes_visible\":0") != std::string::npos);
  assert(!fs::exists(stale / "LOCK"));

  {
    std::ofstream lock(stale / "LOCK");
    lock << "999999999\n";
  }
  auto stale_strict = run_capture_result(quote(cli) + " inspect " + quote(stale) + " 4 --no-recover-stale-lock");
  assert(stale_strict.rc != 0);
  assert(stale_strict.output.find("database lock exists") != std::string::npos);
  fs::remove(stale / "LOCK");

  fs::path busy = base / "busy-lock-db";
  fs::create_directories(busy);
  {
    std::ofstream lock(busy / "LOCK");
    lock << current_pid() << "\n";
  }
  auto busy_blocked = run_capture_result(quote(cli) + " inspect " + quote(busy) + " 4");
  assert(busy_blocked.rc != 0);
  assert(busy_blocked.output.find("database lock exists") != std::string::npos);

  std::cout << "graphenedb_cli_import_tests_passed=true\n";
  return 0;
}
