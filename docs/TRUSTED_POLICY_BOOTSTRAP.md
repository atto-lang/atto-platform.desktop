# Desktop default-branch PR metadata policy — candidate, not release approval

## Trust separation

- `.github/workflows/desktop-trusted-pr-metadata-policy.yml` is designed for
  `pull_request_target` and `pull_request_review`. GitHub takes its workflow
  **from the default branch**, rather than PR code. Until protected `main`
  is established and this PR is independently reviewed, that root is not trusted.
- Workflow permissions are `contents: read` and `pull-requests: read`. It uses
  **no** `actions/checkout`, PR checkout, uploaded artifact, privileged secret,
  cross-organization carrier, source build, token write or status publication.
- The workflow fetches only **same-repository policy blob**
  `5ccd5a27ad8b595728dfb39e85c6a8e167f2b723`, then verifies SHA-256
  `a8511f7bd50a72101197ab20994fcc10c55138b68e64e62b74f151de1d83642b`
  before executing it from runner temporary storage.
- The policy compares authoritative PR ID/head/base with event metadata, checks
  branch protection, strict required checks, admin enforcement, and current-head
  independent reviews (including the effective branch-protection approval quorum). It fully pages changed filenames and reviews. Changes
  under `.github/`, `scripts/`, `tools/`, `cmake/`, build entrypoints,
  or Git-admission metadata (including renames) require two distinct
  current-head approvals including `ORESoftware`, with reviewer
  write access checked via GitHub's metadata API. Its required checks must
  include all five desktop jobs with the verified GitHub Actions App ID `15368`; force pushes,
  branch deletion, spoofed/nonportable filenames, stale review retention, or disabled CODEOWNER enforcement
  cause an explicit denial. App-ID pinning alone does not prevent a PR
  from changing its own CI workflow; the protected workflow provenance and
  independent review gates are still necessary.

## Fail-closed limitations

1. The pinned blob currently comes from **draft PR #14**, not an independently
   approved, protected policy release. A digest guarantees bytes, not trust.
2. The policy calls `GET /branches/main/protection`, which GitHub documents as
   requiring **Administration: read**. The narrowly scoped automatic
   `GITHUB_TOKEN` used by this workflow does not have that privilege. It will
   **fail closed** on that API until a security-reviewed read-only attestation
   mechanism is available; do not add a secret or broad token ad hoc.
3. `pull_request_target` job checks are not a substitute for independently
   verified, **exact-PR-head** checks and protected-branch required checks.
   This workflow does not write check runs or report release approval.
4. On the current private GitHub plan, rulesets return 403 and `main` is not
   protected. Actions jobs still fail before runner allocation. Therefore this
   candidate cannot run or be an enforceable release security gate now.
5. For the bootstrap, do **not** disable protection or merge unreviewed code.
   An authorized security/billing administrator must first provide eligible
   private-branch protection and independent review, resolve runner admission,
   then approve/publish a fixed trusted policy root and configure exact-head
   required checks with source authentication. A policy update can never
   self-approve merely because its own tests pass.
6. Existing private-source exposure incidents conformance #22/#77 and
   cross-organization publisher authorization source.java #962 remain blocking.
   Do not copy private Git trees to another org without their disposition.

## Local source-free acceptance

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s scripts -p 'test_trusted_policy*.py' -v
```

The policy/CLI/workflow tests cover clean metadata, hostile PR workflow/verifier modifications,
renamed sensitive files, untrusted approvals, missing/stale approvals,
branch-protection failures, incomplete pagination, token-scope-safe workflow
structure, and a fake GitHub CLI process restricted to GETs. They do **not**
prove that the policy ran on hosted GitHub Actions or that native desktop tests
passed.

GitHub references:
- https://docs.github.com/en/actions/reference/security/securely-using-pull_request_target
- https://docs.github.com/en/rest/branches/branch-protection#get-branch-protection
- https://github.com/ores-truffle-oreslang/oreslang-platform-conformance/issues/170
