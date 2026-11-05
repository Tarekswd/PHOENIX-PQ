**To:** Gorjan Alagic, Fahran Bajaj, Aybars Kocoglu (contact addresses as listed on https://eprint.iacr.org/2025/1444)

**Subject:** Question on "The Best of Both KEMs": C2PRI for HQC's 2025 (v5) transform

---

Dear Gorjan Alagic, Fahran Bajaj and Aybars Kocoglu,

Your paper (ePrint 2025/1444) proves C2PRI for HQC as specified on 2024-10-30 (your Algorithm 16, with K = H(m, u, v)). The HQC specification of 2025-08-22, and the official implementation since v5.0.0, use a different transform. I checked both the code and the specification PDF:

```
Encaps:  (K || theta) = G(H(ek) || m || salt)        G = SHA3-512, |K| = 256 bits
         c_KEM = (PKE.Enc(ek, m; theta), salt)
Decaps:  K' from G(H(ek) || m' || salt) if re-encryption matches, else
         K_bar = J(H(ek) || sigma || c_KEM)          J = SHA3-256, domain-separated
```

As far as I can tell, neither your HQC theorem nor Krämer-Struck-Weishäupl (ePrint 2024/1233) covers this version. I wrote a classical-ROM sketch that it is C2PRI. Model G_1 (the K part of G) and J as independent random oracles; an adversary given (ek, dk, c, K) and making q_G, q_J queries succeeds with probability at most

```
((q_G + 2)^2 + (q_J + 2)^2) / 2^257  +  (q_G + q_J + 2) / 2^256
```

The argument has four cases:
1. Both c and c* pass re-encryption. Then (m*, salt*) != (m, salt), because equal inputs give equal theta and hence c* = c. So this is a G_1 collision.
2. c passes and c* fails. Then a J output must equal a fixed G_1 output.
3. c fails and c* passes. This is the same as case 2 with G and J swapped.
4. Both fail. Then J collides on distinct inputs, since J hashes the whole of c_KEM.

The full note is at https://github.com/Tarekswd/PHOENIX-PQ/blob/main/research/g1.2-hqc-v5-c2pri.md. It is Section 9 of the attached draft, which mainly concerns an implementation audit of HQC.

Would you be willing to tell me whether the argument looks right, or whether I have missed something, for example in how the split SHA3-512 output is modelled? I have not attempted the QROM case; I assume your O2H-based argument for the U_m variants would carry over, but I have not checked it. I would cite your paper as the basis of the approach and would be glad to acknowledge any feedback.

Thank you for your time.

Best regards,
Tarek Ahmadieh
