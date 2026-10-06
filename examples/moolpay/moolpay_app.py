#!/usr/bin/env python3
"""MoolPay hackathon web agent. MoolPay never embeds MoolBase; it calls the MoolBase service."""
from __future__ import annotations
import base64, json, os, sys, urllib.error, urllib.parse, urllib.request, uuid
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

PAY="Payment destination is verified and safe to pay"
HOLD="Payment should be held because destination risk is unresolved"
REQUEST={"request_id":"pay-18400-001","supplier":"Northstar Components Ltd","amount_minor":1840000,"currency":"GBP","destination_last4":"4821"}
GOOD=[
 {"id":"supplier-master","content":"Supplier is active in the approved supplier master and the legal entity matches the invoice.","family":"supplier-master","target":0,"kind":"support","verified":True,"retire":""},
 {"id":"po-approved","content":"Purchase order PO-7741 is approved and matches the supplier and invoice amount.","family":"procurement","target":0,"kind":"support","verified":True,"retire":""},
 {"id":"bank-history","content":"The destination account ending 4821 matches the account used for the last verified supplier payment.","family":"payment-history","target":0,"kind":"support","verified":True,"retire":""}
]
BANK_CHANGE={"id":"bank-change-alert","content":"Supplier bank details were changed 20 minutes ago through a new administrator session; this contradicts the claim that the current destination is verified and safe to pay.","family":"bank-change-monitor","target":0,"kind":"refute","verified":True,"retire":""}

HTML=r'''<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>MoolPay</title><style>
:root{font-family:Inter,system-ui,sans-serif;color:#16202a;background:#f5f7f9}body{margin:0}.wrap{max-width:1100px;margin:auto;padding:36px 20px}.hero{display:flex;justify-content:space-between;gap:20px;align-items:flex-end}.tag{font-size:12px;letter-spacing:.12em;text-transform:uppercase;color:#596875}h1{font-size:54px;margin:6px 0 4px}h2{margin:0 0 12px}.sub{font-size:19px;color:#596875;max-width:760px}.arch{margin:28px 0;padding:16px 20px;background:#101820;color:#eef6f8;border-radius:16px;font-family:ui-monospace,monospace}.grid{display:grid;grid-template-columns:1.2fr 1fr 1fr;gap:16px}.card{background:white;border:1px solid #dde4e8;border-radius:18px;padding:20px;box-shadow:0 8px 30px #14202b0d}.e{border-left:4px solid #83a4b8;padding:10px 12px;margin:9px 0;background:#f7fafb}.warn{border-left-color:#d27935;background:#fff8f0}.status{font-size:34px;font-weight:800;margin:10px 0}.pay{color:#08775e}.hold{color:#b34b28}.pill{display:inline-block;padding:6px 10px;border-radius:999px;background:#e9eff2;font-size:12px}.btns{display:flex;gap:10px;flex-wrap:wrap;margin:20px 0}button{border:0;border-radius:12px;padding:12px 16px;font-weight:700;cursor:pointer;background:#16202a;color:white}button.alt{background:#e8eef1;color:#16202a}pre{font-size:11px;white-space:pre-wrap;max-height:250px;overflow:auto;background:#f5f7f9;padding:12px;border-radius:12px}.small{font-size:12px;color:#6b7882}@media(max-width:820px){.grid{grid-template-columns:1fr}h1{font-size:42px}.hero{display:block}}
</style></head><body><div class="wrap"><div class="hero"><div><div class="tag">Autonomous payments · evidence-aware execution</div><h1>MoolPay</h1><div class="sub">An AI payment agent that can move money only while MoolBase can still defend the decision.</div></div><span class="pill">Stripe: test/demo only</span></div><div class="arch">MoolPay Agent → MoolBase DB + HypoKosh/DWM → policy gate → Stripe</div><div class="btns"><button onclick="run('initial')">1 · Gather evidence</button><button class="alt" onclick="run('final')">2 · Introduce bank change</button><button onclick="execPay()">3 · Attempt payment</button><button class="alt" onclick="clean()">Clean-path payment</button></div><div class="grid"><section class="card"><h2>Evidence</h2><div id="evidence"></div></section><section class="card"><h2>MoolBase</h2><div class="small">Database + competing explanations + contradiction + receipt</div><div id="status" class="status">—</div><div id="reason"></div><pre id="receipt">Run the scenario.</pre></section><section class="card"><h2>Action</h2><div class="small">Server-side policy: only resolved PAY can reach Stripe.</div><div id="action" class="status">LOCKED</div><div id="execution"></div></section></div></div><script>
function evidence(items){document.getElementById('evidence').innerHTML=items.map(function(x){return '<div class="e '+(x.kind==='refute'?'warn':'')+'"><b>'+x.id+'</b><br><span class="small">'+x.family+' · '+x.kind+'</span><br>'+x.content+'</div>'}).join('')}
function paint(d){evidence(d.evidence);var s=d.latest.status,a=d.latest.action;var st=document.getElementById('status'),ac=document.getElementById('action');st.textContent=s.toUpperCase();st.className='status '+(s==='resolved'?'pay':'hold');ac.textContent=a;ac.className='status '+(a==='PAY'?'pay':'hold');document.getElementById('reason').textContent=d.latest.reason;document.getElementById('receipt').textContent=JSON.stringify(d.latest.receipt,null,2)}
async function post(path,body){var r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});return await r.json()}
async function run(phase){var d=await post('/api/evaluate',{phase:phase});paint(d)}
async function execPay(){var d=await post('/api/execute',{scenario:'bank-change'});paint(d.result);document.getElementById('execution').innerHTML=d.execution.executed?'<b>Stripe test execution created</b><pre>'+JSON.stringify(d.execution.gateway,null,2)+'</pre>':'<b>Stripe blocked</b><br>'+d.execution.reason}
async function clean(){var d=await post('/api/execute',{scenario:'clean'});paint(d.result);document.getElementById('execution').innerHTML=d.execution.executed?'<b>Stripe test execution created</b><pre>'+JSON.stringify(d.execution.gateway,null,2)+'</pre>':'<b>Blocked</b>'}
</script></body></html>'''


def moolbase(events):
    base=os.environ["MOOLBASE_URL"].rstrip("/"); body=json.dumps({"hypotheses":[PAY,HOLD],"evidence":events}).encode(); req=urllib.request.Request(base+"/v1/evaluate",data=body,method="POST",headers={"Content-Type":"application/json"}); token=os.environ.get("MOOLBASE_SERVICE_TOKEN","");
    if token: req.add_header("Authorization","Bearer "+token)
    try:
        with urllib.request.urlopen(req,timeout=25) as r: out=json.loads(r.read().decode())
    except urllib.error.HTTPError as e: raise RuntimeError("MoolBase HTTP %s: %s"%(e.code,e.read().decode(errors="replace"))) from e
    if not out.get("ok"): raise RuntimeError(str(out))
    return out


def selected(state):
    return next((t for t in state.get("targets",[]) if t.get("id")==state.get("answer")),None)

def evaluate(phase="initial",scenario="bank-change"):
    events=list(GOOD)
    if phase=="final" and scenario=="bank-change": events.append(BANK_CHANGE)
    out=moolbase(events); state=out["latest"]; sel=selected(state); sel_text=sel.get("content") if sel else None
    action="PAY" if state.get("status")=="resolved" and sel_text==PAY else "HOLD"
    reason="MoolBase resolved PAY with no blocking contradiction." if action=="PAY" else "MoolBase is not in a resolved PAY state; autonomous execution is blocked."
    return {"request":REQUEST,"evidence":events,"latest":{"status":state.get("status"),"answer":state.get("answer"),"selected":sel,"targets":state.get("targets",[]),"receipt":state.get("receipt",{}),"action":action,"reason":reason},"moolbase":{"service":out.get("service"),"engine":out.get("engine")}}

def stripe_pay():
    key=os.environ.get("STRIPE_SECRET_KEY","").strip()
    if not key: return {"provider":"stripe","mode":"local-test-no-network","id":"pi_demo_"+uuid.uuid4().hex[:16],"livemode":False,"status":"succeeded","amount":REQUEST["amount_minor"],"currency":"gbp","note":"No network call or money movement."}
    if not key.startswith("sk_test_"): raise ValueError("Only Stripe test keys are accepted")
    form=urllib.parse.urlencode({"amount":REQUEST["amount_minor"],"currency":"gbp","description":"MoolPay test transaction","payment_method":"pm_card_visa","confirm":"true","payment_method_types[]":"card","metadata[request_id]":REQUEST["request_id"],"metadata[moolbase_gate]":"resolved-pay"}).encode(); auth=base64.b64encode((key+":").encode()).decode(); req=urllib.request.Request("https://api.stripe.com/v1/payment_intents",data=form,method="POST",headers={"Authorization":"Basic "+auth,"Content-Type":"application/x-www-form-urlencoded","Idempotency-Key":"moolpay-"+REQUEST["request_id"]})
    with urllib.request.urlopen(req,timeout=20) as r: x=json.loads(r.read().decode())
    return {"provider":"stripe","mode":"test-api","id":x.get("id"),"livemode":x.get("livemode"),"status":x.get("status"),"amount":x.get("amount"),"currency":x.get("currency")}

class Handler(BaseHTTPRequestHandler):
    server_version="MoolPay/0.3"
    def log_message(self,fmt,*args): sys.stderr.write("[moolpay] "+fmt%args+"\n")
    def reply(self,obj,status=200,ctype="application/json"):
        b=obj.encode() if isinstance(obj,str) else json.dumps(obj).encode(); self.send_response(status); self.send_header("Content-Type",ctype); self.send_header("Content-Length",str(len(b))); self.send_header("Cache-Control","no-store"); self.end_headers(); self.wfile.write(b)
    def body(self):
        n=int(self.headers.get("Content-Length","0") or 0); return json.loads(self.rfile.read(n).decode()) if n else {}
    def do_GET(self):
        p=urlparse(self.path).path
        if p=="/": self.reply(HTML,200,"text/html; charset=utf-8"); return
        if p=="/api/health":
            try:
                base=os.environ["MOOLBASE_URL"].rstrip("/");
                with urllib.request.urlopen(base+"/health",timeout=5) as r: h=json.loads(r.read().decode())
                self.reply({"ok":True,"service":"moolpay","moolbase":h,"stripe_test_configured":os.environ.get("STRIPE_SECRET_KEY","").startswith("sk_test_")})
            except Exception as e: self.reply({"ok":False,"error":str(e)},500)
            return
        self.reply({"error":"not found"},404)
    def do_POST(self):
        try:
            p=urlparse(self.path).path; data=self.body()
            if p=="/api/evaluate": self.reply(evaluate(data.get("phase","initial"),"bank-change")); return
            if p=="/api/execute":
                scenario=data.get("scenario","bank-change"); result=evaluate("final",scenario)
                if result["latest"]["action"]!="PAY": execution={"executed":False,"blocked":True,"reason":result["latest"]["reason"],"moolbase_status":result["latest"]["status"]}
                else: execution={"executed":True,"blocked":False,"gateway":stripe_pay(),"moolbase_status":result["latest"]["status"]}
                self.reply({"result":result,"execution":execution}); return
            self.reply({"error":"not found"},404)
        except Exception as e: self.reply({"error":str(e)},500)

def main():
    host=os.environ.get("HOST","0.0.0.0"); port=int(os.environ.get("PORT","8080")); print(f"MoolPay on {host}:{port}"); ThreadingHTTPServer((host,port),Handler).serve_forever()
if __name__=="__main__": main()
