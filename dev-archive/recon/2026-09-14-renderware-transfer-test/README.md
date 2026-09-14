# 2026-09-14 — Does Manhunt's RenderWare camera work transfer? No.

**Dev PC, no launch.** This closes the `[PD]` row queued earlier the same day:
*"check what Manhunt's RenderWare work already established about how that engine family delivers its
camera and view matrices, and whether any of it transfers here"*.

**The answer is no, and that is worth having written down** — it stops a future session spending a
day looking for structures that are not there.

## What Manhunt established (and it is a lot)

`manhunt-2003-vr`'s dossier is deep on this. The transferable-looking parts were:

- the engine is **RenderWare 3.6**, identified from `rwsdk` source-path strings left in the binary;
- **RenderWare expresses FOV as a "view window" = `tan(fov/2)` per axis**, not as an angle —
  set through `RwCameraSetViewWindow`;
- the camera is an **`RwCamera*`** whose **`RwFrame`** carries position and orientation;
- Manhunt's conclusion for head tracking was to **write the translated camera frame before
  `RwCameraBeginUpdate`**, rather than hunt a projection matrix;
- windowed mode came from a RenderWare video-mode index read out of the registry and handed to
  `RwEngineSetVideoMode`.

## The test

Search `deadspace2.exe` for that vocabulary: `RwCamera`, `RwFrame`, `BeginUpdate`, `EndUpdate`,
`ViewWindow`, `rwsdk`, `RwEngine`, `RwMatrix`, `RpWorld`.

**Result: not one hit** `[inferred-static 2026-09-14]`.

## The control — because a negative is only evidence if the test could have produced a positive

The identical search was run against `manhunt.exe`, and it floods:

```
@@(#)$Id: //RenderWare/RW36Active/rwsdk/src/bacamera.c#2 $
@@(#)$Id: //RenderWare/RW36Active/rwsdk/src/babincam.c#1 $
@@(#)$Id: //RenderWare/RW36Active/rwsdk/world/baworld.c#2 $
… (many more)
```

So the test works, and the negative on Dead Space 2 is real rather than a broken search.

## What this means

The 2026-09-14 morning entry recorded the lineage as **"confirmed at framework level, open at
renderer level"**, on the strength of the `EARS`, `RWS` and `rw` namespaces in the export table and
the RenderWare *audio* SDK documentation in `.rdata`. That framing turns out to have been exactly
right, and this test settles the open half:

- **The framework is RenderWare-derived** — the startup, entity registration and input layers.
- **The renderer is not classic RenderWare 3.x.** None of its camera machinery is present under
  those names.

⚠️ **What this does NOT prove.** It does not prove the renderer shares *nothing* with RenderWare —
only that the RenderWare 3.6 identifiers are absent. A later in-house evolution could keep the
concepts and drop the names, and strings prove nothing about code that carries no strings. What it
does establish is that **Manhunt's specific offsets, function names and the `RwCameraBeginUpdate`
plan are not a shortcut here** `[inferred-static 2026-09-14]`.

## So where does the camera hunt actually start?

Back at the ordinary route, which for this game is well signposted:

- It is **Direct3D 9 with `D3DXGetShaderConstantTable`** imported, so its shaders carry **named**
  constants — the view-projection matrix should be findable by name rather than by guesswork.
- That means the first real step is a **`d3d9.dll` proxy watching `SetVertexShaderConstantF`**, the
  same technique already built and working in `staging/alan-wake-vr/proxy-d3d9/`.
- Which runs straight into the activation layer, and so into the stage-1 probe built the same day:
  `dev-archive/tools/proxy-d3d9/`.
