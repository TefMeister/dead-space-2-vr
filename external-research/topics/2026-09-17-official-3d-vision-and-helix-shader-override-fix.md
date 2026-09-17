# Dead Space 2 shipped 3D Vision, and HeliX's fix made it a showcase: a stereo reference for the c0/c4 work

**Status:** 🆕 new · **Priority:** medium — it does not answer the c0 question directly, but it is a
working stereo reference for the same D3D9 renderer.

## What is public

- Dead Space 2 had "mediocre" **official NVIDIA 3D Vision support**, and **HeliX's** fix turned it into
  what the community called a 3D Vision showcase, playable at any convergence and separation with all
  effects on `[reported]`.
- The fix is HeliX's `d3d9.dll` wrapper plus `DX9Settings.ini` and a **`ShaderOverride` folder**, placed
  next to `deadspace2.exe`. It fixes shadows, lights and halos; flares still misbehave; a 2018 update by
  **PaulDusler** added a hold-to-aim low-separation preset `[reported]`.
- Crash notes from the fix page: disable the Origin in-game overlay and the Steam overlay `[reported]`.

## Why it matters here

1. **HeliX-style fixes work by editing specific vertex and pixel shaders, per hash.** Which shaders
   needed correcting tells us which effects reconstruct world position from a projection that 3D
   Vision's automatic stereo got wrong — the same shaders that will break when we shift c4 per eye
   `[hypothesis]`.
2. **The game's own 3D Vision path is a live oracle.** With 3D Vision (or geo-11 / a wrapper that
   drives the same path) active, the per-eye shift the driver applies can be compared against the
   dossier's derived stereo sign `[hypothesis]`.
3. The fix page's crash notes match this project's early-launch crash symptoms only loosely; worth a
   look if the overlay is on.

## Next step

Read the `ShaderOverride` folder's file list and `DX9Settings.ini` (our reading, not redistribution) to
see which shader hashes are touched and which constants they adjust.

## Sources

- Helix Mod, "Dead Space 2 - 3D Vision fix" — <https://helixmod.blogspot.com/2012/04/dead-space-2-3d-vision-fix.html>
- PCGamingWiki, Dead Space 2 — <https://www.pcgamingwiki.com/wiki/Dead_Space_2>
