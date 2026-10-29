# Desktop source-local CI administration and release gates

> Operator runbook, not a CI success attestation. This document must **not**
> be used as approval to export private source, merge without independent review,
> or disable required checks. GitHub org administration is required.

## Current source-local evidence (2026-10-29)

- [Desktop source-free canary run 40814383077](https://github.com/ores-truffle-oreslang/oreslang-platform.desktop/actions/runs/40814383077), attempt 2: job `123258222240` failed with `runner_id=0`, empty runner name and no executed steps.
- [Desktop PR #16 security CI run 40821506560](https://github.com/ores-truffle-oreslang/oreslang-platform.desktop/actions/runs/40821506560): all five Linux GCC, Linux Clang, Linux sanitizers, Windows and macOS jobs `runner_id=0`, `steps=[]`.
- Alternate private-org [source-free canary 40817813629](https://github.com/honeypot-r-us-test/hnpt-e2e/actions/runs/40817813629) ran on a real runner. This confirms admission *only in that test org*; it is not a source-transfer authorization or proof of current budget.
- `main` was verified `protected=false` and required-check enforcement `off`. A GitHub App branch-protection GET returned `403 Resource not accessible by integration`. The integration cannot administer the organization despite its reported repo role.

**Do not guess** that a `runner_id=0` failure means exhausted GitHub Actions minutes. Billing, org policy and runner-admission annotations need privileged verification.

## 1. Restore source-organization runner admission

1. As an authorized `ores-truffle-oreslang` administrator, inspect **Organization Settings → Billing & licensing**: Actions budget, minutes, expenditure limits, payment eligibility and any denied spending. Record the actual GitHub diagnostic, not an inference.
2. Inspect **Organization Settings → Actions → General**: hosted-runner and private-repository eligibility, policy restrictions, allowed marketplace actions and any repository allowlists or runner groups. Inspect repository Actions settings too.
3. Open the failed source-local canary run and exact job; collect any annotation/admission banner. Do not disclose billing secrets or tokens in issues.
4. After fixing the *verified* admission problem, rerun the **source-free, no-checkout** desktop canary before compiling source. Record run ID, attempt, exact commit, job ID, a **nonzero runner ID**, successful executed steps and the actual annotation. An API-accepted rerun alone does not pass the gate.
5. If settings appear healthy and admission still fails, open GitHub Support with the above IDs and sanitized screenshots. Track in [Java org-wide runner issue #963](https://github.com/ores-truffle-oreslang/oreslang-source.java/issues/963).

A privileged `gh` session can examine read-only evidence without revealing tokens:

```sh
gh api repos/ores-truffle-oreslang/oreslang-platform.desktop/actions/runs/40814383077   --jq '{id,run_attempt,status,conclusion,head_sha}'
gh api repos/ores-truffle-oreslang/oreslang-platform.desktop/actions/runs/40814383077/jobs   --jq '{jobs: [.jobs[] | {id,name,status,conclusion,runner_id,runner_name,step_count:(.steps|length)}]}'
```

## 2. Protect every platform/conformance `main` before merging

These seven private repositories need independently verified enforcement:

- `ores-truffle-oreslang/oreslang-platform.mobile`
- `ores-truffle-oreslang/oreslang-platform.iot`
- `ores-truffle-oreslang/oreslang-platform.embedded`
- `ores-truffle-oreslang/oreslang-platform.desktop`
- `ores-truffle-oreslang/oreslang-platform.fpga`
- `ores-truffle-oreslang/oreslang-platform.asic`
- `ores-truffle-oreslang/oreslang-platform-conformance`

Use **Rules → Rulesets** (or equivalent protected-branch configuration), targeting `main`:
- require a pull request before merge, independently submitted approval, dismiss stale reviews and require CODEOWNERS approval when the CODEOWNERS file exists on the protected branch;
- block force-push/deletion/direct writes; apply enforcement to admins and Apps, with no unreviewed bypass;
- require the real exact-head CI/security check names from each repo, pinned to the genuine trusted GitHub App where GitHub supports it; require branch up-to-date/strict status checks;
- prohibit manual green commit statuses, disabling checks to merge and unprotected bootstrap merges.

**Bootstrap:** require PR/independent review and restrict writes *before* adding the missing desktop CODEOWNERS file in PR #14. If CI is still unavailable, keep the PR pending. After CODEOWNERS is merged through the enforced process, verify required-owner-review enforcement before declaring the gate complete.

Acceptance requires GitHub API/administrator UI evidence that all seven branches are protected, rulesets are active, actual approval requirements work, checks have the correct provider and no bypass is available. Track in [conformance #170](https://github.com/ores-truffle-oreslang/oreslang-platform-conformance/issues/170). A repository's `protected` flag by itself is insufficient to prove all criteria.

## 3. Complete source-local acceptance, with exact-head provenance

Once the runner and protected branch are working, review the latest heads of:
- [PR #14: governance, CODEOWNERS and source-free canary](https://github.com/ores-truffle-oreslang/oreslang-platform.desktop/pull/14)
- [PR #16: filter-free checkout verification](https://github.com/ores-truffle-oreslang/oreslang-platform.desktop/pull/16)
- [PR #15: exhaustive host and mock-SDL lifecycle CTests](https://github.com/ores-truffle-oreslang/oreslang-platform.desktop/pull/15)

Each PR is currently draft. Existing Linux local evidence is useful but **not** hosted CI. Rerun source-local Linux GCC/Clang/ASan+UBSan and Windows/macOS against the reviewed exact commit/tree. Require raw executed steps, test counts, no sanitizer errors, and no unreviewed post-review change. PR #16's Windows `CreateFileW` reparse-point paths require **actual Windows execution**. PR #15's test-only SDL shim is **not** a real SDL3 GUI test. A real SDL3 lifecycle and Oreslang LLVM guest-to-host linkage require separate acceptance.

Do not assume three independent PRs based on `main` can be merged without revalidating after each preceding merge. After review, merge in dependency order **#14 → #16 → #15**, only through the protected process; rebase/reconcile exact SHA and rerun required checks as appropriate.

## 4. Private-source carrier is separately blocked

Open incidents [conformance #22](https://github.com/ores-truffle-oreslang/oreslang-platform-conformance/issues/22), [#77](https://github.com/ores-truffle-oreslang/oreslang-platform-conformance/issues/77), and [Java source authorization #962](https://github.com/ores-truffle-oreslang/oreslang-source.java/issues/962) prohibit treating the private alternate-org runner as an approved destination.

A future source-export request requires independent disposition of incidents and explicit approval of the **numeric destination repository ID**, exact source commit and root tree, immutable private carrier workflow, reviewer identity and retention/purge plan. No Git tree mirroring, blob transfer, automatic source export or CI bypass before those are authorized. `@ORESoftware` alone is not an independent source-transfer reviewer.

## Go/no-go

**GO** only if all are true: source-local admission established, all required branch protections enforceable, exact-head required checks *really execute*, Windows/macOS and adapter integration requirements tested, independent source security reviews approved, and source-transfer incidents independently dispositioned (if export proposed). Otherwise **NO-GO**; leave PRs draft and unmerged.
