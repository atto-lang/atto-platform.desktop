> **AI agent instructions (organization-wide):** Before working on code, issues, or pull requests, read and follow the [organization `AGENTS.md`](https://github.com/atto-lang/.github/blob/main/AGENTS.md), including its **merge-first stealth-development policy**. Honor any repository-local `AGENTS.md`, explicit holds, and GitHub-enforced rules.

# atto-platform.desktop

Desktop runtime, native windowing, and platform integration for AttoLang applications.

## Naming migration and language authority

This repository is maintained at [atto-lang/atto-platform.desktop](https://github.com/atto-lang/atto-platform.desktop); use `https://github.com/atto-lang/atto-platform.desktop.git` for Git remotes and canonical `.atto` source examples. The normative language contract is [atto-spec](https://github.com/atto-lang/atto-spec), with executable guest evidence tracked by [atto-platform-conformance](https://github.com/atto-lang/atto-platform-conformance). Follow current ownership, borrowing, effects, actor, and syntax rules there; do not infer implemented features from a README or unexecuted CI.

## Scope

Desktop target support, windowing, platform APIs, packaging, and native integration.

## Portability contract

The AttoLang language contract must remain separate from target-specific capabilities. Document supported toolchains, devices, platform APIs, memory limits, runtime constraints, and unsupported features explicitly.

## Verification

Where feasible, use real devices or representative emulators in addition to host-side tests. Record toolchain versions, target profiles, exact commit SHAs, and execution evidence. A successful build alone is not proof of runtime semantics.

## Development status

This is a statement of intended project responsibilities. Consult source files and CI for verified implementation or release status; do not assume all targets are supported.

## Related projects

- [AttoLang](https://github.com/atto-lang)
- [Target profiles](https://github.com/atto-lang/atto-target-profiles)
- [Platform conformance](https://github.com/atto-lang/atto-platform-conformance)

## Repository ownership and contribution routing

**Belongs here:** Desktop app/window lifecycle, OS-specific event loops, native packaging/installers and desktop target compatibility.

**Belongs elsewhere:** Portable UI widgets and rendering contracts belong in atto-runtime-ui; code generation in atto-source.*; shared target constraints in atto-target-profiles.

**Security and portability boundary:** Restrict filesystem/network/process capabilities and UI-thread access; test real Windows/macOS/Linux targets and resource cleanup before claiming portability.

AttoLang is the canonical project name; new source, examples and documentation use `.atto` and the `atto-lang` repository organization. This README defines scope, not feature readiness.

**Acceptance:** name exact source SHA/tree, runtime/toolchain, positive and negative checks and execution receipt before claiming support. Host-only tests and empty CI steps do not prove guest execution.

## Legacy migration provenance and gates

The original `ores-truffle-oreslang/oreslang-platform.desktop` repository is archived; the current AttoLang repository has a separate numeric GitHub ID. Historical `.ores` files and CLI/package names are compatibility history, not evidence of current `.atto` execution. Desktop host adapters, native app bootstrap and UI-thread ownership contracts belong here; managed cloud deployment and billing do not. Host-only checks do not establish guest parity, AOT, zero-GC or safe production use.

[atto-lang](https://github.com/atto-lang) owns language, portable libraries, runtime/compiler contracts and demos; [atto-co](https://github.com/atto-co) owns managed production services, tenancy, billing and deployment. Any future original-ID transfer or commercial carve-out requires separate owner approval and security review. Inventory exact SHA/tree, numeric ID, protections, permissions, PRs, releases, packages, dependencies, runners, secrets, applications, webhooks, Pages and rollback owner; verify preservation and retest references after an actual transfer. Never mirror private code without authorization. Zero-step jobs are NOT_EXECUTED; host-only checks do not establish compiled guest execution or production readiness.
