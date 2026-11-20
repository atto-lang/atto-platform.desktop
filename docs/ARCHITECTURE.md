# Desktop host architecture v0

## Separation of responsibilities

The LLVM compiler should produce native machine code for a verified Oreslang program. The desktop repository owns the platform adapter and its capabilities, not a second Oreslang compiler. Host C functions can be linked with ordinary scalar LLVM exports today (see LLVM linkage smoke), but guest-source host imports/callback trampolines are not yet admitted by the compiler.

## C ABI v2 (bootstrap, not long-term stable)

include/ores_desktop.h provides a process-local C11 API. All ABI aggregate layouts are native-host layouts, *not* a versioned wire representation. Callbacks are synchronous and only borrow event/config data for the duration of invocation. Events contain no pointers, dynamically allocated strings or OS-owned handles. Key codes are SDL physical scancodes when using SDL3; these are not text or portable business identifiers.

A config must specify version 2, nonzero dimensions up to 16384, an explicit `title` + `title_length` pair of 1..127 bytes with no embedded NUL, and max_events_per_frame in 1..256. The source title does not need a terminator; the host snapshots exactly `title_length` bytes and appends its own NUL before calling a backend. The caller owns all pointed-to config/app/backend memory through ores_desktop_run. The pointer contract assumes caller-supplied valid C memory; it is **not** a hostile foreign pointer validation layer.

The host runs on a single UI thread, validates event types, bounds event processing per frame and yields to on_frame before the next polling round. The backend's present method must pace an unlimited app; SDL3 waits 16 ms. This is a simple cadence, not high-resolution rendering or a fairness guarantee under blocking callbacks.

Open failure is owned by the backend and must clean partial resources. After successful open, the host always closes exactly once; on_stop is called if and only if startup succeeds (including explicit STOP). Invalid callback statuses become APP_ERROR. Backend errors or a malformed event become BACKEND_ERROR. STOP/QUIT/frame limit return OK. The main loop does not swallow native exceptions, nor does it promise resilience against untrusted C callbacks.

## Native backend

SDL3 is explicitly opt-in. It maps quit, resize, physical key, pointer movement and button events to a tiny C record. It bounds discarded unrelated SDL events per poll and owns its SDL video subsystem initialization and shutdown. The example does not render or call SDL graphics APIs; its present operation only paces the loop. SDL3 code is not compiled or tested by default headless CTest.

## Capability/security boundaries

Desktop apps have powerful OS privileges. Future Oreslang file/network/clipboard/notification/interop access must be authorized separately by host policy and compiler effect checking; selecting target=desktop or linking the host does not grant privileges. Do not expose raw native handles or SDL pointers to untrusted actors. Actor scheduling and ownership domains are owned by oreslang-source.llvm and its runtime; v0 makes no actor-isolation claim. Signing, notarization and installer hardening are out of scope.

## Target matrix

| Target | Host event ABI | Window adapter | Oreslang guest direct UI |
| --- | --- | --- | --- |
| Linux x64/arm64 | C11 portable source | SDL3 when installed | Not implemented |
| macOS arm64/x64 | C11 portable source | SDL3 when installed | Not implemented |
| Windows x64/arm64 | C11 portable source | SDL3 when installed | Not implemented |

Target rows are *design intent*, not an assertion that full cross-architecture CI, GUI tests or signed packages have passed.
