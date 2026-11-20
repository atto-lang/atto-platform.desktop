# Desktop delivery roadmap

1. **Bootstrap (this change):** small C11 host ABI, optional SDL3 native window lifecycle, structured input events, deterministic CTest and scalar LLVM native-link smoke. Do not claim guest-side UI API parity.
2. Add real renderer: explicit CPU/surface rendering path, viewport scaling and refresh/event scheduling; test in virtual-display CI.
3. Specify a versioned LLVM-to-C desktop guest import ABI and compiler admission diagnostics. Avoid generating symbols for unsupported Oreslang classes/closures or actor handles.
4. Add actor mailbox-to-UI delivery through a bounded, copied message queue while honoring single-native-thread-per-actor and UI-main-thread constraints.
5. Introduce capability-gated clipboard, dialogs, filesystem, menus, accessibility, timers and notifications. Test denial and cancellation.
6. Provide source-level desktop module APIs and examples compiled by oreslang-source.llvm (no interpreter/JVM dependency), with ownership/exception conformance.
7. Add packaging (MSIX/Windows signing, macOS app bundle/notarization, Linux AppImage/Flatpak), native architecture verification, artifact SBOM and release provenance.
8. Run physical macOS/Windows/Linux GUI acceptance tests and sanitizer fuzzing of host boundary before advertising general availability.

Each milestone should pin an exact LLVM backend commit and report observed vs unsupported behavior. Build evidence on a hosted runner counts only when the actual compiler and test steps execute.
