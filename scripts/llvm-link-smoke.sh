#!/usr/bin/env bash
# Link real Oreslang LLVM host scalar exports with a native C desktop host.
set -euo pipefail
umask 077
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ] || [ ! -x "$1" ]; then
  echo "Usage: bash scripts/llvm-link-smoke.sh /path/to/oreslang-llvmc [expected-sha256]" >&2
  exit 64
fi
root="$(cd "$(dirname "$0")/.." && pwd -P)"
compiler="$1"
expected_sha256="${2:-}"
if [ -n "$expected_sha256" ]; then
  command -v cmake >/dev/null || { echo "CMake required for compiler SHA-256 verification." >&2; exit 69; }
  actual_sha256="$(cmake -E sha256sum "$compiler")"
  actual_sha256="${actual_sha256%% *}"
  if [ "$actual_sha256" != "$expected_sha256" ]; then
    echo "Compiler SHA-256 changed before guest execution" >&2
    exit 65
  fi
fi
clang_bin="clang"
command -v "$clang_bin" >/dev/null || { echo "Clang required." >&2; exit 69; }
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
"$compiler" --tree-shake \
  --tree-shake-root=desktop_initial_width \
  --tree-shake-root=desktop_initial_height \
  --tree-shake-root=desktop_next_frame \
  --target=host "$root/examples/guest_config.ores" > "$work/guest.ll"
if grep -Eq '@"?desktop_unreachable_probe"?\(' "$work/guest.ll"; then
  echo "tree shaking retained unreachable desktop guest symbol" >&2
  exit 1
fi
"$clang_bin" -x ir -c "$work/guest.ll" -o "$work/guest.o"
"$clang_bin" -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$root/include" "$root/src/ores_desktop.c" \
  "$root/examples/llvm_host_bridge.c" "$work/guest.o" -o "$work/linked"
"$work/linked"
