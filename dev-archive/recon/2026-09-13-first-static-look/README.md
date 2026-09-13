# First static look (2026-09-13)

Read from the installed Steam copy on the home PC, without launching the game. Every claim
below is `[inferred-static 2026-09-13]` unless tagged otherwise: it comes from reading file headers
and strings, not from running anything.

- **Install:** `Dead Space 2`, 9.4 GB.
- **Identity:** Dead Space 2 (2011), Steam build, exe `deadspace2.exe`.
- **Engine:** Visceral Games' own in-house engine `[reported]` — lineage not yet checked against the binary. Havok, Scaleform GFx and Lua strings are present in the exe `[inferred-static 2026-09-13]`.
- **Binary:** **32-bit** (PE32), `deadspace2.exe` 48.4 MB, link timestamp zeroed. Sections: a normal `.text`/`.rdata`/`.data` set, plus `.bind` (the Steam DRM wrapper's section) and four oddly named ones (`ri`, `aYv`, `QuFIo`, `sr`, about 20 MB together) that look like a protection layer `[inferred-static 2026-09-13]`.
- **Renderer:** Direct3D 9: `d3d9.dll` appears in the exe's strings `[inferred-static 2026-09-13]`. XInput and DirectInput 8 strings are also present.
- **Protection:** Two layers suspected, neither tested: the Steam DRM wrapper (`.bind`), and an EA-era **product activation** (`activation.exe`, `activation.x86/x64.dll`, and `activation.xml` titled "Product activation", pointing at EA support) `[inferred-static 2026-09-13]`. The unusual extra sections suggest a packer on top.
- **Other files:** Data lives in `DS2DAT*.DAT` archives, not yet looked at.

## Method

PE headers read with a short script: machine type, link timestamp, section names and sizes.
Then a case-insensitive search of each binary for renderer DLL names (`d3d9`, `d3d11`, `d3d12`,
`dxgi`, `vulkan-1`, `opengl32`), protection markers (`denuvo`, `securom`, `.bind`) and middleware
names. A string match shows a name is present in the file, not that the code path is used.

## Risks noted

- ⚠️ **The Burnout Paradise lesson applies:** an EA activation layer may stop the game starting at all. The very first job is to launch it unmodified and see that it reaches gameplay, before anything is built.
- The suspected packer may make static reading of the exe hard until it has unpacked itself in memory.
