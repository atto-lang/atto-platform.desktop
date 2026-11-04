# Oreslang Desktop

Native desktop platform bootstrap for [Oreslang LLVM](https://github.com/ores-truffle-oreslang/oreslang-source.llvm).

**Status: experimental host ABI and SDL3 window/event adapter.** This is not yet a desktop GUI framework, an Oreslang actor runtime, or a compiler extension. It intentionally builds and tests without LLVM, SDL3, a display server, or privileged OS APIs.

## Native host CI

Requires CMake 3.20+ and a C11 compiler.

~~~sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
~~~

This proves the small in-process C host ABI, deterministic application lifecycle, input event contract and error cleanup on a fake backend. The headless test does not display a window.

## Open a real desktop window (SDL3)

Install the SDL3 development package so CMake can resolve SDL3::SDL3. Then:

~~~sh
cmake -S . -B build-sdl -DORES_DESKTOP_WITH_SDL3=ON
cmake --build build-sdl
./build-sdl/ores_desktop_demo
~~~

On Windows, run the built executable from the chosen generator's configuration directory. On macOS, invoke it on the UI/main thread. The demo opens a resizable blank native window, reports resize events, and exits cleanly on close. It intentionally has **no renderer** yet: it does not draw widgets or execute Oreslang code.

## LLVM-native code linkage

The LLVM compiler's current admitted host subset can emit ordinary exported i64 scalar functions without a guest main. Use that to link native Oreslang logic into a C-host desktop application. This does **not** mean arbitrary C imports, UI callbacks, or actor messages compile in Oreslang today.

After building oreslang-source.llvm, run:

~~~sh
./scripts/llvm-link-smoke.sh /path/to/oreslang-source.llvm/build/oreslang-llvmc
~~~

The smoke uses examples/guest_config.ores, compiles it to LLVM IR, links it with examples/llvm_host_bridge.c and the desktop C host, and executes an OS-native headless binary. It requires Clang at build time, not at runtime. It intentionally fails when the compiler or target ABI cannot link; no interpreter fallback.

## Design

- Oreslang LLVM: source verification, native IR, AOT objects. It owns guest calling conventions and future host imports.
- ores_desktop: versioned single-threaded host event-loop C ABI, bounded polling, explicit lifecycle, and length-explicit title bytes (no caller-side NUL scan).
- ores_desktop_sdl3: optional native window/input backend for Linux, macOS and Windows, without embedding SDL pointers in the stable event type.
- Apps: native C callbacks for now. No borrowed event pointer can outlive a callback, and no C++ exception should cross this boundary.

See docs/ARCHITECTURE.md and docs/ROADMAP.md for admitted scope and future integrations. Compare oreslang-platform.mobile's portable host-ABI bootstrap.

## Explicitly not implemented

Oreslang guest-side desktop FFI, actor/event-loop integration, drawing, accessibility, menus, clipboard, notifications, filesystem grants, packaging, code signing, sandbox entitlements, rendering/GPU, mobile reuse, multiple windows and automated end-to-end GUI tests. A passing host CTest is not proof of any of these.
