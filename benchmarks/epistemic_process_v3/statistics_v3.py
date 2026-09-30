#!/usr/bin/env python3
"""Frozen statistical primitives for EP-PROCESS-V3."""
from __future__ import annotations
import math, random, statistics
from typing import Mapping, Sequence

BOOTSTRAP_SEED=20261002
BOOTSTRAP_RESAMPLES=20000
ALPHA=0.05

def mean(values):
    if not values: raise ValueError("mean requires non-empty values")
    return float(statistics.fmean(float(x) for x in values))

def median(values):
    if not values: raise ValueError("median requires non-empty values")
    return float(statistics.median(float(x) for x in values))

def exact_mcnemar(control:Sequence[bool],treatment:Sequence[bool]):
    if len(control)!=len(treatment) or not control:
        raise ValueError("McNemar requires equal non-empty paired samples")
    b=sum(bool(c) and not bool(t) for c,t in zip(control,treatment))
    c=sum(not bool(x) and bool(t) for x,t in zip(control,treatment))
    n=b+c
    if n==0: p=1.0
    else:
        tail=sum(math.comb(n,k) for k in range(0,min(b,c)+1))/(2**n)
        p=min(1.0,2.0*tail)
    return {"control_only_success":b,"treatment_only_success":c,
            "discordant_pairs":n,"p_value_two_sided_exact":p}

def _quantile(vals,q):
    if not vals: raise ValueError("quantile requires values")
    if q<=0:return float(vals[0])
    if q>=1:return float(vals[-1])
    pos=(len(vals)-1)*q; lo=math.floor(pos); hi=math.ceil(pos)
    if lo==hi:return float(vals[lo])
    w=pos-lo
    return float(vals[lo]*(1-w)+vals[hi]*w)

def paired_bootstrap_mean_delta(control,treatment,*,seed=BOOTSTRAP_SEED,resamples=BOOTSTRAP_RESAMPLES,alpha=ALPHA):
    if len(control)!=len(treatment) or not control:
        raise ValueError("bootstrap requires equal non-empty paired samples")
    c=[float(x) for x in control]; t=[float(x) for x in treatment]
    rng=random.Random(seed); n=len(c); observed=mean(t)-mean(c); ds=[]
    for _ in range(resamples):
        idx=[rng.randrange(n) for _ in range(n)]
        ds.append(sum(t[i]-c[i] for i in idx)/n)
    ds.sort()
    return {"n_pairs":n,"control_mean":mean(c),"treatment_mean":mean(t),
            "delta_treatment_minus_control":observed,
            "ci_low":_quantile(ds,alpha/2),"ci_high":_quantile(ds,1-alpha/2),
            "confidence_level":1-alpha,"bootstrap_seed":seed,"bootstrap_resamples":resamples}

def paired_binary_summary(control,treatment,*,seed=BOOTSTRAP_SEED,resamples=BOOTSTRAP_RESAMPLES):
    return {**paired_bootstrap_mean_delta(control,treatment,seed=seed,resamples=resamples),
            "exact_mcnemar":exact_mcnemar(control,treatment)}

def paired_continuous_summary(control,treatment,*,seed=BOOTSTRAP_SEED,resamples=BOOTSTRAP_RESAMPLES):
    b=paired_bootstrap_mean_delta(control,treatment,seed=seed,resamples=resamples)
    dif=[float(t)-float(c) for c,t in zip(control,treatment)]
    return {"n_pairs":len(dif),"control_mean":mean(control),"treatment_mean":mean(treatment),
            "control_median":median(control),"treatment_median":median(treatment),
            "mean_delta":b["delta_treatment_minus_control"],
            "median_paired_delta":median(dif),
            "mean_delta_ci_low":b["ci_low"],"mean_delta_ci_high":b["ci_high"],
            "bootstrap_seed":seed,"bootstrap_resamples":resamples}

def holm_adjust(p_values:Mapping[str,float]):
    for name,p in p_values.items():
        if not 0<=float(p)<=1: raise ValueError(f"invalid p-value {name}={p}")
    ordered=sorted(((k,float(v)) for k,v in p_values.items()),key=lambda x:(x[1],x[0]))
    m=len(ordered); running=0.0; adj={}
    for i,(name,p) in enumerate(ordered):
        running=max(running,min(1.0,(m-i)*p)); adj[name]=running
    return {k:adj[k] for k in p_values}
