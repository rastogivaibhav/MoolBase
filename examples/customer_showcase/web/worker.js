importScripts('./moolbase.js');
const engine = createMoolBase();
const bytes = new TextEncoder();
function text(value, name, limit, optional = false) {
  if (typeof value !== 'string' || value.includes('\0') || bytes.encode(value).length > limit || (!optional && !value.trim()))
    throw Error(`${name} must be nonblank text without NUL characters, at most ${limit} UTF-8 bytes.`);
  return value;
}
function evidence(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) throw Error('Evidence must be an object.');
  const allowed = ['id','content','family','target','kind','verified','retire'];
  if (Object.keys(value).some(key => !allowed.includes(key))) throw Error('Unknown evidence field.');
  text(value.id,'ID',80); text(value.content,'Observation',1000); text(value.family,'Evidence family',80);
  if (!Number.isInteger(value.target) || value.target < 0 || value.target > 1) throw Error('Target must be integer 0 or 1.');
  if (!['support','refute','revoke','supersede'].includes(value.kind)) throw Error('Unknown evidence action.');
  if (typeof value.verified !== 'boolean') throw Error('Certificate must be a boolean.');
  const retire = value.retire === undefined ? '' : text(value.retire,'Prior evidence ID',80,true);
  if (['revoke','supersede'].includes(value.kind) !== Boolean(retire.trim())) throw Error('Retirement requires revoke or supersede and an active prior evidence ID.');
  return {...value, retire};
}
onmessage = async ({data}) => {
  try {
    if (!data || !Number.isInteger(data.id) || typeof data.action !== 'string') throw Error('Invalid request.');
    let args;
    if (data.action === 'reset') {
      if (!Array.isArray(data.hypotheses) || data.hypotheses.length !== 2) throw Error('Two hypotheses are required.');
      args = data.hypotheses.map((v,i) => text(v,`Hypothesis ${i+1}`,200));
    } else if (data.action === 'add') {
      const e = evidence(data.event); args = [e.id,e.content,e.family,e.target,e.kind,e.verified?1:0,e.retire];
    } else if (data.action !== 'reopen') throw Error('Unknown action.');
    const m = await engine;
    const result = JSON.parse(data.action === 'reset'
      ? m.ccall('demo_reset','string',['string','string'],args)
      : data.action === 'add'
        ? m.ccall('demo_add','string',['string','string','string','number','string','number','string'],args)
        : m.ccall('demo_reopen','string',[],[]));
    postMessage({id:data.id,result});
  } catch (error) { postMessage({id:data?.id,result:{ok:false,error:error.message}}); }
};
