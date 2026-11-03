# Security Status

No construction of our own exists yet, and no security claims are made.

## Third-party audit: HQC (see reports/hqc-ct-audit.md)

| Finding | Status | Public? |
|---|---|---|
| AVX2 find_peaks strict-aliasing miscompile at gcc -O2/-Os | Known upstream (HQC #39, #46); independently reproduced | Yes |
| AVX2 find_peaks secret-dependent branch at -O3 | Known upstream (HQC #11) | Yes |
| Rejection-sampler variable iteration count | By design | Yes |
| Class E items (ref, clang) | Under triage | Withheld until triage/disclosure |
