# 2026-09-14 — Dead Space 2, dev-PC static pass (NO LAUNCH)

**Machine:** dev PC `DESKTOP-V8GTSIR`. **Install:** `D:\Program Files (x86)\Steam\steamapps\common\Dead Space 2`,
Steam app 47780, `StateFlags=4` (fully installed) `[inferred-static 2026-09-14]` — the home PC's
2026-09-13 note flagged "installed on the home PC only, as far as this session knows"; **it is
installed here too**.

**The game was not launched.** Everything here is PE headers and strings read off disk.

## Files here

| File | What it is |
| --- | --- |
| `pe-imports-exports.txt` | PE header, sections, import table and the 12 exported symbols |
| `renderware-evidence.txt` | the mangled names and SDK text that identify the engine |
| `cmd-table-names.txt` | 4,828 `CMD_*` names — the engine's entity/script command vocabulary |
| `install-listing.txt` | install root contents |

## ⚠️ CORRECTION — the engine is RenderWare, not "Visceral's own"

The 2026-09-13 dossier recorded *"Visceral Games' own in-house engine `[reported]` — lineage not yet
checked against the binary"*. **It has now been checked against the binary, and the honest answer is
different** `[inferred-static 2026-09-14]`.

The exe's own **exported** symbols demangle into a namespace called **`EARS`**, sitting on top of
RenderWare's own `rw` and `RWS` namespaces:

```
?Open@StartUp@RWS@@YA_NXZ                                   -> RWS::StartUp::Open()
?RegisterEntityClasses@EARS@@YAXXZ                          -> EARS::RegisterEntityClasses()
?CreateMainWindow@Windows@EARS@@ ...                        -> EARS::Windows::CreateMainWindow(...)
?DefineInputTrigger@RemappableControls_DS@EARS@@ ... rw::core::controller::PcKeyScancode ...
??0LevelManager@MainLoop@EARS@@ ...                         -> EARS::MainLoop::LevelManager
```

⚠️ **RenderWare is the solid part of that.** Reading `EARS` as *EA RenderWare Studio* is a
plain-sense expansion of the acronym — inference, not something the binary states.

The `.rdata` section also carries **large slabs of RenderWare audio SDK documentation** verbatim
(`RwAudioCore`, `core::Pan2D1::SetDefaultFarRearSpeakerAngle`, speaker-mode prose for PS3/360).

⭐ **Why the correction is worth having.** "Visceral's own engine" implies a one-off nobody else has
studied. **RenderWare is a well-documented engine family, and this account already has a RenderWare
project** — `manhunt-2003-vr`. Whatever was learned about how RenderWare delivers its camera and
view matrices is a starting point here rather than a blank page. That is a link the old note actively
discouraged anyone from looking for.

⚠️ **What this does and does not say.** It establishes that EA's RenderWare Studio framework is
present and is what starts the program and registers its entity classes. It does **not** establish
that the *renderer* is stock RenderWare — Dead Space's renderer is heavily custom `[reported]`, and
a RenderWare shell around a bespoke renderer is a normal shape. Treat the lineage as **confirmed at
the framework level, open at the renderer level.**

## Protection: the activation layer is a hard static import, not an optional extra

The board's open risk was "an EA activation layer may stop the game starting at all". The static
picture sharpens it `[inferred-static 2026-09-14]`:

- `deadspace2.exe` **statically imports `activation.x86.dll`**, one function: `start`. That is in the
  import table, so Windows resolves it **before a single line of game code runs**. It is not
  something the game asks for later and can skip.
- `activation.exe`, `activation.x86.dll` and `activation.xml` ("Product activation", pointing at EA
  support) ship in the install root.
- ⚠️ **`activation.x64.dll` is 0 bytes on this machine.** The exe is 32-bit and imports the x86 one,
  so this is probably harmless — but a zero-byte DLL is odd enough to write down in case a launch
  fails in a way nobody can explain.
- Steam's DRM wrapper is present as the `.bind` section.
- The four oddly named sections the home PC spotted are real: `ri` and `aYv` have **zero raw size**
  (allocated at load, filled at runtime — the classic unpacking scratch space), `QuFIo` holds 15.9 MB
  of raw data, `sr` 210 KB.

**But the exe is not opaque.** Despite the packer-shaped sections, ordinary strings read perfectly —
4,828 `CMD_*` names, whole paragraphs of SDK documentation, the full mangled export list. Only the
**import table** shows the packed shape: one function per DLL (`CompareStringW`, `MessageBoxA`,
`GetStockObject` …), which is the tell-tale of a wrapper that resolves the real imports after
unpacking.

## The command table: 4,828 named commands

`cmd-table-names.txt` is the whole list. It is the engine's scripting/entity vocabulary, and the
camera portion is unusually rich: `CMD_BaseCamera`, `CMD_ResetCamera`, `CMD_AttachToActiveCamera`,
`CMD_SetAttachFov`, `CMD_DriftCamera`, `CMD_DeathCamera`, `CMD_PuzzleCamera`, `CMD_MiniGameCamera`,
`CMD_Set180TurnCamera`, `CMD_SetAllowCameraSway`, the `CMD_SetBoneBoundCamera*` blend family, and
`CMD_AIAC_SetVisionFOV` / `CMD_AIAC_SetFiringFOV` for the AI.

⚠️ **These are almost certainly level-script commands, not a player console.** Nothing found suggests
a console the player can open — no `CVar` class, no console-registry vocabulary of the kind Hard
Reset has. Do not plan around typing these in. What they *are* good for is **naming the engine's own
camera concepts**, which is what makes a later disassembly readable.

⭐ **`CMD_SetBoneBoundCamera*` is worth flagging now.** A camera bound to a character's bone, with
blend durations and "pop" handling, is the hardest possible starting point for head tracking — it
means the camera is driven by animation, not by a simple position-and-angles pair. If that is the
main gameplay camera, this project is expensive. `[hypothesis]` — the names are evidence of what
exists, not of what the normal camera uses.

## Binary facts

| | |
| --- | --- |
| `deadspace2.exe` | **32-bit**, 48.4 MB, link timestamp **zeroed** (a packer trait) `[inferred-static 2026-09-14]` |
| Module base | `0x400000`, **ASLR off**, relocations present — the base never moves |
| Renderer | **Direct3D 9, confirmed from the import table**: `d3d9.dll → Direct3DCreate9` |
| Shader reflection | `d3dx9_43.dll → D3DXGetShaderConstantTable` — shaders carry named constants |
| Input | `DINPUT8.dll → DirectInput8Create`, plus `XINPUT1_3.dll` |
| Online | EA's Blaze backend (`Blaze::Authentication::*` strings) |
| Useful exports | `EARS::Windows::CreateMainWindow(bool, WindowState, int, int, int, int)` — **windowed mode is a parameter, not a config guess** |

## What this does NOT establish

- **Nothing has been run.** The activation layer's actual behaviour is entirely untested, and it
  remains the first thing a launch must answer.
- Whether the packer merely wraps the exe or actively resists a debugger is unknown. The readable
  strings suggest a light wrapper, not an aggressive protector — `[hypothesis]`, based on how much is
  visible rather than on anything tested.
- The `DS2DAT*.DAT` archives were listed, never opened.
- The renderer's own camera delivery (dossier §6) is completely untouched.
