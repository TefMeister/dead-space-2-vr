# Helix 3d vision fix as stereo reference

**From:** `/gr` estate sweep, home PC, 2026-09-17.

**Relevant to:** the c0/c4 stereo-sign `[PD]` rows and the "named shader constants" row.

Dead Space 2 shipped official 3D Vision, and **HeliX's** `d3d9.dll` + `ShaderOverride` fix corrected
shadows, lights and halos to make it fully playable at any separation `[reported]`. The overridden
shader hashes name the effects that break under a per-eye shift, and the game's own 3D Vision path is a
possible oracle for the stereo sign `[hypothesis]`.

Suggested dossier change: note it in §11 (prior art) as a reference to consult before the per-eye c4 test.
Topic: `external-research/topics/2026-09-17-official-3d-vision-and-helix-shader-override-fix.md`
