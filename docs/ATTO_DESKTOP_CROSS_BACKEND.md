# AttoLang desktop across LLVM, .NET NativeAOT and GraalVM

**State: host-interop bootstrap only.** This repository retains `ores_*` binary symbols while the public language name changes to AttoLang: silently renaming C symbols would break existing consumers. Neither this change nor its examples prove the AttoLang guest compiler can import the host ABI or render an application.

## One app contract, three native execution paths

- **LLVM AOT:** compile AttoLang business logic to native object code; link the C host adapter (existing `scripts/llvm-link-smoke.sh`). Until imported host calls/callback trampolines are implemented, C calls only ordinary exported scalar guest functions.
- **.NET NativeAOT:** compile AttoLang logic through the .NET backend. A thin C# generated P/Invoke layer is an **interop prototype**, not a substitute for guest-source ABI lowering. It can host app startup, but `interop/dotnet` currently proves only the synchronous headless C callback lifecycle.
- **Java/GraalVM Native Image:** compile the Java-backed AttoLang runtime with Native Image. The JDK 25 FFM bridge in `interop/graalvm` demonstrates downcalls and upcalls from a host-side Java program. Real guest imports/exports, native-image reachability registration and AOT binary execution must be separately admitted.

All three must resolve the **same** stable host interface; do not fork UI semantics by toolchain. The SDL3 window/input backend is opt-in and remains separate from the compiler. Select a renderer/UI adapter as a deployment choice: SDL3 for window/input/rendering primitives, or a future accessibility-capable native-widget / WebView adapter. SDL3 is not a full desktop widget toolkit.

## Build the library and run host-only FFI checks

```sh
cmake -S . -B build-interop -DATTO_DESKTOP_BUILD_SHARED=ON -DORES_DESKTOP_WITH_SDL3=OFF -DBUILD_TESTING=ON
cmake --build build-interop --config Release
ctest --test-dir build-interop -C Release --output-on-failure
```

The opt-in shared library is `libores_desktop.so` (Linux), `libores_desktop.dylib` (macOS) or `ores_desktop.dll` (Windows). This adds a dynamic linking option without changing the existing static target or host v1 ABI. On Windows, CMake exports the public C symbols. Build for each target OS/architecture: do not attempt to reuse binaries across platforms.

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

The probes validate host layout (64-bit only), ABI version mismatch rejection, callback/status semantics, correct stop/close counts and native interop. They do **not** open SDL windows, load AttoLang guest classes, prove actor isolation or prove NativeAOT/Native Image deployment. Current host v1 uses a borrowed NUL-terminated title; title ABI v2 and UTF-8 hardening are separate draft PRs and must be rebased semantically before merging.

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
