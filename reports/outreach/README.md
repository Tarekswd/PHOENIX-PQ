# Outreach: what to send, where, in what order, with which attachments

Updated 2026-11-06 after a reply from the liboqs maintainers:

- the `uint64_t*` cast finding is new to liboqs, and an issue is invited;
- the Clang branch in `vect_write_support_to_vector` is already known to them.

| # | Draft | Recipient / channel | Status | Attachments |
|---|---|---|---|---|
| 3 | private reply to the liboqs maintainers (kept outside git) | Email | **Send first** | `phx-tri\hqc-ct-hardening.patch` |
| 2A | [2-liboqs-issues.md](2-liboqs-issues.md), Issue A | Public liboqs GitHub issue | **Send** (invited) | none |
| 2B | [2-liboqs-issues.md](2-liboqs-issues.md), Issue B | | **WITHDRAWN**: already known to the maintainers; do not file | |
| 1 | [1-hqc-team.md](1-hqc-team.md) | HQC GitLab work item / mailing list | **HOLD item 3** (Clang branch + hardening patch) until the liboqs maintainers reply; send the rest when ready | `paper\main.pdf`, `phx-tri\hqc-portability.patch` (+ `hqc-ct-hardening.patch` only if they agree) |
| 1b | [hqc-issue-46-comment.md](hqc-issue-46-comment.md) | Comment on HQC #46 | **Send** (unaffected) | none |

All attachment paths are under `C:\Users\Admin\Desktop\encryption_research\`.

Earlier withdrawal: a review-request email about a C2PRI sketch (2026-11-05), because the result is already proven in Starfighters; see research/FAILED_HYPOTHESES.md, F3.

## Order

1. **Private reply (3).**
2. **liboqs Issue A.**
3. **HQC #46 comment (1b).**
4. **HQC message (1)**, after the maintainers answer.

## Before sending

- [ ] In public places (issues, repository, paper), do not refer to any non-public report or its identifier.
- [ ] In each draft, copy from the line after `---`; the lines above it are notes for you.
