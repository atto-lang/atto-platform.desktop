#!/usr/bin/env bash
# Link real Oreslang LLVM host scalar exports with a native C desktop host.
set -euo pipefail
if [ "$#" -ne 1 ] || [ ! -x "$1" ]; then
  echo "Usage: bash scripts/llvm-link-smoke.sh /path/to/oreslang-llvmc" >&2
  exit 64
fi
root="$(cd "$(dirname "$0")/.." && pwd -P)"
compiler="$1"
clang_bin="clang"
command -v "$clang_bin" >/dev/null || { echo "Clang required." >&2; exit 69; }
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
"$compiler" --target=host "$root/examples/guest_config.ores" > "$work/guest.ll"
"$clang_bin" -x ir -c "$work/guest.ll" -o "$work/guest.o"
"$clang_bin" -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$root/include" "$root/src/ores_desktop.c" \
  "$root/examples/llvm_host_bridge.c" "$work/guest.o" -o "$work/linked"
"$work/linked"
