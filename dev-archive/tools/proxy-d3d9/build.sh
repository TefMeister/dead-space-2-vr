#!/bin/bash
# Builds the 32-bit d3d9.dll proxy for Dead Space 2 (deadspace2.exe is PE32/i386).
# Run from Git Bash. Needs llvm-mingw (i686-w64-mingw32-clang) on PATH.
#
# Recipe copied from staging/alan-wake-vr/proxy-d3d9/build.sh, including its
# hard-won flags — see the comments below.
set -e
cd "$(dirname "$0")"

if command -v i686-w64-mingw32-clang >/dev/null 2>&1; then
    TOOLCHAIN="$(dirname "$(command -v i686-w64-mingw32-clang)")"
else
    TOOLCHAIN="/c/Users/Tefa/AppData/Local/Microsoft/WinGet/Packages/MartinStorsjo.LLVM-MinGW.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe/llvm-mingw-20260616-ucrt-x86_64/bin"
fi
CC="$TOOLCHAIN/i686-w64-mingw32-clang.exe"

mkdir -p build

# -Wl,--no-insert-timestamp: without it the PE TimeDateStamp changes on every
# link, so two builds of identical source differ by two bytes and hash
# differently. CONVENTIONS.md tells a session to rebuild and compare the hash to
# decide whether a deployed DLL is current, and without this flag that check
# silently cannot work. Four projects have been found with this defect.
"$CC" -shared -O2 -Wall -Wextra \
    -o build/d3d9.dll \
    src/proxy.c src/thunks.c src/camhunt.c src/d3d9.def \
    -Wl,--no-insert-timestamp \
    -luser32 -lkernel32

echo "Built build/d3d9.dll"
"$TOOLCHAIN/i686-w64-mingw32-objdump" -p build/d3d9.dll | sed -n '/Export Table/,/^$/p'
file build/d3d9.dll
sha256sum build/d3d9.dll
