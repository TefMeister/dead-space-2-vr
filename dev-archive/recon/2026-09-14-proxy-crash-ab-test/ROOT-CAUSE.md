# 2026-09-14 — Root cause found and proven: `D3DPERF_GetStatus`

**Dev PC. Tefa launching, Claude reading.** `[verified-live 2026-09-14, n=1 clean run after 4 crashes]`

## The finding

Dead Space 2 calls **`D3DPERF_GetStatus`** from `d3d9.dll` during start-up. The stage-1 proxy did not
export it, so the game's lookup returned **NULL**, and the game called NULL.

That is not an inference from the fix working — the timing is on the clock:

| time | event | |
| --- | --- | --- |
| 14:47:40.877 | forwarding table built, 16 of 16 resolved | proxy ready |
| **14:47:46.953** | **`D3DPERF_GetStatus` called** | **≈ 6 s after attach** |
| 14:47:46.953 | `D3DPERF_SetOptions` called | immediately after |
| 14:47:47.098 | `Direct3DCreate9` called | ⭐ **D3D init reached for the first time** |
| 14:47:47.260 | real `Direct3DCreate9` returned `0592BAA0` | |
| 14:47:48.152 | `DebugSetMute` called | |
| 14:48:18.029 | detached | Tefa quit normally |

**Stage 1 crashed ≈ 8 seconds after attach, having never reached `Direct3DCreate9`.** Stage 2 calls
`D3DPERF_GetStatus` ≈ 6 seconds after attach and then immediately reaches `Direct3DCreate9`. The
crash window and the call sit in the same place in the sequence, and the crash signature —
`0xc0000005`, **fault offset `0x00000000`, faulting module `unknown`**, DEP kill — is exactly what
calling a NULL function pointer produces.

⭐ **Three exports are actually used by this game**, and stage 1 provided none of them:
`D3DPERF_GetStatus`, `D3DPERF_SetOptions`, `DebugSetMute`. The first is the one that killed it.

## Why this counts as a root cause rather than a symptom fix

This project's own notes warn about the trap: *a fix that removes the symptom **and** stops the
failing path from being exercised has proved nothing.* That is why each thunk logs its first call.
The failing path is not merely still exercised — **it is exercised and logged by name**, six seconds
in, in the same run that then succeeds. The mechanism is observed, not assumed.

The competing hypothesis — a signature or authenticity check on the DLL — is now **`[disproved 2026-09-14]`**:
an unsigned DLL of ours runs the game to completion. It was a live and reasonable hypothesis
(a genuine signed Microsoft DLL also ran fine), and the log is what separated them.

## ⚠️ This is not a Dead Space 2 problem. It is an account-wide latent defect.

Auditing every `d3d9` proxy `.def` in the estate `[inferred-static 2026-09-14]`:

| project | exports | |
| --- | --- | --- |
| `psychonauts-vr` (`dev-archive/` and `mod/`) | **1** — `Direct3DCreate9` | ⚠️ same shape as the build that crashed here |
| `staging/alan-wake-vr` | **1** — `Direct3DCreate9` | ⚠️ same shape |
| `staging/prince-of-persia-2008-vr` | **1** — `Direct3DCreate9` | ⚠️ same shape |
| `staging/alice-madness-returns-vr` | 2 — plus `D3DPERF_SetOptions` | partially hardened |
| `staging/enslaved-vr` | 9 | most hardened |
| `dead-space-2-vr` | **17 (all)** | fixed here today |

⚠️ **Do NOT read this as "those three are broken."** Psychonauts has a deployed, working proxy — so
those games evidently do not call the missing exports. The defect is **latent**: it fires only if the
game asks for an export the proxy lacks, and then it fires as a crash **before** the mod does anything
visible, which is the worst possible time to meet it.

⭐ **Alice is the tell.** It exports `Direct3DCreate9` *and* `D3DPERF_SetOptions` — exactly two. That
is the signature of someone hitting this once, patching the single function that bit them, and moving
on. The general lesson was available then and was not taken.

**The cheap general fix:** export all seventeen and forward the ones you do not implement. The naked
thunk approach in `../../tools/proxy-d3d9/src/thunks.c` is signature-agnostic, so it costs one file
and needs no knowledge of the undocumented exports. `test/thunk_selftest.c` proves the thunks
forward and keep the stack balanced without any game involved.

## The other lesson, which cost the most time today

Stage 1 crashed the game, and **I could not tell my own bug from the game's protection**, because the
only place the code ever ran was inside a protected game. Two rounds went into separating them.

The self-test harness exists because of that: it runs the thunks in our own process, with no game and
no protection anywhere near. **A crash there is our bug; a crash in the game is the game's.** Any
future proxy on this account should carry one before it is ever deployed.
