# Security Status

No construction of our own exists yet, and no security claims are made.

## Third-party audit: HQC (see reports/hqc-ct-audit.md)

| Finding | Status | Public? |
|---|---|---|
| AVX2 find_peaks strict-aliasing miscompile at gcc -O2/-Os | Known upstream (HQC #39, #46); independently reproduced | Yes |
| AVX2 find_peaks secret-dependent branch at -O3 | Known upstream (HQC #11) | Yes |
| Rejection-sampler variable iteration count | By design | Yes |
| Class E items (ref, clang) | Under triage | Withheld until triage/disclosure |
| AVX2 find_peaks width defect (#46 defect 1): DFR impact | NEW measurement: negligible (q/p = 8e-9 HQC-1, ~0 HQC-3/5); see reports/hqc-findpeaks-dfr.md | Yes (underlying bug already public) |
| liboqs inherits HQC #31 casts (Armv7 alignment UB) | Observed in source; no failure under QEMU; untested on hardware | Yes (upstream issue public) |
