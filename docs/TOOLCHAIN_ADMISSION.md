# Desktop five-toolchain admission gate

This is a **host ABI** project. There is not yet a five-toolchain AttoLang source-level desktop API. The table below tracks current `main` descriptions as historical admission constraints from 2026-11-01; these are *admission constraints*, not test passes or whole-backend capability certifications.

| Toolchain | Current desktop boundary | Required first real guest evidence |
| --- | --- | --- |
| Java / GraalVM | A Java guest runtime does not natively call this in-process C callback ABI; desktop guest bridge not established. | Versioned, capability-gated host bridge with real Java/Graal guest and cleanup/lifetime tests |
| LLVM | Native scalar guest exports can link with the host via `scripts/llvm-link-smoke.sh`; no general guest desktop FFI or actor callback. | Exact-head compiled `.atto` scalar native link, then versioned host-import/callback trampoline with borrowed-data rules |
| Wasm | Experimental pure `() -> i64` module; guest imports forbidden, no UI event host imports. | Deliberately versioned, bounded import/export interface and real Wasm worker execution; reject unsupported UI/actor calls |
| Cranelift | Independent scalar JIT with internal argument/output ABI; no general foreign host ABI or actors. | Execute shared scalar fixture with real Cranelift and reject attempted desktop FFI with no executable output |
| .NET Native AOT | Bootstrap scalar subset with Native AOT target, not general AttoLang guest desktop interop. | Real scalar Native AOT execution, then an explicit safe native P/Invoke/reverse-callback contract |
 
**Common minimum contract before advertising desktop parity:** all backends must agree on guest-side title bytes (well-formed UTF-8, 1–127 bytes in host ABI v2), scalar dimensions (1–16384), event budget (1–256), callback outcome codes, borrowed lifetimes and cleanup ordering. The host guard in `src/ores_desktop.c` alone does not mean a guest can supply a string or receive a UI event.

## Evidence checklist

1. Pin each compiler executable to a reviewed source SHA/tree and record toolchain versions. Check source parsing, type checking, accepted/denied host imports and actual guest execution; no stubs or host-only substitutes count as guest evidence.
2. Use a **shared portable scalar fixture** for window width/height limits, exercised by **all five** real toolchains before attempting UI FFI. Cranelift and Wasm are intentionally much narrower; use only their genuinely shared scalar subset, and fail closed when no overlap exists.
3. Give each backend an explicit adapter for marshaling owned immutable UTF-8 bytes into the host's temporary copied title, or an equivalent **platform-native** window adapter. Do not pass raw C pointers across Wasm workers or Java managed heaps.
4. Validate source-level negative cases (malformed UTF-8 where source admits raw bytes; unsupported desktop FFI; invalid dimensions; callback misuse), with no compiled artifact for compiler rejections, and callback lifecycle/bounds in a backend-neutral model.
5. Check Linux GCC/Clang, sanitizers, macOS and Windows real C host CI with nonzero steps. Check real SDL3 window ownership and teardown separately. **Zero-step, queued, skipped or failed admission jobs provide no compiler/runtime evidence.**
6. Preserve the current length-exact ABI v2 contract; re-run native and downstream tests when changing title admission. Do not assume v1 and v2 are ABI-compatible.
7. Only after all required toolchain and OS matrix jobs execute successfully should the integration metadata move from `not-attested`; until then leave advertised desktop parity as **NOT_VERIFIED**.

The standalone UTF-8 CTest migrated in this PR is **host-only**. It can be run identically under native C compilers on supported operating systems, but is not a claim that all five AttoLang toolchains compile the same desktop guest.
