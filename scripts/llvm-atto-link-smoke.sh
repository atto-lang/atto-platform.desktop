#!/usr/bin/env bash
# Compiles real AttoLang .atto scalar exports into a native headless host.
# This is not a window renderer, general host import, or guest UI proof.
set -euo pipefail
if [ "$#" -ne 1 ] || [ ! -x "$1" ]; then
  echo "Usage: llvm-atto-link-smoke.sh /absolute/path/to/oreslang-llvmc" >&2
  exit 64
fi
if [[ "$1" != /* ]]; then
  echo "Compiler path must be absolute" >&2
  exit 64
fi
root="$(cd "$(dirname "$0")/.." && pwd -P)"
command -v clang >/dev/null || { echo "Clang is required." >&2; exit 69; }
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
"$1" --tree-shake \
  --tree-shake-root=desktop_initial_width \
  --tree-shake-root=desktop_initial_height \
  --target=host "$root/examples/guest_config.atto" > "$work/guest.ll"
if grep -q '@desktop_unreachable_probe(' "$work/guest.ll"; then
  echo "tree shaking retained unreachable native guest symbol" >&2
  exit 1
fi
clang -x ir -c "$work/guest.ll" -o "$work/guest.o"
clang -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$root/include" "$root/src/ores_desktop.c" \
  "$root/examples/llvm_host_bridge.c" "$work/guest.o" -o "$work/linked"
"$work/linked"
