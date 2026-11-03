# Open Questions

- Q1: Is the FIPS 207 (HQC-KEM) initial public draft published, and which HQC version does it fix? Secondary sources disagree. Resolve from csrc.nist.gov before any HQC audit.
- Q2: Which FO variant does FIPS 207 HQC use (salt, implicit rejection), and is it in the class ABK25 proves C2PRI? (G1.2)
- Q3: RESOLVED 2026-11-03. CT-KAT covers ML-KEM, ML-DSA, SLH-DSA, Falcon; not HQC.
- Q1 update: HQC README says "Till the FIPS 207 standard is available", so it is not final; draft status still unconfirmed.
- Q7: Which HQC version do PQClean and liboqs ship? (RQ4)
- Q4: What is Panther's "Market-Theoretic Security Framework", and does it hold up? Read before citing.
- Q5: Is a three-way combiner justified in practice, or is X-Wing plus a code-based backup deployed separately strictly better? Needs a threat-model argument, not just a construction.
- Q6: Authors for several bib entries marked TODO.
