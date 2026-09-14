# 2026-09-14 (c) — 🏆 THE CAMERA IS FOUND. Register c4, left-handed, 16:9, near 0.1.

**Tefa played; Claude read the log.** `[verified-live 2026-09-14, n=1 session, ~1.2M constant uploads]`
Evidence: `dev-archive/recon/2026-09-14-camera-found/ds2_proxy-camera-found.log`.

## The answer

```
c4, layout R (register i = row i), uploaded as a 4-register write "c4+4"

    [ -1.944444    0.000000    0.000000    0.000000 ]
    [  0.000000    3.456790    0.000000    0.000000 ]
    [  0.000000    0.000000    0.990097    1.000000 ]
    [  0.000000    0.000000   -0.099010    0.000000 ]
```

| Property | Value | How it is known |
| --- | --- | --- |
| Register | **c4**, four registers wide | the histogram shows a dedicated `c4+4` write, 12,890 in one 5 s period |
| Packing | **layout R** — register *i* is row *i* | the w-from-z term sits at index 11 |
| Handedness | **LEFT-handed**, `clip.w = +view.z` | `m[11] = +1.000000`, in all 24 announcements without exception |
| Aspect | **exactly −1.7778 = −16/9** | `ys/xs`, matching the 1280×720 window, in every single period |
| X axis | **mirrored** — `xs` is negative | consistent across every sighting |
| Near plane | **0.1000** | `−m[14]/m[10] = 0.099010/0.990097 = 0.100000` |
| Field of view | **54.43° horizontal** at rest | `2·atan(1/1.944444)` |

⭐ **It is a PURE PROJECTION, not a view-projection.** The upper-left 2×2 is exactly diagonal with
zero off-diagonal terms. A combined view-projection would carry the camera's rotation there. So the
view/world transform lives somewhere else — which matters enormously for what we do next.

## How we know c4 is the camera and not one of the others

Three projections were seen. The discriminator was not a guess:

⭐ **The camera is the one that ANIMATES.** At 15:44:35–36 the log caught a smooth field-of-view
sweep — 21 consecutive announcements, **every one of them at c4**, walking from 60.00° to 70.00°
horizontal in even steps. c0 and c18 did not move. A projection that changes while the player is
playing is the live camera; a fixed one is not.

That the sweep's endpoints are **60.00° and 70.00° to two decimal places** is itself corroboration:
those are round numbers a designer types, not artefacts of a misread matrix.

| Register | xs / ys | h-FOV | What it is |
| --- | --- | --- | --- |
| **c4** | −1.944444 / 3.456790 | **54.43°** | ⭐ the live camera, and the only one that animates |
| c4 (swept) | −1.428148 / 2.538930 | 70.00° | the same camera, mid-transition |
| c0 | −1.732051 / 3.079201 | 60.00° | announced once during load, then absent from gameplay |
| c18 | — | — | **a false positive**, see below |

## What c0 is probably doing — and why that is a `[hypothesis]`

`c0+4` is written **36,987 times in five seconds** — far more than c4 — yet after the load it never
again announces a perspective. A block written per-draw that is not projection-shaped is almost
certainly the **per-object world or world-view matrix**, with the projection held separately at c4.
That is a textbook D3D9 layout and it fits every number here.

⚠️ **It is inference, not measurement.** Nothing has read c0's contents during gameplay; the argument
is upload frequency plus the absence of a perspective shape. **The cheap confirmation:** log c0's
actual contents for a few frames and check they change as the player turns. That is a `[PD]` job, no
launch needed to write.

If it holds, it is very good news: **head tracking would modify c0 (the view), and per-eye stereo
would shear c4 (the projection)** — two separate, independently testable changes rather than one
tangled edit.

## ⚠️ The depth term does not match the textbook formula

`m[10] = 0.990097` with `m[11] = +1`. A standard left-handed D3D perspective has
`m[10] = zf/(zf−zn)`, which is **greater than 1** for any sane far plane. Solving for `zf` here gives
a negative number, so this is **not** the standard form.

What the matrix actually does: `z_ndc = 0.990097 − 0.099010/z_view`. Near (0.1) maps to 0 exactly, and
z → ∞ approaches 0.990097 — so **depth never reaches 1.0**.

⚠️ **This is flagged rather than explained.** The near plane is solid; the far behaviour is not the
usual one and nothing here establishes why. **Any stereo maths that depends on the depth mapping must
re-derive from this matrix rather than assuming the standard form.** The horizontal shear a per-eye
offset needs does not touch the depth terms, so it is probably unaffected — but "probably" is not a
derivation. `[hypothesis]`

## A false positive, and the fix it earned

The detector matched a block at **c18** that is plainly not a projection:

```
[ 0.005  0      -0.05   200 ]
[ 0      0     128        0 ]
[ 1      1       1        1 ]
[ 0      0       1        0 ]
```

It passed because its w terms happened to line up. What gives it away is the diagonal: **`ys` is
zero**, and a projection with zero vertical scale would collapse the image to a line.

**Fixed:** both diagonal scale terms must now be non-zero and within a sane range. The exact c18 block
is kept **verbatim** as a regression test — a test written from a real false positive is worth more
than an invented one.

⭐ **And the test immediately caught my own over-correction.** The first floor was `0.005f`, which
rejected a 179° field: `xs` is `cot(fovY/2)` divided *again* by the aspect ratio, so a 179° field at
16:9 gives `xs = 0.0049`. My own comment had quoted the `ys` figure and forgotten the aspect divide.
Floor corrected to `0.001f`; **15/15 checks now pass** `[verified-numerically 2026-09-14]`. This is
exactly why the wide-field and narrow-field cases are in the suite.

## The saturation warning earned its keep on its first outing

```
distinct-signature table: 24/24 slots used, 863 dropped  <-- SATURATED
```

The FOV sweep alone created dozens of distinct signatures and filled the table. **Under the old Alan
Wake design that is precisely where the settled gameplay camera would have been silently lost** — and
the reason lesson ⭐3 was carried over rather than simplified away. The LIVE line carried the true
current value throughout, and the warning said plainly not to trust the one-shot announcements.

## What is NOT established

- **One session, one player, one scene.** `n=1`. The register could differ in another level, a
  cutscene, or a different graphics preset.
- **c0's identity is unconfirmed** (above).
- **Why the depth term is non-standard** (above).
- **Nothing has been written.** The instrument is read-only. No stereo has been derived, no shear
  attempted, and whether editing c4 actually moves the picture is untested — the sibling project
  found an engine that re-uploaded its constants and ignored the edit, so that question is real.
- The device was created **PUREDEVICE** (`BehaviorFlags=0x54`), so D3D9 will refuse `Get*` on shader
  constants. Any future read-back instrument cannot work here and must be designed differently.

## The next step, and it needs no game

Log c0's contents for a few frames and see whether they change as the player turns. If they do, c0 is
the view matrix and the two halves of the job are cleanly separated.
