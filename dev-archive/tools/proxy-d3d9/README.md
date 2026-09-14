# proxy-d3d9 — stage 1, the activation probe

**Built and deployed 2026-09-14 (dev PC). Not yet run — the game has not been launched with it.**

## What this is for

`deadspace2.exe` statically imports `activation.x86.dll` (one function, `start`)
`[inferred-static 2026-09-14]`, so EA's activation layer runs before any game code. That makes one
question sit in front of every other question on this project:

> **Does the activation layer tolerate a foreign DLL in the game folder at all?**

Camera hunting, stereo and head tracking are all downstream of the answer. So stage 1 answers only
that, and answers it as cheaply as possible.

## What it does

Almost nothing, deliberately. It exports `Direct3DCreate9`, forwards it to the real system
`d3d9.dll`, and writes a few lines to `ds2_proxy.log`. No hooking, no vtable patching, no allocation.

⭐ **The smallness is the design, not laziness.** If the game fails to start, the cause is
unambiguous *because* there is nothing in here to blame. A stage-1 proxy that also hunted matrices
would have made a failure uninterpretable — "was it the activation layer, or was it my hook?" — and
that is the one outcome worth paying to avoid.

## Build

```
bash build.sh
```

32-bit (`i686-w64-mingw32-clang`, llvm-mingw), because `deadspace2.exe` is PE32/i386.
`[compile-verified 2026-09-14]` — clean build, zero warnings under `-Wall -Wextra`, export table
shows exactly `Direct3DCreate9`.

**The build is hash-reproducible** `[verified-numerically 2026-09-14, n=2]`: two builds of identical
source produce byte-identical output, `sha256 acfc9bfd77a5…`. That is what `-Wl,--no-insert-timestamp`
buys, and without it the estate's "rebuild and compare the hash to see if the deployed file is
current" check silently cannot work.

## Deployed

`D:\Program Files (x86)\Steam\steamapps\common\Dead Space 2\d3d9.dll`, 59,392 bytes,
`sha256 acfc9bfd77a5…`, recorded with `deployed.sh record` and verified `OK`.

⚠️ **Nothing was overwritten.** There was no `d3d9.dll` in that folder; the deploy was checked for an
existing file first and would have refused.

## How to undo it

Delete that `d3d9.dll`. That is the whole reversal — **no game file was modified and no registry key
was written.**

## The test, and what each outcome means

Launch the game normally from Steam. Then:

| What you see | What it means | Where the proof is |
| --- | --- | --- |
| The game reaches its menu as usual | ⭐ **The route is open.** Activation does not object to a foreign DLL. Stage 2 can begin. | `ds2_proxy.log` exists and ends with `forwarded; the real Direct3DCreate9 returned …` |
| An activation or tamper error | Activation refuses the route. We need a different way in. | log may exist (proxy loaded, then activation complained) or may not (blocked earlier) |
| Nothing happens, or it crashes at once | Read the log before concluding anything | if the log exists, the proxy loaded and something *after* it broke — which is a different problem from activation |

**The log is written next to the DLL** (`Dead Space 2\ds2_proxy.log`) if that folder is writable, and
to `%LOCALAPPDATA%\ds2_proxy.log` if it is not. ⚠️ The fallback matters: this install sits under
`D:\Program Files (x86)\`, and a silently unwritable folder would give **no log at all**, which reads
exactly like "the proxy never loaded" and would send the next session hunting the wrong failure. The
log names its own path in its third line, so there is never any doubt which one was used.

## Stage 2, when stage 1 passes

Port the mature proxy from `staging/alan-wake-vr/proxy-d3d9/`. It already hooks
`SetVertexShaderConstantF`, recognises perspective-matrix-shaped uploads and logs the register they
land in — which is exactly how this game's view-projection matrix will be found. Dead Space 2 also
imports `D3DXGetShaderConstantTable`, so its shaders carry **named** constants, which should make
that hunt easier here than it was there.
