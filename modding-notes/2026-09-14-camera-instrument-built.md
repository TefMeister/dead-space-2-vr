# 2026-09-14 — the camera instrument is built and deployed. `/pd`, no launch.

**The game was not launched and nothing here has been run against it.** Everything below is
compile-verified or numerically verified in a process of our own.

## What this closes

The board's top `[PD]` row: *"port the SetVertexShaderConstantF matrix hunt from
staging/alan-wake-vr/proxy-d3d9 into this proxy, now that the way in is proven"*.

It is ported, built, numerically tested and deployed. `src/camhunt.c` + `src/camhunt.h`,
wired into `proxy.c` through a `CreateDevice` hook.

## What it does

It watches every vertex-shader constant upload and reports which register receives a matrix shaped
like a projection. **Read-only — it never modifies an upload.** Stereo is a later question; this
answers "where is the camera" first.

Per five-second period it logs:

- the **upload-range histogram** (`c0+128:1234 c128+128:1234 …`), which says whether this engine
  flushes whole blocks or writes small ranges — that decides what `start` will mean when we later
  want to match a specific register;
- the **live perspective signatures** — register, packing, `xs`, `ys`, and `ys/xs`;
- the **distinct-signature table occupancy and drop count**;
- and, once each, every distinct projection in full, with a handedness reading.

**The camera is the signature whose `ys/xs` equals the display aspect ratio** (1.7778 at 16:9). A
shadow pass is typically square, ratio 1.0. That one line is what the next launch is for.

## Three lessons carried over from Alan Wake rather than re-learned

That project reached this shape over four sessions. Each of these is marked ⭐ in the source so it is
not tidied away by someone who has not paid for it:

1. **Register-agnostic.** Alan Wake's first instrument watched four candidate registers taken from a
   shader census, and the projection was at none of them. Here *every* 4-register window of *every*
   upload is tested. We do not know Dead Space 2's register and must not pretend to.
2. **Keyed by the projection, not the register.** Keying "already logged" by register meant a shadow
   pass at c0 permanently **masked** the camera projection at c0 — hiding the one matrix most needed.
   The key here is `(xs, ys)`, the diagonal scale terms: they differ between a shadow frustum and the
   camera, and they are **unchanged by transpose**, so the key does not depend on settling the
   packing first.
3. **A saturated table must say so.** Alan Wake's table filled during a load-time FOV settle and
   every later signature — including the settled gameplay FOV, the value actually wanted — was
   dropped **silently**. The drop count is printed here, and a separate LIVE table reports what is
   happening *now* rather than what was seen first.

**Both packings are tested.** A 4×4 reaches the GPU as four rows or four columns; testing only one is
how Alan Wake's first scan found nothing.

## What was verified, and how

| Claim | Evidence |
| --- | --- |
| It builds clean | `-Wall -Wextra`, zero warnings `[compile-verified 2026-09-14]` |
| Device vtable slot 94 is `SetVertexShaderConstantF` | compile-time negative-array assertion against the SDK's `IDirect3DDevice9Vtbl`; the build fails if it is ever untrue |
| `IDirect3D9` slot 16 is `CreateDevice` | same idiom, same guarantee |
| The detector is correct | **11/11 checks** against matrices built from the documented D3D formulae `[verified-numerically 2026-09-14]` — see below |
| The offset scan works | a 128-register block with the projection planted at c8 is reported at **c8**, not c0 |
| Build is reproducible | byte-identical over two builds, `sha256 3249a205f8db…` `[verified-numerically 2026-09-14, n=2]` |
| The thunks still work | the stage-2 self-test re-run against this build and still passes |

**The detector test builds its matrices from the documented formulae**, not from a capture and not
from a transcription of the detector — so the expected answer is known independently of the code
under test. Positives: LH perspective, its transpose, RH perspective, its transpose, a square
(shadow-shaped) frustum. Negatives: identity, orthographic and its transpose, a view matrix and its
transpose, all-zeros.

⭐ Worth noting what the negatives buy. A detector that accepted a **view** matrix or an
**orthographic** HUD projection would have us reading a live log through a broken lens and drawing
confident conclusions from it. Those two cases are the realistic false positives, and both are tested.

## Safety

- **Read-only.** No upload is modified.
- **Both hooks refuse to install if the slot is not owned by the real `d3d9.dll`** — including when
  the owner cannot be determined, since an unbacked pointer is a trampoline. Chaining into a foreign
  hook is what recursed `CreateDevice` 1669 times and killed a launch on the Alan Wake project.
  Standing down loses the instrument, not the game.
- **Both hooks are removed on detach.** The vtables are shared per interface class and the pointers
  we wrote live inside this DLL, so they must come back out before it can be unloaded.
- The previous working build is kept at `tools/proxy-d3d9/d3d9.dll.stage2-working-backup`. Reversal
  is one file copy; full removal is deleting `d3d9.dll` from the game folder.

## What is NOT established

- **Nothing has been run against the game.** Whether Dead Space 2's projection travels through
  `SetVertexShaderConstantF` at all is **unknown**. It is a D3D9 game, so it is likely, but the
  engine could use a fixed-function transform path or push the matrix some other way.
- If the log shows uploads but **no** perspective-shaped window, that is a real finding and not a
  bug — the instrument says so explicitly in that case.
- Which signature is the camera is a **runtime** question. The `ys/xs` heuristic narrows it; it does
  not settle it.
- Nothing about stereo has been derived, and no handedness has been observed — only the reading that
  *will* be printed when one is seen.

## The one launch, and what each outcome means

Launch the game and reach any scene with 3D in it, then quit normally. Read
`Dead Space 2\ds2_proxy.log`.

| What the log shows | What it means |
| --- | --- |
| `PERSPECTIVE-SHAPED 4x4 at cN` with one signature whose `ys/xs` ≈ 1.7778 | ⭐ the camera is found — register, packing and handedness all in that block |
| Several signatures, one square (ratio 1.0) and one at the display aspect | the square ones are shadow passes; the aspect-matching one is the camera |
| Uploads counted but `NOTHING perspective-shaped seen yet` | the projection does not travel this way — a genuine finding, and the next step changes entirely |
| `REFUSING to hook` | something else holds the slot; the game still runs, the instrument stood down |
| No `CreateDevice` line at all | the game did not create a device through our `IDirect3D9` — investigate before anything else |
