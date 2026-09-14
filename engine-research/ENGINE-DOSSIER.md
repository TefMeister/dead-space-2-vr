# Engine Dossier — Dead Space 2 (EA RenderWare Studio / RenderWare)

> One consolidated, living reference for this game's engine, filled in as the
> `PLAYBOOK.md` phases are worked. Chronological blow-by-blow belongs in the
> `dev-archive/` and `modding-notes/` folders; this file is the *distilled current
> truth*. Update it whenever a fact changes; correct false leads in place.

**Status:** M0, static recon done on both machines (2026-09-13 home, 2026-09-14 dev PC); the game has **not** been launched yet. · **VR-readiness verdict:** TBD, and the hardest of the 2026-09-13 batch. Two open worries: an activation DLL that is a **hard static import**, and camera names suggesting the gameplay camera may be animation-driven.

## 1. Identity
- Game / build / version: Dead Space 2 (2011), Steam build, exe `deadspace2.exe`.
- Platform & store; unofficial port? (extra fragility/legal notes): Steam (PC). Official release, not a fan port.
- Legitimacy: owned copy confirmed.

## 2. Engine lineage
- Family / base engine and how it was modified: ⚠️ **CORRECTED 2026-09-14.** The 2026-09-13 entry read *"Visceral Games' own in-house engine `[reported]` — lineage not yet checked against the binary"*. It has now been checked. The exe's **exported** symbols demangle into **`EARS`** layered on RenderWare's own `rw` and `RWS` namespaces — **RenderWare is the solid part**; reading `EARS` as *EA RenderWare Studio* is a plain-sense expansion of the acronym and is inference, not something the binary states `[inferred-static 2026-09-14]`: `RWS::StartUp::Open()`, `EARS::RegisterEntityClasses()`, `EARS::MainLoop::LevelManager`, `EARS::Windows::CreateMainWindow(...)`, and `rw::core::controller::PcKeyScancode` inside `EARS::RemappableControls_DS::DefineInputTrigger`. `.rdata` also carries verbatim slabs of **RenderWare audio SDK documentation** (`RwAudioCore`, `core::Pan2D1::SetDefaultFarRearSpeakerAngle`).
  - ⭐ **Why this matters:** "Visceral's own engine" implies a one-off nobody has studied. RenderWare is a documented family, and **this account already has a RenderWare project — `manhunt-2003-vr`**. That is a link the old wording actively discouraged.
  - ⚠️ **What it does NOT say:** this establishes the **framework** (what starts the program and registers entity classes), not the **renderer**. Dead Space's renderer is heavily custom `[reported]`, and a RenderWare shell around a bespoke renderer is a normal shape. Lineage: confirmed at framework level, open at renderer level.
  - Havok, Scaleform GFx and Lua strings are also present `[inferred-static 2026-09-13]`; EA's **Blaze** online backend appears as `Blaze::Authentication::*` `[inferred-static 2026-09-14]`.
- Middleware (animation, audio, physics, megatexture, CUDA, etc.):
- Distinctive file formats / build tags / symbol naming: Data lives in `DS2DAT*.DAT` archives, not yet looked at.

## 3. Binary & memory
- 32/64-bit, size, module base, ASLR behaviour (stable base? relocations?): **32-bit** (PE32), `deadspace2.exe` 48.4 MB, link timestamp zeroed (a packer trait). Sections: a normal `.text`/`.rdata`/`.data` set, plus `.bind` (the Steam DRM wrapper's section) and four oddly named ones (`ri`, `aYv`, `QuFIo`, `sr`, about 20 MB together) that look like a protection layer `[inferred-static 2026-09-13]`. **Module base `0x400000`, ASLR OFF, relocations present** `[inferred-static 2026-09-14]` — the base does not move.
- Renderer API (D3D11/12, DXGI, GL, Vulkan) with evidence: **Direct3D 9, confirmed from the import table** (not just strings): `d3d9.dll → Direct3DCreate9` `[inferred-static 2026-09-14]`. It also imports **`d3dx9_43.dll → D3DXGetShaderConstantTable`**, so shaders carry named constants. Input is `DINPUT8.dll → DirectInput8Create` plus `XINPUT1_3.dll`, both real imports.
- Developer console / cvar system present? how opened?: **No player console found** `[inferred-static 2026-09-14]` — no `CVar` class, no console registry vocabulary of the kind Hard Reset carries. What does exist is a **4,828-entry `CMD_*` table**, the engine's entity/script command vocabulary (full list: `dev-archive/recon/2026-09-14-dev-pc-static-pass/cmd-table-names.txt`). Treat these as level-script commands, **not** as something to type.

## 4. DRM / anti-debug & injection foothold
- DRM (CEG/Denuvo/GOG/none); launch-time-debugger behaviour: Two layers suspected, neither tested: the Steam DRM wrapper (`.bind`), and an EA-era **product activation** (`activation.exe`, `activation.x86/x64.dll`, and `activation.xml` titled "Product activation", pointing at EA support) `[inferred-static 2026-09-13]`. The unusual extra sections suggest a packer on top.
- Attach workflow that works: not yet tested. **A stage-1 `d3d9.dll` proxy is built and deployed as of 2026-09-14** (`dev-archive/tools/proxy-d3d9/`) purely to answer whether the activation layer tolerates a foreign DLL in the game folder. It forwards `Direct3DCreate9` to the system DLL and logs; nothing else. `[compile-verified 2026-09-14]`, hash-reproducible `[verified-numerically 2026-09-14, n=2]`, **not yet run**. Reversal is deleting one file. ⚠️ But the static shape is now clearer: **the exe is not opaque.** Ordinary strings read perfectly (4,828 command names, whole paragraphs of SDK documentation, the full mangled export list). Only the **import table** shows the packed shape — one function per DLL (`CompareStringW`, `MessageBoxA`, `GetStockObject`…), the tell-tale of a wrapper that resolves the real imports after unpacking. That suggests a light wrapper rather than an aggressive protector `[hypothesis]`, based on how much is visible rather than on anything tested. The `ri` and `aYv` sections have **zero raw size** (allocated at load, filled at runtime — classic unpacking scratch space); `QuFIo` holds 15.9 MB raw, `sr` 210 KB.
- Injection vector that works (proxy DLL name / injector / framework): ⭐ **`d3d9.dll` next to the exe WORKS — the game runs to its menu and quits cleanly with our proxy in place** `[verified-live 2026-09-14, n=1 clean run after 4 crashes]`. The requirement is that the proxy **export all seventeen of the system DLL's functions**, not just `Direct3DCreate9`: this game calls `D3DPERF_GetStatus`, `D3DPERF_SetOptions` and `DebugSetMute`, and an unexported one resolves to NULL and is then called. Root cause and timing proof: `dev-archive/recon/2026-09-14-proxy-crash-ab-test/ROOT-CAUSE.md`.
  - ⭐ **The activation layer does NOT object to an unsigned foreign DLL** — `[disproved 2026-09-14]` for the tamper-check hypothesis, which had been live and reasonable (a genuine signed Microsoft d3d9.dll also ran fine, so authenticity was a real candidate until the thunk log separated them).
  - **Historical, kept because it is the evidence:** the stage-1 proxy, exporting only `Direct3DCreate9`, stopped the game launching `[verified-live 2026-09-14, n=4 crashes / 2 clean runs]` — full A/B and evidence: `dev-archive/recon/2026-09-14-proxy-crash-ab-test/`.
  - The route is available *in principle*: the exe statically imports `d3d9.dll` by name, and an exe's own directory is searched before the system directory regardless of SafeDllSearchMode `[inferred-static 2026-09-14]`.
  - But in practice the game dies ~8 seconds in, **before Direct3D initialisation is ever reached** (the proxy logs its attach and the real DLL loading, and never logs the `Direct3DCreate9` call). Crash is `0xc0000005`, **fault offset `0x00000000`, faulting module `unknown`**, reported as a **BEX/DEP** kill — a jump through a NULL pointer.
  - ⚠️ **The earlier reading of the log line "PROXY LOADED — a foreign DLL was allowed into the process" as "the activation layer tolerates the proxy" was too strong.** It shows the DLL was allowed to *load*. The process dying seconds later is exactly what a late-acting tamper check looks like.
  - **Two live hypotheses, not yet separated** `[hypothesis]`: (1) **missing exports** — the real import table is rebuilt after unpacking and may need more of `d3d9.dll`'s 17 exports than the one we provide, and a NULL import called gives precisely this crash signature; (2) **a tamper check** on an unsigned, unexpected `d3d9.dll`.
  - **The separating experiment needs no code:** put a byte-identical copy of Microsoft's own `d3d9.dll` in the folder. Crash ⇒ (2), the route is dead here and `dinput8.dll` is the next thing to try. Runs ⇒ (1), and the fix is to export all 17.

## 5. Threading & frame structure
- Immediate context only, or deferred contexts + command lists?:
- Which thread(s) do what; render-thread name(s):
- One-frame walkthrough (record → replay → present):

## 6. Camera & projection delivery (the crucial section)

# 🏆 ANSWERED 2026-09-14 `[verified-live 2026-09-14, n=1 session, ~1.2M uploads]`

Evidence: `dev-archive/recon/2026-09-14-camera-found/`; notes: `modding-notes/2026-09-14c-the-camera-is-found.md`.

**The projection reaches the GPU as a dedicated 4-register vertex-shader constant write at `c4`.**

```
c4, layout R (register i = row i), written as "c4+4"
    [ -1.944444    0.000000    0.000000    0.000000 ]
    [  0.000000    3.456790    0.000000    0.000000 ]
    [  0.000000    0.000000    0.990097    1.000000 ]
    [  0.000000    0.000000   -0.099010    0.000000 ]
```

| Property | Value |
| --- | --- |
| Register / width | **c4**, 4 registers |
| Packing | **layout R** (w-from-z at index 11) |
| Handedness | **LEFT-handed**, `clip.w = +view.z` (`m[11] = +1`, all 24 sightings) |
| Aspect | **exactly −1.7778 = −16/9**, matching the 1280×720 window |
| X axis | **mirrored** (`xs` negative) |
| Near plane | **0.1000** (`−m[14]/m[10]`) |
| FOV at rest | **54.43° horizontal** |

⭐ **It is a PURE PROJECTION, not a view-projection** — the upper-left 2×2 is exactly diagonal, with
none of the rotation a combined matrix would carry. The view/world transform is elsewhere.

⭐ **How c4 was distinguished from the other candidates: it is the one that ANIMATES.** A field-of-view
sweep at 15:44:35–36 produced 21 consecutive announcements, **all at c4**, walking 60.00° → 70.00°
horizontal in even steps while c0 and c18 stayed put. The round endpoints corroborate the reading.

- `c0` is written far more often (36,987 per 5 s vs c4's 12,890) but announces **no** perspective during
  gameplay — almost certainly the **per-object world/world-view matrix** `[hypothesis]`. Confirming it
  is a `[PD]` job: log c0's contents for a few frames and see whether they change as the player turns.
  If it holds, head tracking edits **c0** and per-eye stereo shears **c4** — two separable changes.
- `c18` was a **false positive** (a UI block whose `ys` is zero); the detector now gates on both
  diagonal scale terms and keeps that exact block as a regression test.

⚠️ **The depth term is NOT the textbook form.** `m[10] = 0.990097` with `m[11] = +1`; a standard LH
perspective has `m[10] = zf/(zf−zn) > 1`, and solving for `zf` here gives a negative number. What it
actually does is `z_ndc = 0.990097 − 0.099010/z`, so near maps to 0 exactly and z→∞ approaches
0.990097 — **depth never reaches 1.0.** The near plane is solid; the far behaviour is flagged, not
explained. **Any stereo maths depending on the depth mapping must re-derive from this matrix rather
than assume the standard form** `[hypothesis]`.

⚠️ **The device is created PUREDEVICE** (`BehaviorFlags=0x54`), so D3D9 refuses `Get*` on shader
constants. A read-back instrument cannot work here and must be designed differently.

⚠️ **Nothing has been written.** The instrument is read-only; whether editing c4 actually moves the
picture is untested, and the sibling project met an engine that re-uploaded its constants and ignored
the edit.

---

### How it was found (the instrument)

⚠️ **STILL UNKNOWN — but the instrument that answers it is built and deployed** (2026-09-14, `/pd`,
no launch). `dev-archive/tools/proxy-d3d9/src/camhunt.c`; notes:
`modding-notes/2026-09-14-camera-instrument-built.md`.

⚠️ **The first design could not run and a launch proved it** (Tefa played to a save point,
2026-09-14): patching `IDirect3D9` slot 16 stood down every time because something else — almost
certainly the Steam overlay `[hypothesis]` — already held it, and standing down happens *before* a
device exists. Fixed by **wrapping `IDirect3D9` instead of patching it** (`src/wrap_d3d9.c`): we own
the `Direct3DCreate9` export, so we hand back our own object and write into no shared vtable. The
overlay's hook still runs, layered below ours. Notes: `modding-notes/2026-09-14b-standdown-and-the-wrapper-fix.md`.

⭐ **The whole chain is now proven outside the game** `[verified-numerically 2026-09-14]`: the
wrapper self-test creates a real device in our own process, the instrument installs on it, and a
planted perspective matrix is detected at the right register with the right handedness. Wrapper
forwarding checked 16/16 against the real `IDirect3D9`.

It hooks `IDirect3DDevice9::SetVertexShaderConstantF` (slot 94, compile-time asserted against the SDK
header) from inside the wrapper's `CreateDevice`, and reports **read-only** which register
receives a projection-shaped 4x4, in which packing, with a handedness reading. It is
**register-agnostic** — every 4-register window of every upload is tested, in both row and column
packings — and its "already seen" key is `(xs, ys)` rather than the register, so a shadow pass cannot
mask the camera at the same register.

Detector correctness: **11/11 against matrices built from the documented D3D formulae**
`[verified-numerically 2026-09-14]` — positives LH/RH perspective and their transposes plus a square
shadow-shaped frustum; negatives identity, orthographic and its transpose, a view matrix and its
transpose, all-zeros. Build reproducible `[verified-numerically 2026-09-14, n=2]`.

⚠️ **Whether this game's projection travels through `SetVertexShaderConstantF` at all is untested.**
If the log counts uploads but finds nothing perspective-shaped, that is a real finding and the
approach changes.

- How the world transform reaches the GPU (shared VP buffer / per-draw MVP /
  other), with **shader-reflection / disassembly evidence**: **open.** `c0+4` is written 36,987× per
  5 s and is never perspective-shaped after load, so it is probably the per-object world or
  world-view matrix `[hypothesis]`. Its contents have not been read.
- Exact constant-buffer slot, parameter name(s), byte offset(s), layout,
  handedness, row/column convention: **c4, four registers, layout R (register i = row i),
  row-vector `clip = view * M`, LEFT-handed (`m[11] = +1`, `clip.w = +view.z`)**
  `[verified-live 2026-09-14, n=1 session]`. Parameter names unknown — the DS2DAT archives have not
  been opened.
- Where projection `P` / FOV comes from: unknown in code; observed to animate smoothly (a 60→70°
  sweep), so something drives it per frame.

### ✅ The per-eye override maths — DERIVED 2026-09-14 `[verified-numerically 2026-09-14, 59 checks]`

For an eye displaced by `e` along `+view.x`, converging at distance `zc`, **two elements change and
no others**:

```
m[12] = -xs * e          (3,0)  eye offset  -> parallax, a 1/z term
m[8]  =  xs * e / zc     (2,0)  convergence -> a constant NDC x offset
```

`xs = m[0] = -1.944444`. Code `dev-archive/tools/proxy-d3d9/src/stereo.{c,h}`, test
`test/stereo_selftest.c`, note `modding-notes/2026-09-14d-the-stereo-shear-derived.md`.

⭐ **Both terms are in the PROJECTION, so per-eye stereo does not wait on c0 being confirmed.** This
corrects the earlier reading that c4's half was a single "shear": `m[8]` alone gives a flat image
shifted sideways by the same amount at every depth, which is not stereo. The selftest caught that on
its first run, by the disparity coming out constant.

⭐ **The depth triplet `m[10]/m[11]/m[14]` is carried over untouched and identically in both eyes**,
so this game's non-textbook depth mapping cannot be tripped over by the stereo maths. That closes the
worry recorded the same day — by construction rather than by argument.

⚠️ **SIGN TRAP.** `xs` is **negative** (X mirrored), and `sign(e_right − e_left) = sign(xs)`, so the
**right eye takes a NEGATIVE `e`** — the opposite of the textbook. Backwards swaps the eyes: no
crash, fine on a monitor, sickening in a headset. `stereo_right_eye_sign()` derives it from the
matrix rather than hard-coding it, and the test checks both directions.

⚠️ **That rule assumes the mirror is in the projection ALONE** `[hypothesis]`. If c0's view matrix
also negates X the two cancel and the correct sign flips back. **The check, needing no headset:**
read c0 and take the determinant of its upper-left 3×3 — **+1** means the rule stands as written,
**−1** means it inverts. That is the same row as the queued c0 logging job.

⚠️ **Nothing has been run against the game.** Untested: that the game accepts a modified c4, that c4
is the only projection consumer, and that nothing re-uploads it after we write. The game's own
culling is untouched, so edge pop-in is expected. `zc` has no measured value — the view-space unit
scale is unknown, so an IPD in millimetres cannot be set yet.

## 7. Constant-buffer fill mechanism
- Map/DISCARD ring / UpdateSubresource / D3D11.1 offset / **persistent map +
  memcpy** (trap):
- Can source contents be read cheaply (captured CPU pointer) or need staging
  read-back?:
- The chosen override patch point and why:

## 8. Pass inventory (by render target)
- Main scene (res/formats):
- Shadow passes (depth-only sizes):
- Post / AA chain (SMAA/TAA/motion vectors; downscale sizes):
- UI / HUD (how it's kept separate):

## 9. cvar / console cheat sheet
| command / cvar | effect | use |
|---|---|---|
| | | |

## 10. Autonomous harness recipe (this game)
- Launch to a known scene (commands used):
- In-process input / camera drive method that worked:
- Frame-capture method; where images land:

## 11. Dead ends & false leads (save future time)

- ⚠️ **Manhunt's RenderWare camera work does NOT transfer to this game. Do not go looking for it.**
  `[inferred-static 2026-09-14]` — evidence and method: `dev-archive/recon/2026-09-14-renderware-transfer-test/`.
  Searching `deadspace2.exe` for `RwCamera`, `RwFrame`, `BeginUpdate`, `EndUpdate`, `ViewWindow`,
  `rwsdk`, `RwEngine`, `RwMatrix` and `RpWorld` returns **nothing**. The same search against
  `manhunt.exe` floods with `//RenderWare/RW36Active/rwsdk/src/bacamera.c`-style paths, so the test
  works and the negative is real.
  - This settles the open half of the 2026-09-14 lineage note: **framework RenderWare-derived,
    renderer not classic RenderWare 3.x.**
  - ⚠️ It does **not** prove the renderer shares nothing with RenderWare — only that the 3.6
    identifiers are absent, and strings prove nothing about code carrying no strings. What it does
    kill is the shortcut: Manhunt's offsets, function names and its
    "write the camera frame before `RwCameraBeginUpdate`" plan do not apply here.
  - **So the camera hunt starts at the ordinary route:** a `d3d9.dll` proxy watching
    `SetVertexShaderConstantF`, helped by the fact that this game imports `D3DXGetShaderConstantTable`
    and so carries **named** shader constants.

## 12. Open risks toward the North Star
- ⚠️ **The Burnout Paradise lesson applies, and the static picture makes it sharper, not softer.** `deadspace2.exe` **statically imports `activation.x86.dll`** (one function, `start`) `[inferred-static 2026-09-14]`. Being in the import table means Windows resolves it **before a single line of game code runs** — it is not something the game asks for later and can skip. The very first job is still to launch it unmodified and see that it reaches gameplay, before anything is built.
- ⚠️ **`activation.x64.dll` is 0 bytes** on both machines' installs `[inferred-static 2026-09-14]`. The exe is 32-bit and imports the x86 one, so this is probably harmless — written down in case a launch fails in a way nobody can explain.
- ⚠️ **`CMD_SetBoneBoundCamera*` (a camera bound to a character bone, with blend durations and "pop" handling) is the hardest possible starting point for head tracking** — it means a camera driven by animation rather than by a simple position-and-angles pair. If that is what the main gameplay camera uses, this project is expensive. `[hypothesis]` — the names prove such cameras exist, not that the normal one is among them.
- ~~The suspected packer may make static reading of the exe hard until it has unpacked itself in memory.~~ **Partly retired 2026-09-14:** strings and exports read fine off disk; only the import table is hidden until unpacking. Code disassembly of `.text` is still unattempted and may yet be the hard part.
- ⭐ The camera vocabulary is unusually rich and is now written down (`CMD_BaseCamera`, `CMD_ResetCamera`, `CMD_AttachToActiveCamera`, `CMD_SetAttachFov`, `CMD_DriftCamera`, `CMD_DeathCamera`, `CMD_PuzzleCamera`, `CMD_Set180TurnCamera`, `CMD_SetAllowCameraSway`, `CMD_AIAC_SetVisionFOV`/`SetFiringFOV`). Naming the engine's own concepts is what makes a later disassembly readable.
- ⭐ `EARS::Windows::CreateMainWindow(bool, WindowState, int, int, int, int)` is exported — **windowed mode is a parameter of a named exported function**, not a config-file guess.
