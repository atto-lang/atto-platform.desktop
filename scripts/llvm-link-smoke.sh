#!/usr/bin/env bash
# Link real Oreslang LLVM host scalar exports with a native C desktop host.
set -euo pipefail
umask 077
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "Usage: bash scripts/llvm-link-smoke.sh /path/to/oreslang-llvmc [expected-sha256]" >&2
  exit 64
fi
if [ ! -f "$1" ] || [ -L "$1" ] || [ ! -x "$1" ]; then
  echo "Compiler path must be an executable regular file and not a symlink" >&2
  exit 64
fi
root="$(cd "$(dirname "$0")/.." && pwd -P)"
compiler="$1"
expected_sha256="${2:-}"
command -v cmake >/dev/null || { echo "CMake required for compiler/artifact SHA-256 verification." >&2; exit 69; }
clang_bin="clang"
command -v "$clang_bin" >/dev/null || { echo "Clang required." >&2; exit 69; }
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT

actual_sha256="$(cmake -E sha256sum "$compiler")"
actual_sha256="${actual_sha256%% *}"
if [ -n "$expected_sha256" ]; then
  expected_sha256="$(printf '%s' "$expected_sha256" | tr '[:upper:]' '[:lower:]')"
  if [ "$actual_sha256" != "$expected_sha256" ]; then
    echo "Compiler SHA-256 changed before guest execution" >&2
    exit 65
  fi
fi

# Execute a private byte-for-byte copy so a mutable path or symlink cannot swap
# compiler contents after the admission hash. Re-hash the copy before use.
compiler_exec="$work/oreslang-llvmc"
cp "$compiler" "$compiler_exec"
chmod 500 "$compiler_exec"
pinned_sha256="$(cmake -E sha256sum "$compiler_exec")"
pinned_sha256="${pinned_sha256%% *}"
if [ "$pinned_sha256" != "$actual_sha256" ]; then
  echo "Compiler bytes changed while pinning private execution copy" >&2
  exit 65
fi
printf 'ORES_DESKTOP_COMPILER_SHA256=%s\n' "$pinned_sha256"

"$compiler_exec" --tree-shake \
  --tree-shake-root=desktop_initial_width \
  --tree-shake-root=desktop_initial_height \
  --tree-shake-root=desktop_next_frame \
  --target=host "$root/examples/guest_config.atto" > "$work/guest.ll"

require_i64_signature() {
  local symbol="$1"
  local args_regex="$2"
  local count
  count="$(grep -Ec "^define[[:space:]].*i64[[:space:]]+@\"?${symbol}\"?\\(${args_regex}\\)" "$work/guest.ll" || true)"
  if [ "$count" -ne 1 ]; then
    echo "desktop guest symbol has missing/duplicate/wrong i64 ABI: $symbol" >&2
    exit 1
  fi
}
require_i64_signature desktop_initial_width ""
require_i64_signature desktop_initial_height ""
require_i64_signature desktop_next_frame "i64[^,)]*"
if grep -Eq '@"?desktop_unreachable_probe"?\(' "$work/guest.ll"; then
  echo "tree shaking retained unreachable desktop guest symbol" >&2
  exit 1
fi
ir_sha256="$(cmake -E sha256sum "$work/guest.ll")"
ir_sha256="${ir_sha256%% *}"
printf 'ORES_DESKTOP_GUEST_IR_SHA256=%s\n' "$ir_sha256"

"$clang_bin" -x ir -c "$work/guest.ll" -o "$work/guest.o"
object_sha256="$(cmake -E sha256sum "$work/guest.o")"
object_sha256="${object_sha256%% *}"
printf 'ORES_DESKTOP_GUEST_OBJECT_SHA256=%s\n' "$object_sha256"

"$clang_bin" -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$root/include" "$root/src/ores_desktop.c" \
  "$root/examples/llvm_host_bridge.c" "$work/guest.o" -o "$work/linked"
linked_before="$(cmake -E sha256sum "$work/linked")"
linked_before="${linked_before%% *}"
printf 'ORES_DESKTOP_LINKED_SHA256=%s\n' "$linked_before"

set +e
"$work/linked"
run_status=$?
set -e
linked_after="$(cmake -E sha256sum "$work/linked")"
linked_after="${linked_after%% *}"
if [ "$linked_after" != "$linked_before" ]; then
  echo "Linked executable changed during guest execution" >&2
  exit 66
fi
printf 'ORES_DESKTOP_LINKED_POSTRUN_SHA256=%s\n' "$linked_after"
if [ "$run_status" -ne 0 ]; then
  echo "Linked desktop guest exited with status $run_status" >&2
  exit "$run_status"
fi
