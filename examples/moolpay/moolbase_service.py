#!/usr/bin/env python3
"""Minimal authenticated HTTP boundary around the real MoolBase evidence lifecycle adapter."""
from __future__ import annotations
import json, os, subprocess, sys, tempfile, traceback
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

PAY = "Payment destination is verified and safe to pay"
HOLD = "Payment should be held because destination risk is unresolved"


def run_moolbase(events, hypotheses):
    binary = Path(os.environ.get("MOOLBASE_BINARY", "build-moolpay/moolbase_customer_showcase")).resolve()
    if not binary.exists():
        raise FileNotFoundError(f"MoolBase adapter not found: {binary}")
    rows=[]
    for e in events:
        if int(e.get("target",-1)) not in (0,1): raise ValueError("target must be 0 or 1")
        if e.get("kind","support") not in {"support","refute","revoke","supersede"}: raise ValueError("invalid evidence kind")
        vals=[str(e.get("id","")),str(e.get("content","")),str(e.get("family","")),str(int(e["target"])),str(e.get("kind","support")),str(int(bool(e.get("verified",False)))),str(e.get("retire",""))]
        if not vals[0] or not vals[1] or not vals[2]: raise ValueError("id, content and family are required")
        if any(any(c in v for c in "\t\n\x00") for v in vals): raise ValueError("evidence contains a forbidden TSV character")
        rows.append("\t".join(vals))
    if not rows: raise ValueError("evidence must be non-empty")
    with tempfile.TemporaryDirectory(prefix="moolbase-service-") as tmp:
        tsv=Path(tmp)/"events.tsv"; tsv.write_text("\n".join(rows)+"\n",encoding="utf-8")
        p=subprocess.run([str(binary),str(Path(tmp)/"db"),str(tsv),*hypotheses],text=True,capture_output=True,timeout=30)
        if p.returncode: raise RuntimeError(p.stderr.strip() or p.stdout.strip() or f"adapter exited {p.returncode}")
        raw=[json.loads(line) for line in p.stdout.splitlines() if line.strip()]
        if len(raw)<len(events)+1: raise RuntimeError("MoolBase returned too few states")
        states=raw[1:1+len(events)]
        bad=next((s for s in states if s.get("ok") is False),None)
        if bad: raise RuntimeError(bad.get("error",str(bad)))
        return states


class Handler(BaseHTTPRequestHandler):
    server_version="MoolBase-Service/0.1"
    def log_message(self,fmt,*args): sys.stderr.write("[moolbase] "+fmt%args+"\n")
    def reply(self,obj,status=200):
        b=json.dumps(obj).encode(); self.send_response(status); self.send_header("Content-Type","application/json"); self.send_header("Content-Length",str(len(b))); self.send_header("Cache-Control","no-store"); self.end_headers(); self.wfile.write(b)
    def body(self):
        n=int(self.headers.get("Content-Length","0") or 0); return json.loads(self.rfile.read(n).decode()) if n else {}
    def do_GET(self):
        if urlparse(self.path).path=="/health":
            binary=Path(os.environ.get("MOOLBASE_BINARY","build-moolpay/moolbase_customer_showcase")).resolve()
            self.reply({"ok":binary.exists(),"service":"moolbase","database":"MoolBase","reasoning":["HypoKosh","DWM"],"adapter_ready":binary.exists()}); return
        self.reply({"error":"not found"},404)
    def do_POST(self):
        try:
            token=os.environ.get("MOOLBASE_SERVICE_TOKEN","")
            if token and self.headers.get("Authorization","")!="Bearer "+token: self.reply({"ok":False,"error":"unauthorized"},401); return
            if urlparse(self.path).path!="/v1/evaluate": self.reply({"error":"not found"},404); return
            data=self.body(); hs=data.get("hypotheses",[PAY,HOLD])
            if not isinstance(hs,list) or len(hs)!=2: raise ValueError("hypotheses must contain exactly two strings")
            states=run_moolbase(data.get("evidence",[]),(str(hs[0]),str(hs[1])))
            self.reply({"ok":True,"service":"moolbase","api_version":"v1","engine":{"database":"MoolBase","reasoning":["HypoKosh","DWM"]},"evidence_count":len(states),"states":states,"latest":states[-1]})
        except ValueError as e: self.reply({"ok":False,"error":str(e)},400)
        except Exception as e: self.reply({"ok":False,"error":str(e),"trace":traceback.format_exc().splitlines()[-4:]},500)


def main():
    host=os.environ.get("HOST","0.0.0.0"); port=int(os.environ.get("PORT","8090")); print(f"MoolBase service on {host}:{port}"); ThreadingHTTPServer((host,port),Handler).serve_forever()
if __name__=="__main__": main()
