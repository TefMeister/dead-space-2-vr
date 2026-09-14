# camhunt improvement: let the instrument mark the camera itself

**From:** the modding lane, working `prototype-vr`, 2026-09-14 evening.
**Why an inbox drop rather than an edit:** `dead-space-2-vr` was `FRESH`-claimed by a `/pd` session at
the time, so its files were left alone. This is the create-only hand-off.

## The change, in one line

`camhunt.c` now **computes the display aspect ratio itself** and marks any perspective signature whose
`|ys/xs|` matches it, instead of printing a line of prose telling a human to do the division.

## Why Prototype earned it and Dead Space 2 did not

Dead Space 2's log produced **three** candidate signatures and the right one was obvious by eye.

Prototype produced **dozens** of junk matches — blocks that merely happen to carry a ±1 in the w slot:
`[c9 … 17.2844]`, `[c25 … −0.5589]`, `[c8 392.918060 29.281885 … 0.0745]`, and a `c1` whose ratio
wandered between 2.8 and 18.9 from one period to the next.

⭐ **The only thing that separated the camera from all of it was that its `ys/xs` equalled the display
aspect (1.7778) in every period while the noise wandered.** That test existed already — as a sentence
in the log asking the reader to divide. On a noisy engine that sentence is the difference between an
answer and a list, so the instrument now does the division.

**The numbers were always in hand:** the wrapper already receives `pp->BackBufferWidth/Height` in
`CreateDevice` and was throwing them away after logging them.

## What it looks like

```
CAMHUNT: display is 1920x1080, aspect 1.7778 - signatures whose |ys/xs| matches that will be marked.
PERSPECTIVE-SHAPED 4x4 at c0, layout R (70903 uploads):   <<< MATCHES DISPLAY ASPECT - very likely the camera
LIVE perspective signatures this period (...): [c0 R 1.191754 2.118673 1.7778 n=66 <<<CAMERA?] [c9 R ... 17.2844 n=450]
```

## Design decisions worth keeping

- ⚠️ **It MARKS, it never FILTERS.** A square (1.0) frustum is a legitimate shadow pass and worth
  seeing, and an engine rendering at a non-display aspect would be hidden entirely by a filter.
- **The comparison is on `|ys/xs|`**, absolute. ⭐ **This matters for Dead Space 2 specifically:** its
  camera has a *negative* `xs` (mirrored X), so its ratio is −1.7778. A signed comparison would fail to
  mark the very camera that project just found. There is a self-test case for exactly that shape.
- Tolerance is 1% of the display aspect.
- If the dimensions are unavailable it says so and falls back to the old prose line, rather than
  silently marking nothing.

## Files

Three small edits, all in `dev-archive/tools/proxy-d3d9/`:

| File | Change |
| --- | --- |
| `src/camhunt.h` | declare `camhunt_set_display(width, height)` |
| `src/camhunt.c` | store the aspect, add `matches_display_aspect()`, mark in both the announcement and the LIVE line |
| `src/wrap_d3d9.c` | one line in `W_CreateDevice`: pass `pp->BackBufferWidth/Height` over |
| `test/camhunt_selftest.c` | three new cases — a 16:9 camera marked, a square frustum not marked, a **mirrored-X** 16:9 camera marked anyway |

Copy them from `prototype-vr/dev-archive/tools/proxy-d3d9/`, where they are
`[compile-verified 2026-09-14]` with the full suite passing (detector 15/15, thunks, wrapper 16/16)
and the build hash-reproducible.

⚠️ **Not verified against a running Dead Space 2.** It is a logging change with no effect on the
hooking path, but that is a reason to expect it to be safe, not evidence that it is.
