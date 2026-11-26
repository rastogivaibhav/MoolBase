// Isolated research adapter: native DB adjacency, not a change to MoolBase core.
#include "graphene/db.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <set>
#include <tuple>
#include <functional>
#include <stdexcept>
using namespace graphene;
static void check(Status s){if(!s)throw std::runtime_error(s.message);}
int main(int argc,char**argv){
 try{
  if(argc!=3||std::filesystem::exists(argv[1]))throw std::runtime_error("usage: egpm_graph_native NEW_DB input");
  std::ifstream in(argv[2]);unsigned n,m,start,hops;
  if(!(in>>n>>m>>start>>hops)||!n||start>=n||hops<1||hops>4||n>1000||m>2000)throw std::runtime_error("bad header");
  GrapheneDB db;DBOptions opts;opts.dimension=2;opts.fsync_on_commit=false;check(db.open(argv[1],opts));
  std::vector<uint32_t> ids(n);std::map<uint32_t,unsigned> reverse;
  for(unsigned i=0;i<n;i++){NodeInput x;x.content="entity-"+std::to_string(i);x.vector={1.f,0.f};check(db.put_node(x,&ids[i]));reverse[ids[i]]=i;}
  std::set<unsigned> seen;
  for(unsigned i=0;i<m;i++){unsigned from,to,stage,eid;char sign;
   if(!(in>>from>>to>>stage>>sign>>eid)||from>=n||to>=n||from==to||stage>=hops||(sign!='S'&&sign!='R')||!seen.insert(eid).second)throw std::runtime_error("bad edge");
   EdgeInput x;x.from=ids[from];x.to=ids[to];x.origin=EdgeOrigin::Discovered;x.role=sign=='S'?EdgeRole::Supports:EdgeRole::Contradicts;
   x.metadata={{"egpm_stage",std::to_string(stage)},{"egpm_evidence",std::to_string(eid)},{"source_id","span-"+std::to_string(eid)}};check(db.put_edge(x));
  }
  auto snap=db.snapshot();std::vector<unsigned> path;std::set<uint32_t> visited{ids[start]};unsigned count=0;
  std::function<void(uint32_t,unsigned,bool,bool)> dfs=[&](uint32_t u,unsigned stage,bool refuted,bool contested){
    if(stage==hops){if(++count>10000)throw std::runtime_error("path limit");std::cout<<reverse.at(u)<<" "<<(contested?"contested":refuted?"refuted_chain":"supported");for(auto e:path)std::cout<<" "<<e;std::cout<<"\n";return;}
    auto edges=db.outgoing_edges(u,snap);std::map<uint32_t,unsigned> signs;
    for(const auto& e:edges)if(e.metadata.at("egpm_stage")==std::to_string(stage))signs[e.to]|=e.role==EdgeRole::Contradicts?2:1;
    for(const auto& e:edges){if(e.metadata.at("egpm_stage")!=std::to_string(stage)||visited.count(e.to))continue;
      path.push_back(std::stoul(e.metadata.at("egpm_evidence")));visited.insert(e.to);
      dfs(e.to,stage+1,refuted||e.role==EdgeRole::Contradicts,contested||signs[e.to]==3);
      visited.erase(e.to);path.pop_back();
    }
  };dfs(ids[start],0,false,false);check(db.close());return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
}
