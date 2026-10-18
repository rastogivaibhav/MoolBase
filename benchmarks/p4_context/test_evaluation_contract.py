#!/usr/bin/env python3
"""Native receipt / boundary mutation tests for MOOL-P4-CONTEXT-001."""
import argparse
import csv
import json
import sys
import tempfile
from pathlib import Path

from evaluate import evaluate, p4_features_stocks, parse_stocks, prequential_native_rows

def expect_rejection(callback, reason):
    try:
        callback()
    except (ValueError, KeyError, IndexError, OSError):
        return
    raise AssertionError("P4 mutation was not rejected: " + reason)

def rewrite(path,header,rows):
    with open(path,"w",newline="",encoding="utf-8") as f:
        writer=csv.DictWriter(f,fieldnames=header)
        writer.writeheader()
        writer.writerows(rows)

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--weather",required=True)
    p.add_argument("--stocks",required=True)
    p.add_argument("--native",required=True)
    a=p.parse_args()
    original=evaluate(a.weather,a.stocks,a.native)
    assert original["native_parity"]=="PASS"

    with open(a.native,newline="",encoding="utf-8") as f:
        rd=csv.DictReader(f)
        fields=rd.fieldnames
        rows=list(rd)
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp=Path(tmpdir)/"altered.csv"
        changed=[dict(x) for x in rows]
        # A forged native prediction cannot quietly change out-of-sample skill.
        idx=next(i for i,x in enumerate(changed)
                 if x["domain"]=="stocks")
        changed[idx]["p4"]="0.99999"
        rewrite(tmp,fields,changed)
        expect_rejection(lambda:evaluate(a.weather,a.stocks,tmp),
                         "forged native finance prediction")

        changed=[dict(x) for x in rows]
        changed[idx]["observed"]=str(1-int(changed[idx]["observed"]))
        rewrite(tmp,fields,changed)
        expect_rejection(lambda:evaluate(a.weather,a.stocks,tmp),
                         "forged finance label")

        changed=[dict(x) for x in rows]
        changed.append(dict(changed[idx]))
        rewrite(tmp,fields,changed)
        expect_rejection(lambda:prequential_native_rows(tmp),
                         "duplicate finance record")

        changed=[dict(x) for x in rows]
        changed.pop(idx)
        rewrite(tmp,fields,changed)
        expect_rejection(lambda:evaluate(a.weather,a.stocks,tmp),
                         "missing finance record")

        changed=[dict(x) for x in rows]
        changed[idx]["p1"]="nan"
        rewrite(tmp,fields,changed)
        expect_rejection(lambda:prequential_native_rows(tmp),
                         "nonfinite baseline probability")

    stocks=parse_stocks(a.stocks)
    symbol=sorted(stocks)[0]
    sequence=stocks[symbol]
    # Current observation price must never influence features constructed
    # from the previously available four completed records.
    target=sequence[5]
    original=p4_features_stocks(sequence,5)
    tampered=[dict(x) for x in sequence]
    tampered[5]={"date":target["date"],"price":target["price"]*12.0}
    assert original==p4_features_stocks(tampered,5)
    print("p4_adversarial_evaluator=PASS cases=6")

if __name__=="__main__":
    main()
