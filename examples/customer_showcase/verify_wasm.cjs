const fs=require('fs'),assert=require('assert');
const path=require('path');
const root=path.resolve(process.argv[2]||'dist');
const reports=path.resolve(process.argv[3]||'reports/customer-showcase');
(async()=>{
 const m=await require(root+'/moolbase.js')({wasmBinary:fs.readFileSync(root+'/moolbase.wasm')});
 const fixtures=JSON.parse(fs.readFileSync(root+'/scenarios.json'));
 let calls=0;
 const parse=x=>{calls++;const r=JSON.parse(x);assert(r.ok,r.error);return r;};
 for(const s of fixtures){
  let states=[parse(m.ccall('demo_reset','string',['string','string'],s.hypotheses))];
  for(const e of s.events)states.push(parse(m.ccall('demo_add','string',['string','string','string','number','string','number','string'],[e.id,e.content,e.family,e.target,e.kind,e.verified?1:0,e.retire])));
  states.push(parse(m.ccall('demo_reopen','string',[],[])));
  const native=JSON.parse(fs.readFileSync(reports+'/'+s.id+'.json')).states;
  assert.equal(states.length,native.length);
  for(let i=0;i<states.length;i++){
   assert.equal(states[i].status,native[i].status,s.id+' status step '+i);
   assert.equal(states[i].answer,native[i].answer,s.id+' answer step '+i);
   assert.equal(states[i].bundleHash,native[i].bundleHash,s.id+' hash step '+i);
   assert.deepEqual(states[i].events,native[i].events,s.id+' events step '+i);
   assert.deepEqual(states[i].targets.map(t=>t.families),native[i].targets.map(t=>t.families));
  }
  console.log(s.id,'PASS native/WASM parity',states.length,'states');
 }
 const before=parse(m.ccall('demo_reopen','string',[],[]));
 const invalid=JSON.parse(m.ccall('demo_add','string',['string','string','string','number','string','number','string'],['bad','bad','family',0,'revoke',1,'missing']));
 assert.equal(invalid.ok,false);
 const after=parse(m.ccall('demo_reopen','string',[],[]));assert.equal(after.bundleHash,before.bundleHash);
 assert(m.FS.readFile('/moolbase-demo/graphene.wal').length>0);
 console.log('PASS:',calls,'WASM calls; invalid retirement preserved bundle; actual WAL file exists.');
})();
