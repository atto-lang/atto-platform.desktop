#!/usr/bin/env bash
# Host-only regression for the real LLVM guest link gate.
# Fake compilers below emit controlled LLVM IR; this does NOT prove Oreslang compilation.
set -euo pipefail
umask 077
root="$(cd "$(dirname "$0")/.." && pwd -P)"
smoke="$root/scripts/llvm-link-smoke.sh"
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT

write_good() {
  cat > "$1" <<'IR'
#!/usr/bin/env bash
cat <<'LLVM'
define i64 @desktop_next_frame(i64 %frame) {
entry:
  %next = add i64 %frame, 1
  ret i64 %next
}
define i64 @desktop_initial_width() {
entry:
  ret i64 800
}
define i64 @desktop_initial_height() {
entry:
  %h = call i64 @desktop_next_frame(i64 499)
  ret i64 %h
}
LLVM
IR
  chmod 700 "$1"
}

hash_file() {
  local value
  value="$(cmake -E sha256sum "$1")"
  printf '%s' "${value%% *}"
}

good="$work/good-compiler"
write_good "$good"
good_sha="$(hash_file "$good")"
upper_sha="$(printf '%s' "$good_sha" | tr '[:lower:]' '[:upper:]')"
good_out="$(bash "$smoke" "$good" "$upper_sha")"
grep -Eq '^ORES_DESKTOP_COMPILER_SHA256=[0-9a-f]{64}$' <<<"$good_out"
grep -Eq '^ORES_DESKTOP_GUEST_IR_SHA256=[0-9a-f]{64}$' <<<"$good_out"
grep -Eq '^ORES_DESKTOP_GUEST_OBJECT_SHA256=[0-9a-f]{64}$' <<<"$good_out"
before="$(sed -n 's/^ORES_DESKTOP_LINKED_SHA256=//p' <<<"$good_out")"
after="$(sed -n 's/^ORES_DESKTOP_LINKED_POSTRUN_SHA256=//p' <<<"$good_out")"
test -n "$before" && test "$before" = "$after"
grep -Fq 'LLVM guest i64 frame transitions -> native desktop callback: PASS' <<<"$good_out"

set +e
wrong_out="$(bash "$smoke" "$good" 0000000000000000000000000000000000000000000000000000000000000000 2>&1)"
wrong_status=$?
set -e
test "$wrong_status" -eq 65
grep -Fq 'Compiler SHA-256 changed before guest execution' <<<"$wrong_out"

symlinked="$work/symlinked-compiler"
ln -s "$good" "$symlinked"
set +e
symlink_out="$(bash "$smoke" "$symlinked" "$good_sha" 2>&1)"
symlink_status=$?
set -e
test "$symlink_status" -eq 64
grep -Fq 'Compiler path must be an executable regular file and not a symlink' <<<"$symlink_out"

missing="$work/missing-compiler"
cat > "$missing" <<'IR'
#!/usr/bin/env bash
cat <<'LLVM'
define i64 @desktop_initial_width() { ret i64 800 }
define i64 @desktop_initial_height() { ret i64 500 }
LLVM
IR
chmod 700 "$missing"
missing_sha="$(hash_file "$missing")"
set +e
missing_out="$(bash "$smoke" "$missing" "$missing_sha" 2>&1)"
missing_status=$?
set -e
test "$missing_status" -eq 1
grep -Fq 'desktop guest symbol has missing/duplicate/wrong i64 ABI: desktop_next_frame' <<<"$missing_out"

wrong_abi="$work/wrong-abi-compiler"
cat > "$wrong_abi" <<'IR'
#!/usr/bin/env bash
cat <<'LLVM'
define i32 @desktop_next_frame(i32 %frame) {
entry:
  %next = add i32 %frame, 1
  ret i32 %next
}
define i64 @desktop_initial_width() { ret i64 800 }
define i64 @desktop_initial_height() { ret i64 500 }
LLVM
IR
chmod 700 "$wrong_abi"
wrong_abi_sha="$(hash_file "$wrong_abi")"
set +e
wrong_abi_out="$(bash "$smoke" "$wrong_abi" "$wrong_abi_sha" 2>&1)"
wrong_abi_status=$?
set -e
test "$wrong_abi_status" -eq 1
grep -Fq 'desktop guest symbol has missing/duplicate/wrong i64 ABI: desktop_next_frame' <<<"$wrong_abi_out"

unreachable="$work/unreachable-compiler"
cat > "$unreachable" <<'IR'
#!/usr/bin/env bash
cat <<'LLVM'
define i64 @desktop_next_frame(i64 %frame) {
entry:
  %next = add i64 %frame, 1
  ret i64 %next
}
define i64 @desktop_initial_width() { ret i64 800 }
define i64 @desktop_initial_height() { ret i64 500 }
define i64 @"desktop_unreachable_probe"() { ret i64 999 }
LLVM
IR
chmod 700 "$unreachable"
unreachable_sha="$(hash_file "$unreachable")"
set +e
unreachable_out="$(bash "$smoke" "$unreachable" "$unreachable_sha" 2>&1)"
unreachable_status=$?
set -e
test "$unreachable_status" -eq 1
grep -Fq 'tree shaking retained unreachable desktop guest symbol' <<<"$unreachable_out"

echo "desktop LLVM link-smoke guard controls: PASS (host-only fake IR)"
