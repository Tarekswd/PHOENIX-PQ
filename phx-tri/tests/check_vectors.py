"""Independent check of PHX-Tri test vectors.

1. Recomputes the combiner ss = SHA3-256(ss_M || ss_H || ss_X || ct_X || pk_X || "PHX-Tri-v0") from the
   exported component values, using Python's hashlib (independent of the C/OpenSSL code path).
2. Compares the regenerated vectors with the committed file, if one is given.

usage: python3 check_vectors.py <generated.txt> [committed.txt]
"""
import hashlib
import sys

LABEL = b"PHX-Tri-v0"


def parse(path):
    vecs, cur = [], {}
    for line in open(path):
        line = line.strip()
        if not line:
            if cur:
                vecs.append(cur)
                cur = {}
            continue
        k, v = (x.strip() for x in line.split("=", 1))
        cur[k] = v
    if cur:
        vecs.append(cur)
    return vecs


gen = parse(sys.argv[1])
assert gen, "no vectors"
for v in gen:
    parts = [bytes.fromhex(v[k]) for k in ("ss_M", "ss_H", "ss_X", "ct_X", "pk_X")]
    assert all(len(p) == 32 for p in parts)
    assert hashlib.sha3_256(b"".join(parts) + LABEL).hexdigest() == v["ss"], f"combiner mismatch in vector {v['count']}"
print(f"ok  combiner recomputed independently for {len(gen)} vectors")

if len(sys.argv) > 2:
    ref = parse(sys.argv[2])
    assert len(ref) == len(gen), "vector count differs from committed file"
    for a, b in zip(gen, ref):
        assert a == b, f"vector {a['count']} differs from committed file"
    print(f"ok  {len(gen)} vectors match the committed file")
