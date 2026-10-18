#!/usr/bin/env python3
"""P4 frozen cross-domain native contextual-learning evaluator (stdlib only).

NOTE: The weather data and P3 logistic result are already known from P3.
Finance P4 is the independent prespecified NEW primary evaluation. No tuning.
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import math
import random
import statistics
import sys
from collections import defaultdict, deque
from pathlib import Path

SEED = 20261018
BOOTSTRAPS = 5000
BLOCK = 14
MODELS = ("p1", "p4", "rolling")
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "p3_weather"))
from evaluate import source_rows as read_weather, logistic_features as weather_features

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def parse_stocks(path):
    source = defaultdict(list)
    with open(path, newline="", encoding="utf-8") as f:
        rd = csv.DictReader(f)
        if rd.fieldnames != ["symbol","date","price"]:
            raise ValueError("source financial schema mismatch")
        for row in rd:
            if None in row or any(v is None or v=="" for v in row.values()):
                raise ValueError("financial missing field")
            try:
                date=dt.datetime.strptime(row["date"],"%b %d %Y").date()
                price=float(row["price"])
            except Exception as e:
                raise ValueError("financial malformed date/price") from e
            if not math.isfinite(price) or price<=0:
                raise ValueError("invalid stock price")
            group=source[row["symbol"]]
            if group and group[-1]["date"]>=date:
                raise ValueError("stock date duplicate or not chronological")
            group.append({"date":date,"price":price})
    if len(source)<3:
        raise ValueError("too few finance ticker streams")
    return source

def p4_features_stocks(rows,i):
    if i<4: raise ValueError("need four previous prices")
    def clamp(x): return max(-1.0,min(1.0,x))
    returns=[clamp(rows[i-k]["price"]/rows[i-k-1]["price"]-1.0)
             for k in (1,2,3)]
    mean=sum(returns)/3.0
    stdev=math.sqrt(sum((x-mean)**2 for x in returns)/3.0)
    month=rows[i]["date"].month
    angle=2*math.pi*month/12.0
    return [1.0,returns[0],returns[1],mean,stdev,
            1.0 if returns[0]>0 else 0.0, math.sin(angle),math.cos(angle)]

def sigmoid(z):
    return 1.0/(1.0+math.exp(-max(-15.,min(15.,z))))

def prequential_native_rows(path):
    groups=defaultdict(dict)
    with open(path, newline="", encoding="utf-8") as f:
        rd=csv.DictReader(f)
        if rd.fieldnames!=["domain","symbol","date","observed","p1","p4"]:
            raise ValueError("native schema drift")
        for raw in rd:
            key=(raw["domain"],raw["symbol"])
            if key[0] not in ("weather","stocks"): raise ValueError("unknown domain")
            date=dt.date.fromisoformat(raw["date"])
            if date in groups[key]: raise ValueError("duplicate prediction")
            if raw["observed"] not in ("0","1"): raise ValueError("invalid label")
            p1,p4=float(raw["p1"]),float(raw["p4"])
            if not all(math.isfinite(x) and 0 <= x <= 1 for x in (p1,p4)):
                raise ValueError("invalid probability")
            groups[key][date]={"date":date,"y":int(raw["observed"]),
                               "p1":p1,"p4":p4}
    return groups

def test_native_parity(native,weather,stocks):
    assembled=[]
    counts={"weather":0,"stocks":0}
    for city,entries in weather.items():
        symbol="NewYork" if city=="New York" else "Seattle"
        forecasted=native[("weather",symbol)]
        if len(forecasted)!=len(entries)-1: raise ValueError("weather row missing")
        w=[0.0]*7
        positives=0
        completed=[]
        for i,record in enumerate(entries):
            y=int(record["precipitation"]>0)
            if i>0:
                date=record["date"]
                raw=forecasted.get(date)
                if raw is None or raw["y"]!=y:
                    raise ValueError("weather native missing/mismatched label")
                x=weather_features(record, entries[i-1])
                expected=sigmoid(sum(a*b for a,b in zip(w,x)))
                beta=(positives+1)/(i+2)
                if abs(raw["p4"]-expected)>1e-9: raise ValueError(f"weather native/context training mismatch: {symbol} {date}")
                if abs(raw["p1"]-beta)>1e-9: raise ValueError(f"weather native/P1 mismatch: {symbol} {date}")
                rolled=(1+sum(completed[-14:]))/(2+len(completed[-14:]))
                if date.year>=2014:
                    assembled.append(dict(domain="weather",symbol=symbol,
                                          date=date.isoformat(),y=y,p1=raw["p1"],
                                          p4=raw["p4"],rolling=rolled,
                                          period="dev" if date.year==2014 else "test"))
                error=expected-y
                w=[w[j]-0.12*(error*x[j]+(.0005*w[j] if j else 0.0))
                   for j in range(len(w))]
                counts["weather"]+=1
            positives+=y
            completed.append(y)
    for symbol,entries in stocks.items():
        expected_rows=entries[4:] if len(entries)>=54 else []
        forecasted=native[("stocks",symbol)]
        if len(forecasted)!=len(expected_rows): raise ValueError("financial rows missing")
        if not expected_rows: continue
        w=[0.0]*8
        positive=0
        completed=[]
        n=len(expected_rows)
        warmup=int(.6*n)
        dev_end=int(.8*n)
        for i,rec in enumerate(expected_rows):
            date=rec["date"]
            row=forecasted.get(date)
            y=int(rec["price"]>entries[i+3]["price"])
            if row is None or row["y"]!=y:
                raise ValueError("financial native missing/mismatched target")
            xs=p4_features_stocks(entries,i+4)
            expected=sigmoid(sum(a*b for a,b in zip(w,xs)))
            beta=(1+positive)/(2+i)
            if abs(row["p4"]-expected)>1e-9:
                raise ValueError(f"finance native contextual parity: {symbol} {date}")
            if abs(row["p1"]-beta)>1e-9:
                raise ValueError(f"finance native Beta parity: {symbol} {date}")
            rolling=(1+sum(completed[-4:]))/(2+len(completed[-4:]))
            period="warmup" if i<warmup else "dev" if i<dev_end else "test"
            if period!="warmup":
                assembled.append(dict(domain="stocks",symbol=symbol,
                                      date=date.isoformat(),y=y,p1=row["p1"],
                                      p4=row["p4"],rolling=rolling,period=period))
            error=expected-y
            w=[w[j]-.12*(error*xs[j]+(.0005*w[j] if j else 0.0))
               for j in range(len(w))]
            positive+=y
            completed.append(y)
            counts["stocks"]+=1
    if sum(counts.values())!=sum(len(g) for g in native.values()):
        raise ValueError("unexpected native groups or extra output records")
    return assembled,counts

def brier(p,y): return (p-y)**2

def log_loss(p,y):
    p=max(1e-12,min(1.-1e-12,p))
    return -(y*math.log(p)+(1-y)*math.log(1-p))

def metrics(rows):
    if not rows: raise ValueError("empty evaluation sample")
    result={"n":len(rows),"prevalence":sum(r["y"] for r in rows)/len(rows)}
    for model in MODELS:
        probs=[r[model] for r in rows]
        bands=[[] for _ in range(10)]
        for p,y in zip(probs,(r["y"] for r in rows)):
            bands[min(9,int(p*10))].append((p,y))
        ece=sum(len(b)/len(rows)*abs(statistics.mean(x for x,_ in b)-
                                    statistics.mean(y for _,y in b))
                for b in bands if b)
        result[model]={"brier":statistics.mean(brier(r[model],r["y"]) for r in rows),
                       "log_loss":statistics.mean(log_loss(r[model],r["y"]) for r in rows),
                       "ece10":ece}
    result["p1_minus_p4_brier"]=result["p1"]["brier"]-result["p4"]["brier"]
    return result

def bootstrap_finance(rows):
    bysymbol=defaultdict(list)
    for r in rows: bysymbol[r["symbol"]].append(brier(r["p1"],r["y"])-brier(r["p4"],r["y"]))
    rng=random.Random(SEED)
    samples=[]
    lengths={k:len(v) for k,v in bysymbol.items()}
    for _ in range(BOOTSTRAPS):
        total=0.0
        n_total=0
        for symbol,vals in sorted(bysymbol.items()):
            series=[]
            while len(series)<len(vals):
                k=rng.randrange(len(vals))
                series.extend(vals[(k+j)%len(vals)] for j in range(BLOCK))
            sample=series[:len(vals)]
            total+=sum(sample)
            n_total+=len(sample)
        samples.append(total/n_total)
    samples.sort()
    return [samples[int(.025*BOOTSTRAPS)],samples[int(.975*BOOTSTRAPS)]]

def evaluate(weather_file,stocks_file,native_file):
    weather=read_weather(weather_file)
    stocks=parse_stocks(stocks_file)
    native=prequential_native_rows(native_file)
    rows,counts=test_native_parity(native,weather,stocks)
    test_weather=[r for r in rows if r["domain"]=="weather" and r["period"]=="test"]
    test_finance=[r for r in rows if r["domain"]=="stocks" and r["period"]=="test"]
    if len(test_weather)!=730 or len({r["symbol"] for r in test_finance})<3:
        raise ValueError("missing preregistered holdout observations")
    test_weather_metrics=metrics(test_weather)
    test_finance_metrics=metrics(test_finance)
    finance_by_symbol={symbol:metrics([r for r in test_finance if r["symbol"]==symbol])
                       for symbol in sorted({r["symbol"] for r in test_finance})}
    weather_by_city={city:metrics([r for r in test_weather if r["symbol"]==city])
                     for city in ("Seattle","NewYork")}
    ci=bootstrap_finance(test_finance)
    improved=test_finance_metrics["p1_minus_p4_brier"]>=.01
    not_worse=all(m["p1_minus_p4_brier"]>=-.03 for m in finance_by_symbol.values())
    earned=improved and ci[0]>0 and not_worse
    weather_gain=test_weather_metrics["p1_minus_p4_brier"]>=.01
    report={
        "experiment":"MOOL-P4-CONTEXT-001",
        "status":"COMPLETE",
        "finance_primary_claim":"EARNED" if earned else "NOT_EARNED",
        "two_domain_claim":"EARNED" if earned and weather_gain else "NOT_EARNED",
        "native_parity":"PASS",
        "preregistration":"benchmarks/p4_context/PREREGISTRATION.md",
        "weather_source_sha256":digest(weather_file),
        "stocks_source_sha256":digest(stocks_file),
        "native_predictions_sha256":digest(native_file),
        "native_prediction_counts":counts,
        "weather_test_pooled":test_weather_metrics,
        "weather_per_city":weather_by_city,
        "finance_test_pooled":test_finance_metrics,
        "finance_per_symbol":finance_by_symbol,
        "finance_95ci_p1_minus_p4_block14":ci,
        "thresholds":{"finance_brier_delta_min":.01,"ci_low_strict_gt":0,"per_symbol_worst_delta_min":-.03,
                      "weather_two_domain_brier_delta_min":.01},
        "stock_test_rows":len(test_finance),
        "weather_test_rows":len(test_weather),
        "limitations":[
          "public retrospective weather and monthly stock datasets, not blinded or authenticated",
          "outcomes used for later online test predictions only after precommitment",
          "only one logistic unit; no learned causal world transition",
          "finance symbol time-series correlate; only five stock symbols",
          "weather parity reuses P3 model settings and is known from prior evaluation",
          "bootstrap 14-observation window lacks guaranteed finite-sample validity",
        ],
    }
    return report

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--weather",required=True)
    parser.add_argument("--stocks",required=True)
    parser.add_argument("--native",required=True)
    parser.add_argument("--out",required=True)
    a=parser.parse_args()
    r=evaluate(a.weather,a.stocks,a.native)
    out=Path(a.out)
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(r,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    print(json.dumps({k:r[k] for k in ("experiment","status","native_parity","finance_primary_claim","two_domain_claim","stock_test_rows")},sort_keys=True))
    for domain,per in (("weather",r["weather_test_pooled"]),("stocks",r["finance_test_pooled"])):
        print(f'{domain} P1={per["p1"]["brier"]:.6f} P4={per["p4"]["brier"]:.6f} rolling={per["rolling"]["brier"]:.6f} delta={per["p1_minus_p4_brier"]:.6f}')
    print("finance_95ci=",r["finance_95ci_p1_minus_p4_block14"])

if __name__=="__main__":
    main()
