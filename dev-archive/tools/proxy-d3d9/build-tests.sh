#!/bin/bash
# Builds and runs every selftest in this tool. Run from Git Bash.
#
# Why this exists (added 2026-09-14): the four selftests were each built by hand
# from a one-off command line that lived only in a session transcript. That makes
# "the suite still passes" an unverifiable claim the moment the session ends — and
# this account's standing rule is that a checker gets re-run after anything it
# guards changes. One script, one answer, no remembering.
#
# Needs llvm-mingw (i686-w64-mingw32-clang) on PATH, same as build.sh.
set -u
cd "$(dirname "$0")"

if command -v i686-w64-mingw32-clang >/dev/null 2>&1; then
    CC="i686-w64-mingw32-clang"
else
    CC="/c/Users/Tefa/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20260616-ucrt-x86_64/bin/i686-w64-mingw32-clang.exe"
fi

mkdir -p build
FLAGS="-O2 -Wall -Wextra"
fails=0

run_one() {              # run_one <name> <libs> <sources...>
    local name="$1"; shift
    local libs="$1"; shift
    printf '\n=== %s ===\n' "$name"
    if ! "$CC" $FLAGS -o "build/$name.exe" "$@" $libs -lm; then
        echo "BUILD FAILED: $name"; fails=$((fails + 1)); return
    fi
    if ! "./build/$name.exe"; then
        echo "RUN FAILED: $name"; fails=$((fails + 1))
    fi
}

# The libs column is load-bearing: wrap_selftest needs IID_IDirect3D9, which lives
# in dxguid/uuid and nowhere else — the same flags build.sh already carries.
run_one stereo_selftest  ""                  test/stereo_selftest.c  src/stereo.c
run_one camhunt_selftest ""                  test/camhunt_selftest.c src/camhunt.c
run_one thunk_selftest   ""                  test/thunk_selftest.c
run_one wrap_selftest    "-ldxguid -luuid"   test/wrap_selftest.c    src/wrap_d3d9.c src/camhunt.c src/proxy.c src/thunks.c

printf '\n============================================\n'
if [ "$fails" -eq 0 ]; then
    echo "ALL SUITES PASSED"
else
    echo "$fails SUITE(S) FAILED"
fi
exit "$fails"
