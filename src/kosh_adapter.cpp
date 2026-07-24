#include "graphene/kosh_adapter.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace graphene {
namespace {
std::vector<std::string> split(const std::string& s, char delim) {
  std::vector<std::string> out; std::string item; std::stringstream ss(s); while (std::getline(ss, item, delim)) out.push_back(item); return out;
}
int hexval(char c){ if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
bool hex_decode_local(const std::string& in, std::string* out){ if(in.size()%2) return false; out->clear(); for(size_t i=0;i<in.size();i+=2){ int h=hexval(in[i]), l=hexval(in[i+1]); if(h<0||l<0)return false; out->push_back(static_cast<char>((h<<4)|l)); } return true; }
bool parse_vec_local(const std::string& s, std::vector<float>* out){ out->clear(); if(s.empty()) return true; for(auto& part: split(s, ',')){ try{ size_t pos=0; float f=std::stof(part,&pos); if(pos!=part.size() || !std::isfinite(f)) return false; out->push_back(f); } catch(...) { return false; } } return true; }
bool parse_meta_local(const std::string& s, std::map<std::string,std::string>* out){ out->clear(); if(s.empty()) return true; for(auto& kvs: split(s,';')){ auto p=kvs.find('='); if(p==std::string::npos) return false; std::string k,v; if(!hex_decode_local(kvs.substr(0,p),&k)) return false; if(!hex_decode_local(kvs.substr(p+1),&v)) return false; (*out)[k]=v; } return true; }
}

KoshAdapter::KoshAdapter(GrapheneDB& db) : db_(db) {}

Status KoshAdapter::ingest_memory(const KoshMemoryRecord& memory, uint32_t* out_id) {
  NodeInput n;
  n.content = memory.content;
  n.vector = memory.vector;
  n.signature = memory.signature;
  n.incident = memory.incident;
  n.root = memory.root;
  n.symptom = memory.symptom;
  n.metadata = memory.metadata;
  n.metadata["kosh_external_id"] = memory.external_id;
  n.metadata["kosh_type"] = memory.type;
  n.metadata["adapter"] = "graphene-kosh-adapter-v1";
  return db_.put_node(n, out_id);
}

Status KoshAdapter::link(uint32_t from,
                         uint32_t to,
                         EdgeRole role,
                         EdgeOrigin origin,
                         double confidence,
                         std::map<std::string, std::string> provenance) {
  EdgeInput e;
  e.from = from;
  e.to = to;
  e.origin = origin;
  e.role = role;
  e.confidence = confidence;
  e.metadata = std::move(provenance);
  e.metadata["adapter"] = "graphene-kosh-adapter-v1";
  return db_.put_edge(e);
}

MemoryBundle KoshAdapter::retrieve_causal_bundle(const std::vector<float>& query, uint64_t signature, QueryMode mode) const {
  return db_.causal_search(query, signature, mode);
}

DialecticResult KoshAdapter::retrieve_dialectic(
    const std::vector<float>& query,
    uint64_t signature,
    const DialecticOptions& options) const {
  return DialecticEngine(db_).reason(query, signature, options);
}

Status KoshAdapter::ingest_tsv(const std::filesystem::path& file, std::vector<uint32_t>* out_ids) {
  std::ifstream in(file);
  if (!in) return Status::error(ErrorCode::IoError, "cannot read Kosh interchange file: " + file.string());
  std::string line; size_t line_no = 0;
  while (std::getline(in, line)) {
    ++line_no;
    if (line.empty() || line[0] == '#') continue;
    auto p = split(line, '\t');
    if (p.size() != 9) return Status::error(ErrorCode::InvalidInput, "bad Kosh TSV field count at line " + std::to_string(line_no));
    KoshMemoryRecord r;
    r.external_id = p[0]; r.type = p[1];
    try { r.signature = std::stoull(p[2]); r.incident = static_cast<uint32_t>(std::stoul(p[3])); } catch(...) { return Status::error(ErrorCode::InvalidInput, "bad numeric field at line " + std::to_string(line_no)); }
    r.root = p[4] == "1"; r.symptom = p[5] == "1";
    if (!parse_vec_local(p[6], &r.vector)) return Status::error(ErrorCode::InvalidInput, "bad vector at line " + std::to_string(line_no));
    if (!hex_decode_local(p[7], &r.content)) return Status::error(ErrorCode::InvalidInput, "bad content hex at line " + std::to_string(line_no));
    if (!parse_meta_local(p[8], &r.metadata)) return Status::error(ErrorCode::InvalidInput, "bad metadata at line " + std::to_string(line_no));
    uint32_t id = 0; auto st = ingest_memory(r, &id); if (!st) return st; if (out_ids) out_ids->push_back(id);
  }
  return Status::ok();
}

} // namespace graphene
