#include "graphene/db.hpp"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> parse_vec(const std::string& s) {
  std::vector<float> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) out.push_back(std::stof(item));
  return out;
}

static void usage() {
  std::cout << "graphenedb_cli commands:\n"
            << "  init <path> <dim>\n"
            << "  inspect <path> <dim>\n"
            << "  put-node <path> <dim> <content> <comma-vector> <signature> [root|symptom|impact]\n"
            << "  put-edge <path> <dim> <from> <to> [causal|contradicts|supersedes|supports]\n"
            << "  search <path> <dim> <comma-vector> <signature>\n"
            << "  compact <path> <dim>\n";
}

int main(int argc, char** argv) {
  if (argc < 2) { usage(); return 1; }
  std::string cmd = argv[1];
  if (cmd == "init") {
    if (argc < 4) { usage(); return 1; }
    GrapheneDB db;
    DBOptions opt; opt.dimension = static_cast<uint32_t>(std::stoul(argv[3]));
    auto st = db.open(argv[2], opt);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    db.close();
    std::cout << "initialized\n";
    return 0;
  }
  if (argc < 4) { usage(); return 1; }
  fs::path path = argv[2];
  DBOptions opt; opt.dimension = static_cast<uint32_t>(std::stoul(argv[3]));
  GrapheneDB db;
  auto st = db.open(path, opt);
  if (!st) { std::cerr << st.message << "\n"; return 2; }

  if (cmd == "inspect") {
    std::string out;
    st = db.inspect(&out);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << out;
    return 0;
  }
  if (cmd == "put-node") {
    if (argc < 7) { usage(); return 1; }
    NodeInput n;
    n.content = argv[4];
    n.vector = parse_vec(argv[5]);
    n.signature = std::stoull(argv[6]);
    if (argc >= 8) {
      std::string flag = argv[7];
      n.root = flag == "root";
      n.symptom = flag == "symptom";
      n.impact = flag == "impact";
    }
    uint32_t id = 0;
    st = db.put_node(n, &id);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << id << "\n";
    return 0;
  }
  if (cmd == "put-edge") {
    if (argc < 6) { usage(); return 1; }
    EdgeInput e;
    e.from = static_cast<uint32_t>(std::stoul(argv[4]));
    e.to = static_cast<uint32_t>(std::stoul(argv[5]));
    if (argc >= 7) {
      std::string role = argv[6];
      if (role == "contradicts") e.role = EdgeRole::Contradicts;
      else if (role == "supersedes") e.role = EdgeRole::Supersedes;
      else if (role == "supports") e.role = EdgeRole::Supports;
      else e.role = EdgeRole::Causal;
    }
    uint32_t id = 0;
    st = db.put_edge(e, &id);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << id << "\n";
    return 0;
  }
  if (cmd == "search") {
    if (argc < 6) { usage(); return 1; }
    auto q = parse_vec(argv[4]);
    auto sig = std::stoull(argv[5]);
    auto b = db.causal_search(q, sig, QueryMode::Empirical);
    if (b.abstain) {
      std::cout << "ABSTAIN " << b.reason << "\n";
      return 0;
    }
    std::cout << "target=" << b.target_node << " confidence=" << b.confidence << " paths=" << b.paths.size() << "\n";
    for (const auto& why : b.why_retrieved) std::cout << "why=" << why << "\n";
    return 0;
  }
  if (cmd == "compact") {
    st = db.compact();
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << "compacted\n";
    return 0;
  }
  usage();
  return 1;
}
