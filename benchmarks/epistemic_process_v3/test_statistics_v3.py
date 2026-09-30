#!/usr/bin/env python3
from statistics_v3 import exact_mcnemar, holm_adjust, paired_binary_summary, paired_bootstrap_mean_delta

def main():
    x=exact_mcnemar([False]*8,[True]*8)
    assert x["discordant_pairs"]==8
    assert abs(x["p_value_two_sided_exact"]-0.0078125)<1e-12
    tied=exact_mcnemar([False,True],[True,False])
    assert tied["p_value_two_sided_exact"]==1.0
    a=paired_bootstrap_mean_delta([0,0,1,1],[1,1,1,1],seed=20261002,resamples=2000)
    b=paired_bootstrap_mean_delta([0,0,1,1],[1,1,1,1],seed=20261002,resamples=2000)
    assert a==b
    assert a["delta_treatment_minus_control"]==0.5
    h=holm_adjust({"a":.01,"b":.04,"c":.03})
    assert abs(h["a"]-.03)<1e-12 and abs(h["b"]-.06)<1e-12 and abs(h["c"]-.06)<1e-12
    s=paired_binary_summary([False,False,True,True],[True,True,True,True],seed=7,resamples=1000)
    assert s["exact_mcnemar"]["discordant_pairs"]==2
    for p in (-.1,1.1):
        try: holm_adjust({"bad":p})
        except ValueError: pass
        else: raise AssertionError("invalid p accepted")
    print("cycle6_v3_statistics=passed")

if __name__=="__main__": main()
