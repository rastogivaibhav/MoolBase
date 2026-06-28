#include "graphene/db.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;

static int get_arg(int argc, char** argv, const std::string& key, int def) { for(int i=1;i+1<argc;i++) if(argv[i]==key) return std::stoi(argv[i+1]); return def; }
static std::vector<float> vec(uint32_t d, int i) { std::vector<float> v(d); for(uint32_t j=0;j<d;j++) v[j]=float(((i+1)*(j+3))%997)/997.0f + 0.001f; return v; }
static void require(Status st, const char* what){ if(!st){ std::cerr<<"FAIL "<<what<<": "<<st.message<<"\n"; std::abort(); }}

int main(int argc, char** argv) {
  int nodes = get_arg(argc, argv, "--nodes", 1000000);
  int queries = get_arg(argc, argv, "--queries", 5);
  uint32_t D = static_cast<uint32_t>(get_arg(argc, argv, "--dim", 2));
  bool reopen = get_arg(argc, argv, "--reopen", 1) != 0;
  fs::path dir = fs::temp_directory_path() / ("graphenedb_rc_1m_storage_" + std::to_string(nodes) + "_" + std::to_string(D));
  fs::remove_all(dir);
  DBOptions opt; opt.dimension=D; opt.fsync_on_commit=false; opt.wal_rotate_bytes=0;
  GrapheneDB db; require(db.open(dir,opt),"open 1m storage");
  auto t0=Clock::now();
  for(int i=0;i<nodes;i++){
    NodeInput n; n.content="n"+std::to_string(i); n.vector=vec(D,i); n.signature=signature_for(static_cast<uint32_t>(i%16), static_cast<uint32_t>((i/16)%16)); n.incident=static_cast<uint32_t>(i); if(i%128==0) n.metadata["bucket"]="b"+std::to_string(i%16);
    require(db.put_node(n),"put 1m node");
  }
  auto t1=Clock::now();
  assert(db.node_count()==static_cast<size_t>(nodes));
  std::vector<double> qms;
  for(int q=0;q<queries;q++){
    auto qv=vec(D,(q*7919)%nodes); auto a=Clock::now(); auto r=db.vector_search(qv,10); auto b=Clock::now(); assert(!r.empty()); qms.push_back(std::chrono::duration<double,std::milli>(b-a).count());
  }
  std::sort(qms.begin(),qms.end());
  std::string report; require(db.validate(&report),"validate 1m storage");
  require(db.close(),"close 1m storage");
  auto t2=Clock::now();
  double reopen_ms=0.0;
  if(reopen){ auto r0=Clock::now(); GrapheneDB rdb; require(rdb.open(dir,opt),"reopen 1m storage"); auto r1=Clock::now(); assert(rdb.node_count()==static_cast<size_t>(nodes)); require(rdb.validate(&report),"validate reopened 1m"); reopen_ms=std::chrono::duration<double,std::milli>(r1-r0).count(); require(rdb.close(),"close reopened 1m"); }
  std::cout << "rc_1m_storage_gate=true\n";
  std::cout << "nodes="<<nodes<<" dim="<<D<<" queries="<<queries<<"\n";
  std::cout << "ingest_ms="<<std::chrono::duration<double,std::milli>(t1-t0).count()<<" total_close_ms="<<std::chrono::duration<double,std::milli>(t2-t0).count()<<"\n";
  std::cout << "vector_p50_ms="<<qms[qms.size()/2]<<" vector_p95_ms="<<qms[std::min(qms.size()-1, static_cast<size_t>(qms.size()*0.95))]<<"\n";
  std::cout << "reopen_ms="<<reopen_ms<<"\n";
  return 0;
}
