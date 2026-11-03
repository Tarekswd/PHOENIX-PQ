"""Exact per-weight model of the HQC AVX2 find_peaks width bug, checked against simulation.

Model (duplicated RM(1,7), length N2 = 128 * mult, any two non-complementary codewords at distance
HALF = N2 / 2). For a block whose noise has weight w, placed uniformly among the N2 positions:
  - true transform entry:        |t_a| = HALF - w
  - each of the 127 competitors: |t_j| = |2k - w|,  k ~ Hypergeometric(N2, HALF, w)
Reference (ML) decoder errs when some |t_j| > |t_a|.
Official AVX2 decoder caps its peak search at CAP = 1 << (5 + mult // 2); if |t_a| > CAP and some
competitor has CAP < |t_j| < |t_a|, it returns whichever of them has the lower index.
Union bound over 127 competitors; index order taken as 1/2 (uniform messages).

usage: python3 analyze.py <runs_dir>
"""
import glob
import json
import math
import sys
from collections import defaultdict

PARAMS = {  # from src/ref/hqc-N/parameters.h, v5.1.0
    1: dict(N1=46, N2=384, K=16),
    3: dict(N1=56, N2=640, K=24),
    5: dict(N1=90, N2=640, K=32),
}
COMPETITORS = 127


def hyper_tail(N2, half, w, pred):
    """P(pred(k)) for k ~ Hypergeometric(N2, half, w), exact."""
    num = sum(math.comb(half, k) * math.comb(N2 - half, w - k) for k in range(max(0, w - (N2 - half)), min(w, half) + 1) if pred(k))
    return num / math.comb(N2, w)


def model(N2, w):
    half, mult = N2 // 2, N2 // 128 + (N2 % 128 > 0)
    cap = 1 << (5 + mult // 2)
    ta = half - w
    # an exact tie with the true peak goes to the lower index, i.e. is an error about half the time
    p_ref = min(1.0, COMPETITORS * (hyper_tail(N2, half, w, lambda k: abs(2 * k - w) > ta)
                                    + 0.5 * hyper_tail(N2, half, w, lambda k: abs(2 * k - w) == ta))) if ta > 0 else 1.0
    p_bug = 0.0
    if ta > cap:
        p_bug = 0.5 * COMPETITORS * hyper_tail(N2, half, w, lambda k: cap < abs(2 * k - w) < ta)
    return p_ref, p_bug, cap


def binom_tail_log2(n, p, t):
    """log2 P(Bin(n, p) >= t)."""
    if p <= 0:
        return float("-inf")
    terms = [math.lgamma(n + 1) - math.lgamma(i + 1) - math.lgamma(n - i + 1) + i * math.log(p) + (n - i) * math.log1p(-p) for i in range(t, n + 1)]
    m = max(terms)
    return (m + math.log(sum(math.exp(x - m) for x in terms))) / math.log(2)


def load_natural(runs, v):
    c, hw = defaultdict(int), defaultdict(lambda: [0, 0, 0, 0])
    for f in glob.glob(f"{runs}/natural_hqc{v}_*.txt"):
        for line in open(f):
            p = line.split()
            if not p or p[0] == "param":
                continue
            if p[0] == "hw":
                for i in range(4):
                    hw[int(p[1])][i] += int(p[2 + i])
            elif p[0] != "hist":
                c[p[0]] += int(p[1])
    return c, hw


def load_cond(runs, v):
    out = []
    for f in glob.glob(f"{runs}/cond_hqc{v}.txt"):
        for line in open(f):
            m = dict(zip(*[iter(line.split()[1:])] * 2))
            out.append({k: int(x) for k, x in m.items()})
    return sorted(out, key=lambda d: d["w"])


def main():
    runs = sys.argv[1]
    results = {}
    for v, prm in PARAMS.items():
        N1, N2, K = prm["N1"], prm["N2"], prm["K"]
        delta = (N1 - K) // 2
        c, hw = load_natural(runs, v)
        if not c:
            continue
        blocks = c["blocks"]
        print(f"\n===== HQC-{v}: n1={N1} n2={N2} delta={delta}")
        print(f"natural: {c['trials']:,} encryptions, {blocks:,} blocks")
        p_ref_emp = c["ref_err"] / blocks
        print(f"  ref block error rate      {p_ref_emp:.3e}  ({c['ref_err']:,} errors)")
        print(f"  bug-only (avx wrong, ref right): {c['bug_only']}   avx right/ref wrong: {c['avx_only_right']}   fix != ref: {c['fix_ne_ref']}")
        mean_w = sum(w * a[0] for w, a in hw.items()) / blocks
        light = sum(a[0] for w, a in hw.items() if model(N2, w)[2] < N2 // 2 - w) / blocks
        print(f"  mean block noise weight {mean_w:.1f}; fraction of blocks light enough for the bug (|t_a| > cap): {light:.3f}")

        # Model vs simulation, integrated over the empirical weight distribution
        p_ref_model = q_model = 0.0
        for w, a in hw.items():
            pr, pb, _ = model(N2, w)
            p_ref_model += a[0] / blocks * pr
            q_model += a[0] / blocks * pb
        print(f"  model ref error rate (uniform-in-block, union bound): {p_ref_model:.3e}  vs simulated {p_ref_emp:.3e}")
        print(f"  model bug-only rate q: {q_model:.3e}   q / p_ref = {q_model / p_ref_emp:.3e}")
        if c["bug_only"] == 0:
            print(f"  simulation upper bound on q (95%, zero events): {3 / blocks:.2e}")

        # Per-weight check for weights with enough reference errors
        print("  per-weight ref error: simulated vs model (weights with >= 50 sim errors)")
        for w in sorted(hw):
            a = hw[w]
            if a[1] >= 50:
                print(f"    w={w:4d}  blocks={a[0]:>12,}  sim={a[1] / a[0]:.3e}  model={model(N2, w)[0]:.3e}")
        cond = load_cond(runs, v)
        if cond:
            print("  conditional (uniform noise of fixed weight): simulated vs model")
            for d in cond:
                print(f"    w={d['w']:4d}  blocks={d['blocks']:>10,}  sim={d['ref_err'] / d['blocks']:.3e}  model={model(N2, d['w'])[0]:.3e}  bug_only={d['bug_only']}  fix!=ref={d['fix_ne_ref']}")
        wmax = max(range(N2 // 2), key=lambda w: model(N2, w)[1])
        print(f"  bug probability peaks at w={wmax}: P_bug={model(N2, wmax)[1]:.3e}")

        # Failure-rate implication under an independent-block binomial model (relative change only)
        l0 = binom_tail_log2(N1, p_ref_emp, delta + 1)
        l1 = binom_tail_log2(N1, p_ref_emp + q_model, delta + 1)
        print(f"  binomial-model DFR: log2 {l0:.2f} -> {l1:.2f}  (relative increase {2 ** (l1 - l0) - 1:.2e}; first-order (delta+1)*q/p = {(delta + 1) * q_model / p_ref_emp:.2e})")
        results[f"HQC-{v}"] = dict(blocks=blocks, encryptions=c["trials"], p_ref=p_ref_emp, p_ref_model=p_ref_model,
                                   q_model=q_model, q_over_p=q_model / p_ref_emp, bug_only_sim=c["bug_only"],
                                   fix_ne_ref_sim=c["fix_ne_ref"], mean_w=mean_w, light_fraction=light,
                                   dfr_rel_increase=2 ** (l1 - l0) - 1)
    json.dump(results, open(f"{runs}/summary.json", "w"), indent=2)


if __name__ == "__main__":
    main()
