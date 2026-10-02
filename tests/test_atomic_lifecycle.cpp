#include "graphene/db.hpp"
#include <filesystem>
#include <cstdlib>
#include <iostream>
using namespace graphene;
void check(bool ok){if(!ok){std::cerr<<"atomic lifecycle regression failed\n";std::exit(1);}}
int main(){
 const auto base=std::filesystem::temp_directory_path()/"graphenedb_atomic_lifecycle";std::filesystem::remove_all(base);
 DBOptions o;o.dimension=2;o.fsync_on_commit=true;
 for(const auto fault:{"GRAPHENEDB_TEST_FAIL_WAL_APPEND","GRAPHENEDB_TEST_FAIL_WAL_WRITE","GRAPHENEDB_TEST_FAIL_WAL_FSYNC"}){
  GrapheneDB db;auto p=base/fault;check(bool(db.open(p,o)));uint32_t a,b;
  NodeInput n;n.content="old";n.vector={1,0};check(bool(db.put_node(n,&a)));n.content="target";check(bool(db.put_node(n,&b)));
  EdgeInput e;e.from=a;e.to=b;check(bool(db.put_edge(e)));auto snap=db.snapshot();
  BatchInput batch;batch.delete_node_ids={a};n.content="replacement";batch.nodes={n};e.from=b;e.to=b+1;batch.edges={e};
  setenv(fault,"1",1);check(!db.put_batch(batch));unsetenv(fault);
  check(db.snapshot()==snap&&db.get_node(a).has_value()&&db.node_count()==2&&db.edge_count()==1);
  check(bool(db.close()));check(bool(db.open(p,o)));check(db.snapshot()==snap&&db.get_node(a).has_value());
  auto invalid=batch;invalid.delete_node_ids={a,a};check(!db.put_batch(invalid));invalid=batch;invalid.edges[0].from=a;check(!db.put_batch(invalid));check(db.snapshot()==snap);
  BatchResult r;check(bool(db.put_batch(batch,&r)));check(r.node_ids.size()==1&&!db.get_node(a)&&db.get_node(a,snap).has_value());check(db.node_count()==2&&db.edge_count()==1);check(bool(db.validate()));
  check(bool(db.close()));check(bool(db.open(p,o)));check(!db.get_node(a)&&db.get_node(r.node_ids[0]).has_value());check(bool(db.compact()));check(bool(db.validate()));
  BatchInput retire;retire.delete_node_ids={b,r.node_ids[0]};check(bool(db.put_batch(retire)));check(db.node_count()==0&&db.edge_count()==0);check(bool(db.close()));check(bool(db.open(p,o)));check(db.node_count()==0&&db.edge_count()==0);check(bool(db.close()));
 }
 std::filesystem::remove_all(base);std::cout<<"PASS atomic lifecycle: append/write/fsync rollback, replay, snapshots, validation, compaction and shared-edge retirement\n";
}
