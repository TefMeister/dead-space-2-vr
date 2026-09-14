# 2026-09-14 — The stage-1 proxy stops Dead Space 2 launching. A/B confirmed.

**Dev PC. Tefa did the launching; Claude never launched the game.**

## The claim I got wrong, stated plainly

When Tefa reported the game had stopped launching, I had two candidates and I named the wrong one as
likely: the windowed-mode change they had made. **It was not the windowed setting. It was my proxy.**

The morning's note also read the first log line — `PROXY LOADED — a foreign DLL was allowed into the
process` — as meaning the activation layer tolerated the proxy. **That was too strong.** It shows the
DLL was allowed to *load*. The process then died a few seconds later, which is exactly what a
late-acting tamper check looks like.

## The evidence

Three crashes on 2026-09-14 at 14:02:46, 14:03:31 and 14:04:46, plus a fourth at 14:08 during the
A/B. **All four identical** (Windows Error Reporting):

```
Faulting application name: deadspace2.exe
Exception code: 0xc0000005          (access violation)
Fault offset:   0x00000000
Faulting module name: unknown
Event Name: BEX                     (DEP / execute-protection kill)
P7: PCH_91_FROM_ntdll+0x0007379C    P8: c0000005    P9: 00000008
```

Fault offset `0x00000000` with **no owning module** means execution jumped to address zero — a call
through a NULL function pointer — and DEP killed it.

The proxy's own log, from every attempt (`ds2_proxy-first-three-crashes.log`,
`ds2_proxy-ab-test-default-settings.log` in the sibling recon folder):

```
=== stage-1 proxy attached ===
host process: ...\deadspace2.exe
PROXY LOADED — a foreign DLL was allowed into the process.
real d3d9 loaded from C:\Windows\system32\d3d9.dll (module 738D0000), Direct3DCreate9 = 73934B20
```

⭐ **and never the next line.** `Direct3DCreate9(...) called <-- THE GAME REACHED D3D9 INIT` does not
appear in any run. **The game dies before it reaches Direct3D initialisation at all**, roughly eight
seconds after the proxy attaches.

## The A/B, one variable at a time

The two changes were confounded: the settings file was written at **13:47:11** and the proxy deployed
at **13:55**, so the first launch that would *read* `Window.Fullscreen = false` was also the first
launch with the proxy present. Neither could be blamed on timing alone.

The settings file was therefore moved aside, putting the game back on defaults — the exact state it
was in during its one successful run at 13:46, when no settings file existed yet.

| # | settings | proxy | result |
| --- | --- | --- | --- |
| 1 | none (defaults) | no | **ran** — Tefa reached the options and set windowed (13:46–13:47) |
| 2 | windowed | yes | crashed ×3 (14:02, 14:03, 14:04) |
| 3 | none (defaults) | yes | **crashed** (14:08) — settings eliminated |
| 4 | none (defaults) | no | **ran** — Tefa confirmed "dead space works again" |

Rows 3 and 4 differ in exactly one thing. **The proxy is the cause** `[verified-live 2026-09-14, n=4 crashes / 2 clean runs]`.

⚠️ Also worth recording: in run 3 the game **did not regenerate `settings.txt`**, which it would have
done on a normal exit. It dies early enough to leave no trace of itself.

## What is NOT yet established — the two live hypotheses

The A/B says *my proxy*. It does not say *why*, and there are two very different answers:

1. **Missing exports.** The visible import table is the DRM stub's, listing one function per DLL. The
   real import table is rebuilt after unpacking, and may pull more from `d3d9.dll` than
   `Direct3DCreate9` — the system DLL exports 17 (the `D3DPERF_*` family, `Direct3DCreate9Ex`,
   `Direct3DShaderValidatorCreate9`, …). My proxy exports **one**. Anything else resolves to NULL, and
   calling NULL gives **precisely** offset `0x00000000`, module `unknown`, DEP kill. `[hypothesis]`
2. **A tamper check.** The activation or Steam wrapper notices an unsigned, unexpected `d3d9.dll` in
   the game directory and sabotages execution. Anti-tamper commonly crashes rather than reporting,
   and a jump to a junk address is a normal way to do it. `[hypothesis]`

⚠️ Searching the exe for `D3DPERF_*` name strings found none — but the DRM may hold its import names
compressed or encrypted, so that is **not** evidence against hypothesis 1.

## The experiment that separates them, and it needs no code

Put a **genuine, byte-identical copy of Microsoft's own `C:\Windows\SysWOW64\d3d9.dll`** in the game
folder and launch.

- **Crashes** → hypothesis 2. The game rejects *any* local `d3d9.dll`, and the same-named-proxy route
  is dead here. Pivot: try `dinput8.dll` (also a static import) to find out whether it objects to
  foreign DLLs generally or to a graphics one specifically.
- **Runs** → hypothesis 1. The folder is fine and my DLL specifically is at fault, with exports the
  prime suspect. Fix: export all 17, forwarding the 16 we do not implement.

⭐ **Why this is the right next test:** it removes *my code* from the equation entirely while keeping
*a foreign file in the folder*. Neither hypothesis can hide behind the other.

## Reversal

Delete `d3d9.dll` from the game folder. Tefa's windowed `settings.txt` is preserved at
`%LOCALAPPDATA%\EA Games\Dead Space 2\settings.txt.2026-09-14-backup-windowed` and is to be restored
once testing is finished — it is being kept out of the way only to hold the variable count at one.
