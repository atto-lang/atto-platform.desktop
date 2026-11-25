# AttoLang desktop across LLVM, .NET NativeAOT and GraalVM

**State: host-interop bootstrap only.** This repository retains `ores_*` binary symbols while the public language name changes to AttoLang: silently renaming C symbols would break existing consumers. Neither this change nor its examples prove the AttoLang guest compiler can import the host ABI or render an application.

## One app contract, three native execution paths

- **LLVM AOT:** compile AttoLang business logic to native object code; link the C host adapter (existing `scripts/llvm-link-smoke.sh`). Until imported host calls/callback trampolines are implemented, C calls only ordinary exported scalar guest functions.
- **.NET NativeAOT:** compile AttoLang logic through the .NET backend. A thin C# generated P/Invoke layer is an **interop prototype**, not a substitute for guest-source ABI lowering. It can host app startup, but `interop/dotnet` currently proves only the synchronous headless C callback lifecycle.
- **Java/GraalVM Native Image:** compile the Java-backed AttoLang runtime with Native Image. The JDK 25 FFM bridge in `interop/graalvm` demonstrates downcalls and upcalls from a host-side Java program. Real guest imports/exports, native-image reachability registration and AOT binary execution must be separately admitted.

All three must resolve the **same** stable host interface; do not fork UI semantics by toolchain. The SDL3 window/input backend is opt-in and remains separate from the compiler. Select a renderer/UI adapter as a deployment choice: SDL3 for window/input/rendering primitives, or a future accessibility-capable native-widget adapter. SDL3 is not a full desktop widget toolkit.

## Build the library and run host-only FFI checks

```sh
cmake -S . -B build-interop -DATTO_DESKTOP_BUILD_SHARED=ON -DORES_DESKTOP_WITH_SDL3=OFF -DBUILD_TESTING=ON
cmake --build build-interop --config Release
ctest --test-dir build-interop -C Release --output-on-failure
```

The opt-in shared library is `libores_desktop.so` (Linux), `libores_desktop.dylib` (macOS) or `ores_desktop.dll` (Windows). This adds a dynamic linking option without changing the existing static target or host v2 ABI. On Windows, CMake exports the public C symbols. Build for each target OS/architecture: do not attempt to reuse binaries across platforms.

With .NET SDK 10 and the library in the native loader search path, run:

```sh
# Linux example; use DYLD_FALLBACK_LIBRARY_PATH or PATH as appropriate on other OSes.
LD_LIBRARY_PATH="$PWD/build-interop" dotnet run --project interop/dotnet/AttoDesktopInterop.csproj --configuration Release
```

With JDK 25, using an **absolute path** to the shared library:

```sh
javac -d build-interop/ffm interop/graalvm/DesktopInteropProbe.java
java --enable-native-access=ALL-UNNAMED -cp build-interop/ffm DesktopInteropProbe "$PWD/build-interop/libores_desktop.so"
```

A GraalVM 25 Native Image build requires its FFM upcall/downcall metadata admission (including build-time registration of function descriptors and upcall targets), platform-specific native-link tests and a **run of the actual image**, not just HotSpot Java execution. It must fail closed on missing metadata or unsupported targets. GraalVM 25 documents FFM support on Windows x64, Linux x64/arm64 and macOS arm64; do not assume macOS x64 Native Image upcalls work.

The probes validate host layout (64-bit only), ABI version mismatch rejection, callback/status semantics, correct stop/close counts and native interop. They do **not** open SDL windows, load AttoLang guest classes, prove actor isolation or prove NativeAOT/Native Image deployment. The canonical host ABI v2 uses a borrowed length-exact UTF-8 span (native size_t, 1–127 bytes); the host validates and snapshots the span before callbacks. ABI v1 callers are rejected. The merged scalar LLVM callback smoke remains the canonical .atto proof.

## Additive host-only SPSC and genuine native-image gates

The `include/ores_desktop_ui_mailbox.h` / `src/ores_desktop_ui_mailbox.c` pair is a bounded host-only **single-producer/single-consumer** command edge. It has no guest code, OS window registry or renderer. A separate runtime component must aggregate multiple actors, wake the UI thread, validate tenant/capability/window generations, and dispatch. C11 atomics are used on Linux/macOS and Windows uses native Interlocked operations because MSVC's C11 atomic headers are experimental. Push/Pop never allocate and return FULL/EMPTY promptly. Closing is terminal; do not reuse storage while threads might access it.

The `desktop-cross-runtime-ffi` workflow now additionally **attempts** actual Linux x64 published .NET NativeAOT and GraalVM Native Image host-probe binaries. These builds are deliberately separate from JVM/managed and compiler-backed AttoLang guest execution. Graal's `interop/graalvm/reachability-metadata.json` explicitly describes each registered C ABI upcall/downcall and the image must be built with `--no-fallback`. See [GraalVM 25 FFM Native Image registration requirements](https://www.graalvm.org/jdk25/reference-manual/native-image/native-code-interoperability/ffm-api/).

If the GitHub Actions job has `steps=null`, those AOT steps **did not run**. Do not report any NativeAOT/Native Image pass until an exact-head executed-step job and binary receipt exist.

## ABI exposure and mailbox audit (November 2026)

The opt-in shared library deliberately exports **only** `ores_desktop_abi_version` and `ores_desktop_run`. The previous `WINDOWS_EXPORT_ALL_SYMBOLS` arrangement would also have exported host-internal C11 mailbox functions, falsely promoting unstable native struct layout to a permanent public DLL contract. Linux/Unix builds now compile the shared target with hidden default visibility and annotate the two published symbols; Windows uses explicit `__declspec(dllexport)`. Linux CTest checks the dynamic symbol allowlist, and must fail if future internal symbols leak.

The SPSC UI mailbox enforces canonical message payloads: `REQUEST_REDRAW` and `REQUEST_CLOSE` require `value == 0`; only `SET_VISIBILITY` consumes `value`, and accepts only 0/1. Each mailbox is associated with one immutable actor generation. On terminal closure, valid commands return CLOSED instead of STALE/FULL when closure is observed; this is not a synchronization guarantee that a message racing with close was never executed **before** closure. Consumers must still check window generations and per-tenant capabilities at actual UI dispatch.

Counter wraparound, 50,000-event FIFO ordering and post-close rejection are covered by the Linux threaded test. The fixed-capacity queue remains **single producer + single consumer**, not an actor MPSC system; never free/reinitialize its storage until all users have been joined or quiesced. Avoid passing the host-internal atomic layout to .NET/GraalVM foreign code; stable interop is limited to the two ABI v1 functions.

The canonical extension for new AttoLang source is `.atto`; `fixtures/desktop_scalar_contract.atto` is a **source-only** scalar example, paired with a legacy `.ores` compatibility fixture. Neither has an admitted compiler-backed desktop GUI receipt.

## Stable source-level desktop API to build next

Proposed package split (illustrative names, **not** accepted guest syntax):

- `atto.desktop.core`: App, Window, WindowId, WindowEvent, EventStream, UiDispatcher, lifecycle, feature negotiation.
- `atto.desktop.render`: CPU/Skia/Vulkan/Metal/Direct3D adapters; no raw GPU pointers cross actor ownership domains.
- `atto.desktop.widgets`: widgets, layout, text shaping, focus, accessibility tree and IME composition.
- `atto.desktop.os`: menus, tray, drag/drop, clipboard, file picker, notifications, deep links, power/sleep, DPI and screen-reader support; each sensitive operation gated by explicit capabilities.
- `atto.desktop.webview`: optional, hardened embedded HTML; separate origin/protocol policy and host-guest bridge.
- `atto.desktop.test`: deterministic virtual clock, synthetic events, headless render snapshots, focus/accessibility model tests.

**Main thread:** `Window`/OS UI objects have UI-thread affinity (macOS requires a main-thread UI loop); only the UI dispatcher touches them. Atto actors never run/block inside native GUI callbacks. Copy bounded events to an explicit actor mailbox and marshal actor results back to the UI dispatcher. No foreign pointers or arena-backed Java segments survive the synchronous callback; no GC-managed .NET object is retained as an unpinned `void*`. Backpressure, cancellation, shutdown and actor generation/tenant fences are mandatory.

**Isolation/security:** Cross-tenant actor heaps remain backend-specific and separate from the desktop host. Desktop capability grants are checked at each invocation, not implied by linking `ores_desktop`. File dialogs return scoped access tokens; default permissions deny network/filesystem/clipboard/notifications unless explicitly granted. Invalid event types, malformed UTF-8, stale window handles, off-main-thread access and callback exceptions must fail closed at their respective boundaries.

## Parity gates before advertising desktop support

| Milestone | LLVM | .NET NativeAOT | GraalVM Native Image |
| --- | --- | --- | --- |
| Shared native host ABI | Existing C host + scalar native link smoke | P/Invoke host-only prototype in this PR | JDK 25 FFM host-only prototype in this PR |
| **Compiler-backed Atto guest** imports, callbacks and ownership | Pending | Pending | Pending |
| SDL3 window / input / text / DPI / IME / multi-window lifecycle | Pending | Pending | Pending |
| Actor/UI bridging, nonblocking scheduling and retirement fencing | Pending | Pending | Pending |
| Capability denial, sandboxing, a11y and native widgets | Pending | Pending | Pending |
| Signed Windows/macOS installers, Linux packages, SBOM, updates | Pending | Pending | Pending |

Minimum acceptance: compile the **same** native `.ores` desktop sample for each backend on Windows x64, Linux x64/arm64 and macOS arm64 where supported; verify output/event hashes and negative cases (wrong ABI, invalid UTF-8, foreign-window events, denial, late event after close, cancellation, callback trap, data races); run real SDL GUI acceptance under virtual display and on physical macOS/Windows; run .NET NativeAOT and GraalVM `native-image` output rather than just JIT host programs; record exact PR head/tree SHA, actual executed job steps, compiler/runtime version, target architecture and signatures. Existing GitHub Actions runner admission issue #10 means zero-step failures are **not** failed tests or verification.

Before merging this branch, reconcile the open desktop ABI-v2 PR #11, safety PRs #15/#17/#19 and CI PR #16. No source transfer to another organization without explicit private-destination authorization.
