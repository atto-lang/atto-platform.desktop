# Desktop checkout verifier trust boundary

This document describes an **unapproved candidate implementation** on desktop
PR #16. It is not proof of protected-source provenance, reviewer approval or
successful GitHub-hosted CI.

## Threat model

A pull request can modify `scripts/verify-checkout-raw.py`. If a CI job
executes that same checkout's verifier, an attacker can replace the program with
`sys.exit(0)`, defeating any subsequent checked-source integrity claim.

The desktop workflow now:

1. Checks out exact **same-repository** commit
   `27f2c9aa6d67a7e2302ba27d643907fa28c352f9`, using a SHA-pinned
   `actions/checkout` and non-persistent credentials.
2. Uses **inline Python** in the workflow (not a PR-file helper) to verify the
   verifier bytes' SHA-256
   `727948bdaa80a5a2f8b391673eb7ad01235dcae78d4de7481c80aae74fa4e156`.
   A mismatched source aborts before writing the snapshot.
3. Copies those verified bytes to
   `$RUNNER_TEMP/ores-desktop-pinned-verifier.py`, outside the working checkout.
4. Checks out the exact PR head with `persist-credentials: false`.
5. Executes the *saved* verifier from `RUNNER_TEMP` (never the PR-controlled
   verifier). It still checks that all raw candidate file bytes, index and Git
   `HEAD` match the expected commit.
6. Runs host, sanitizer and checkout-negative tests only afterward. The Linux,
   macOS and Windows jobs have identical pin/digest semantics.

The pinned verifier version includes defenses against external Git content
filters, fsmonitor, environment `GIT_*` overrides, unexpected index flags,
symlink traversal and changes during reads, and Windows reparse points.

## Verified synthetic negative cases

`python3 -m unittest discover -s scripts -p 'test_pinned_verifier_guard.py' -v`
exercises eight source-free regression cases, including:

- Reject a modified verifier before snapshotting; do not write the snapshot.
- Save the verifier outside the candidate checkout at the exact SHA-256 digest.
- Change the candidate's verifier after snapshotting; prove the saved verifier
  stays unchanged.
- In a real temporary Git checkout, mutate a tracked source file and replace
  the candidate verifier with a program that exits successfully. Prove the
  forged candidate verifier lies, but the pinned verifier **rejects** the
  tampered file.
- Check every CI job has identical commit/digest pins and invokes the
  runner-temp version. Assert separate Windows/PowerShell behavior.

## Crucial non-guarantees / blocking gates

**The digest pin is immutable content identification, not independent approval.**

- The PR also controls the workflow `.github/workflows/ci.yml` on a
  `pull_request` job. A malicious PR could edit the pin, disable the snapshot,
  or remove the entire check. The new self-contained candidate workflow
  therefore **cannot establish an independent trust boundary while `main`
  is unprotected and this pin has not received a separate security approval**.
- The SHA-pinned verifier commit above comes from a **draft PR branch**, not a
  previously reviewed and protected source policy release. It must be reviewed,
  approved and promoted to a protected policy root before being accepted as
  independently trusted. Merely reusing an immutable commit SHA is insufficient.
- The verifier checks *point-in-time workspace integrity* against the PR's
  expected Git tree; it does **not** prove PR code is benign, sandboxed,
  authorized for export, or immutable during a later build.
- A later build executes PR-controlled CMake and test code. This requires
  ordinary unprivileged, read-only-token, no-secrets, isolated runner controls.
  Do not change to `pull_request_target` and then execute PR code: GitHub warns
  against that privilege escalation.
- This repository's `main` is currently unprotected; private rulesets return
  plan/feature 403, branch protection returns integration-permission 403, and
  source Actions jobs fail before runner allocation.
- Source-transfer incidents in conformance #22/#77 and Java source-publisher
  authorization #962 remain open. Never use a cross-organization private Git
  tree carrier without independent disposition and exact destination ID,
  source SHA/root tree, protected branch and approved workflow.

## Release acceptance

The invariant becomes enforceable only when the **workflow itself** and
the pinned verifier have an independently approved, protected immutable root,
and a separate exact-head policy check cannot be modified or bypassed by a PR.
A recommended future follow-up is a minimal **metadata-only** default-branch
policy workflow that never executes PR code in a privileged event; ordinary
source-bearing tests should continue on isolated, read-only, no-secret PR jobs.

Require independently observed exact-head Linux/Windows/macOS jobs with actual
runner IDs and executed successful steps; independent reviewers; strict required
checks; and protected-branch merge enforcement. Until then keep PR #16 draft.

References:
- https://docs.github.com/en/actions/reference/security/securely-using-pull_request_target
- https://github.com/ores-truffle-oreslang/oreslang-platform-conformance/issues/170
- https://github.com/ores-truffle-oreslang/oreslang-source.java/issues/963
