# AttoLang desktop guest contract v0 (proposed; no compiler support claimed)

The public programming language is **AttoLang**; existing repositories and the `ores_*` C ABI symbols stay unchanged until a deliberate, tested migration. The frontend and standard library, not the host adapters, own the source-level semantics.

## Proposed guest-visible modules

1. `atto.desktop.app`: `DesktopApp`, `AppLifecycle`, `UiDispatcher`, `WindowRef`, `WindowGeneration`. It owns startup, graceful close, and UI-thread entry.
2. `atto.desktop.window`: create, show/hide, request redraw, close, dimensions, minimum size, DPI, screen; all operations return an explicit result and check the window generation.
3. `atto.desktop.events`: immutable, copied `WindowEvent` records, `EventStream`, text/IME composition, key physical scancode vs character, pointer coordinates in logical units, close-request and cancellation semantics.
4. `atto.desktop.render`: capability-negotiated CPU or GPU surface; rendering is optional, SDL3 alone is not a widget or accessibility toolkit.
5. `atto.desktop.os`: capability-checked open/save dialogs, clipboard, notifications, menus, tray, drag-drop, deep links.
6. `atto.desktop.accessibility`: semantic tree, focus management, labels and roles, keyboard traversal and screen-reader signals.
7. `atto.desktop.test`: virtual display and deterministic event source, without requiring a graphics device.

Do not invent a new `async` keyword. AttoLang's accepted async, ownership and actor syntax must be used when actual guest declarations are authored. All default I/O is asynchronous; UI-bound operations must post messages to a UI dispatcher rather than blocking an actor carrier or running guest closures in native event callbacks. Do not reinterpret the C11 host struct layout as the guest object representation.

## Identity and isolation

A window reference is a tuple `(tenant, app, window_id, window_generation)` with an unforgeable host-side capability. Actor-to-UI messages additionally carry an actor incarnation/generation. The caller may not supply a raw SDL pointer, C FFI pointer, .NET object address or Java MemorySegment as a transferable value.

1. The main/UI thread owns the single SDL3 process-global event pump and the live window registry. Native callbacks may only snapshot bounded data into owned records.
2. Every command is validated **at dispatch time** against the current tenant, capability, active window generation, expected actor generation, command permissions, and cancellation deadline.
3. Closing or retiring a window invalidates its generation and denies all late commands. Reopening allocates a new generation, not just a reused numeric handle.
4. Bounded producer edges apply backpressure; FULL is a returned outcome, not a spin-loop on an actor carrier. Cancellation is idempotent, wakeups are nonblocking, and fairness is measured under load.
5. Neither SDL nor the host calls back into guest code while holding native locks or while a UI-only lifecycle callback is running. Fail-closed on host exceptions/traps.
6. Multi-producer actor traffic is aggregated by the runtime/scheduler or by one SPSC edge per actor. **The current C SPSC queue is not a general MPSC/MPMC actor mailbox.**

## What this PR implements now

- A **C host-side SPSC UI command mailbox**: fixed capacity 64, zero allocation per message, one producer and one UI consumer, immutable actor generation, bounded message copying, terminal close, and explicit FULL/EMPTY/CLOSED/STALE results. The mailbox is a C host-internal struct and is not a stable .NET/FFM public memory layout. A UI dispatcher must still perform tenant/capability/window-generation checks.
- The static and shared desktop host targets compile it. Linux adds a threaded 50k-FIFO CTest and all host build platforms add deterministic queue boundary tests.
- A portable scalar `.ores` admission fixture (`fixtures/desktop_scalar_contract.ores`) exercises only a minimal language subset; it is **not** a guest-side UI API or native desktop application.
- .NET NativeAOT and GraalVM Native Image host-interop build-and-execution CI gates are authored separately from the JVM/managed probes. They are not verified until the exact PR head executes real steps.

## Compiler implementation contracts / smallest lowering

**LLVM:** typed `.ores` external-import declaration -> type/effect/borrow checker -> MIR foreign call operation -> AOT lowering with named C symbol and exact v1/v2 ABI layout -> native object link -> dispatcher command result. Callback ingress must be a runtime-owned trampoline that only copies messages; do not pass a guest closure through C event callbacks.

**.NET NativeAOT:** verified guest import -> pre-generated/static CIL P/Invoke stub -> NativeAOT publish -> bounded host call. Use explicit native structs and caller-owned lifetimes. Never emit Reflection.Emit, runtime assembly loading, unrestricted marshaling or a JIT fallback. Generated `main()` remains a scalar subset today.

**Java/GraalVM:** verified guest import -> statically enumerated FFM downcall descriptor -> runtime-owned UI message dispatcher -> GraalVM Native Image. FFM upcall/downcall signatures and reachability metadata must be closed over at image-build time. No dynamic guest host-access escape and no reliance on a JIT compiling the missing FFI.

## Required executable parity receipts

- Three backend compilers independently accept the **same** full desktop `.ores` fixture. Record each source blob/tree SHA, compiler tree SHA, exact toolchain version and produced binary's hash.
- An actual window opens, processes two copied input events, performs a redraw, and closes exactly once; no GTK/Qt/Avalonia/SDL backend-specific semantics leak into the guest result. Test real multiwindow filtering separately.
- Negative tests: unsupported ABI, malformed UTF-8, forged/stale window capability, foreign window events, denied clipboard/file access, UI call from wrong thread, mailbox full, late message after close, actor shutdown, guest callback trap, lost wakeup.
- Native executables: LLVM linked binary; `dotnet publish -p:PublishAot=true` with executed output; `native-image --no-fallback` with executed output. A JDK HotSpot or .NET managed JIT smoke is never promoted into an AOT receipt.
- At least one x64 Linux + one macOS arm64 + Windows x64 stepful test, then expand to arm64 targets supported by each toolchain. GUI tests need virtual display and physical macOS/Windows runs; headless CTest isn't a GUI receipt.
- No release claim until actual nonzero-step exact-head runs, source/tree identity, code review, signing/notarization and runtime/actor conformance tests exist.

## Source organization / dependencies

The shared desktop host remains in `oreslang-platform.desktop`, with the real `.ores` API in a shared standard-library package, not reimplemented independently in Java/C#/C. The compiler work belongs in `oreslang-source.llvm`, `oreslang-source.net` and `oreslang-source.java`. Platform backend adapters may contain minimal native C/.NET/Java glue only. Reconcile desktop PR #11 (length-explicit title ABI), #15/#17 (SDL mock and foreign-window gate), #19 (UTF-8), and #16 (CI) before merging.
