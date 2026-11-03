"""Aggregate Valgrind memcheck logs from run_ct.sh into findings tables.

usage: python3 parse_vg.py <out_dir> [<out_dir> ...] --dest <results_dir>

Writes:
  findings_raw.tsv  one row per (build, mode, error)
  findings.tsv      one row per unique (kind, function, location), with the builds/modes that hit it
"""
import argparse
import collections
import csv
import re
from pathlib import Path

HEAD = re.compile(r"^==\d+== (Conditional jump or move depends on uninitialised value\(s\)|"
                  r"Use of uninitialised value of size \d+|Syscall param .*|Invalid .*)$")
FRAME = re.compile(r"^==\d+==\s+(?:at|by) 0x[0-9A-F]+: (\S+) \((.*)\)$")
KIND = {"Conditional": "branch", "Use": "mem-index", "Syscall": "syscall", "Invalid": "invalid-access"}


def parse_log(path):
    errs, cur = [], None
    for line in path.read_text(errors="replace").splitlines():
        m = HEAD.match(line)
        if m:
            cur = {"kind": KIND[m.group(1).split()[0]], "stack": []}
            errs.append(cur)
            continue
        f = FRAME.match(line)
        if f and cur is not None:
            cur["stack"].append(f"{f.group(1)} ({f.group(2).split('/')[-1]})")
        elif cur is not None and not line.strip("= 0123456789"):
            cur = None
    return errs


def build_fields(build):
    # label-arch-cc-opt-variant, label may itself contain dashes (e.g. v5.1.0)
    parts = build.rsplit("-", 4)
    return dict(zip(["version", "arch", "cc", "opt", "variant"], parts))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out_dirs", nargs="+")
    ap.add_argument("--dest", required=True)
    a = ap.parse_args()
    dest = Path(a.dest)
    dest.mkdir(parents=True, exist_ok=True)

    raw, agg = [], collections.defaultdict(set)
    for od in a.out_dirs:
        for log in sorted(Path(od).glob("*/vg.*.log")):
            build, mode = log.parent.name, log.name[3:-4]
            for e in parse_log(log):
                top = e["stack"][0] if e["stack"] else "?"
                row = {"build": build, "mode": mode, "kind": e["kind"], "top": top,
                       "stack": " <- ".join(e["stack"])}
                row.update(build_fields(build))
                raw.append(row)
                agg[(e["kind"], top)].add((build, mode))

    cols = ["build", "version", "arch", "cc", "opt", "variant", "mode", "kind", "top", "stack"]
    with open(dest / "findings_raw.tsv", "w", newline="") as fh:
        w = csv.DictWriter(fh, cols, delimiter="\t")
        w.writeheader()
        w.writerows(raw)

    with open(dest / "findings.tsv", "w", newline="") as fh:
        w = csv.writer(fh, delimiter="\t")
        w.writerow(["kind", "location", "n_hits", "versions", "archs", "compilers", "opts", "modes"])
        for (kind, top), hits in sorted(agg.items(), key=lambda kv: -len(kv[1])):
            f = [build_fields(b) for b, _ in hits]
            w.writerow([kind, top, len(hits),
                        ",".join(sorted({x["version"] for x in f})),
                        ",".join(sorted({x["arch"] for x in f})),
                        ",".join(sorted({x["cc"] for x in f})),
                        ",".join(sorted({x["opt"] for x in f})),
                        ",".join(sorted({m for _, m in hits}))])
    print(f"{len(raw)} raw errors, {len(agg)} unique (kind, location)")


if __name__ == "__main__":
    main()
