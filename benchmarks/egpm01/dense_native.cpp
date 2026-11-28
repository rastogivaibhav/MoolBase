#include "graphene/db.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <stdexcept>
using namespace graphene;
static void check(Status s){if(!s) throw std::runtime_error(s.message);}
int main(int argc,char**argv){
 try{
  if(argc!=3 || std::filesystem::exists(argv[1])) throw std::runtime_error("usage: egpm_dense_native NEW_DB_DIR vectors.txt");
  std::ifstream in(argv[2]); size_t n,m,d,k;
  if(!(in>>n>>m>>d>>k)||!n||!m||!d||!k||k>n||d>4096) throw std::runtime_error("bad dimensions");
  GrapheneDB db; DBOptions opt;opt.dimension=d;opt.vector_index_kind=VectorIndexKind::Flat;opt.fsync_on_commit=false;
  check(db.open(argv[1],opt));
  std::map<uint32_t,size_t> ids;
  for(size_t i=0;i<n;i++){NodeInput node;node.content="document-"+std::to_string(i);node.vector.resize(d);for(auto& x:node.vector)if(!(in>>x))throw std::runtime_error("bad document vector");uint32_t id;check(db.put_node(node,&id));ids.emplace(id,i);}
  for(size_t i=0;i<m;i++){std::vector<float> q(d);for(auto& x:q)if(!(in>>x))throw std::runtime_error("bad query vector");auto hits=db.vector_search(q,k);if(hits.size()!=k)throw std::runtime_error("unexpected result count");for(size_t j=0;j<hits.size();j++)std::cout<<(j?" ":"")<<ids.at(hits[j].node_id);std::cout<<"\n";}
  check(db.close());return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
