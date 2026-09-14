# 2026-09-14 (d) — THE PER-EYE STEREO IS DERIVED, AND IT NEEDS TWO SLOTS, NOT ONE

**`/pd`, dev PC `DESKTOP-V8GTSIR`. The game was not launched and nothing here has been run against
it.** Everything below is arithmetic, checked numerically in a process of our own.
Code: `dev-archive/tools/proxy-d3d9/src/stereo.{c,h}`; test: `test/stereo_selftest.c`
(**59/59** `[verified-numerically 2026-09-14]`); runner: `build-tests.sh`.

Closes the second `[PD]` row of 2026-09-14: *re-derive the per-eye shear from the ACTUAL c4 matrix*.

---

## The answer, in one block

For the measured projection (c4, layout R, row-vector `clip = view * M`), an eye displaced by `e`
along `+view.x` with convergence distance `zc` needs **exactly two elements changed**:

```
m[12] = -xs * e          (3,0) — THE EYE OFFSET   : parallax, falls off with depth
m[8]  =  xs * e / zc     (2,0) — THE CONVERGENCE  : a constant NDC offset

everything else, and the depth triplet m[10]/m[11]/m[14] in particular, is copied UNCHANGED
```

with `xs = m[0] = -1.944444` as measured.

## ⭐ The finding: one slot is not enough, and the test is what proved it

My first derivation used `m[8]` alone and called it "the shear". It is in the note this supersedes
in spirit — the board row itself says *"re-derive the per-eye shear"*, singular — and it is wrong.
The selftest failed it on the first run with a signature that named the fault outright: **disparity
came out constant at every depth.**

Writing the two terms out shows why. After the perspective divide (and `clip.w = view.z` here):

```
ndc.x = xs*view.x/z  +  m[8]  +  m[12]/z
                        ^^^^     ^^^^^^^^
                     constant    falls off with depth
```

- `m[12]` carries `view.w`, so after dividing by `z` it becomes a **1/z** term. That is real
  parallax — near things separate a lot, far things barely — and it is exactly what displacing the
  eye produces: `xs*(view.x - e)/z` differs from mono by `-xs*e/z`.
- `m[8]` carries `view.z`, so after dividing it becomes a **constant**. That slides the whole eye
  image sideways by the same amount at every depth, which sets where the two eyes agree.

**`m[8]` on its own is a flat image shifted sideways. It is not stereo, and on a flat screen it
would not look obviously wrong** — which is precisely the failure this account's numeric-test rule
exists to catch.

## ⭐ What this changes about the plan: stereo does NOT wait on c0

The board's reading was *"head tracking edits c0, stereo shears c4"*, with c4's half assumed to be
the easy one and c0 still `[hypothesis]`. Both stereo terms turn out to live in the **projection**,
so:

**Per-eye separation can be done in c4 alone, with c0 unconfirmed and untouched.**

c0 is still wanted — head **rotation** has nowhere else to go — but it is no longer on the critical
path for getting two different pictures to two eyes. That reorders the cheap work: the c0 logging
row is now about *turning your head*, not about *seeing in stereo*.

## ⚠️ THE SIGN TRAP — the thing most likely to waste a headset session

`xs` is **negative** here (−1.944444; the X axis is mirrored). Work the disparity through:

```
ndc_right - ndc_left = -xs * (e_r - e_l) * (1/z - 1/zc)
```

Correct stereo needs an object **nearer** than convergence to sit further **left** in the right eye.
For `z < zc` the bracket is positive, so that requires `sign(e_r - e_l) == sign(xs)` — and with `xs`
negative, **the right eye takes a NEGATIVE `e`, the opposite of every textbook.**

`stereo_right_eye_sign()` returns this from the matrix rather than hard-coding it, and the selftest
checks both directions: the measured matrix gives −1, an otherwise identical matrix with positive
`xs` gives +1. So it is a rule, not this one matrix memorised.

⚠️ **Getting it backwards swaps the eyes.** That does not crash, does not look broken on a monitor,
and is deeply unpleasant in a headset — reversed depth is one of the classic causes of sickness.

⚠️ **AND THE RULE RESTS ON AN ASSUMPTION I CANNOT SETTLE STATICALLY** `[hypothesis]`. It assumes the
mirror lives in the **projection alone**. If the view matrix at c0 also negates X, the two cancel and
the correct sign flips back. Nothing in a projection matrix can reveal that.

**The check that settles it, and it needs no headset:** the c0 logging row was already queued. When
c0's contents are read, take the **determinant of its upper-left 3×3**. A pure rotation gives **+1**
and the rule above stands as written. **−1** means the view mirrors too, the two cancel, and the sign
flips. ⭐ That turns a vague "watch out for handedness" into one number with two meanings, and it is
the same row that was already going to be done.

## What is NOT established

- ⚠️ **Nothing has been run against Dead Space 2.** This proves the arithmetic; it does not prove the
  game will accept a modified c4, that c4 is the only projection consumer, or that nothing else
  re-uploads it afterwards.
- ⚠️ **The game's own culling and frustum are untouched**, so geometry just outside the mono frustum
  may pop at the edges of the sheared view. Expected, not yet seen, no work done on it.
- **`zc` has no measured value.** 2.5 view-space units is the test's placeholder. The unit scale of
  this game's view space is unknown, so IPD in metres cannot be set until something calibrates it.
- The depth mapping is preserved rather than explained. `m[10] = 0.990097` is still flagged as
  non-textbook in the dossier; this work **carries it over untouched in both eyes**, which closes
  the worry that stereo maths would trip over it, without answering why it is that shape.

## Two housekeeping fixes made in passing

- ⚠️⚠️ **THE RECON EVIDENCE WAS NEVER IN GIT, AND THE CAUSE WAS `.gitignore`, NOT FORGETFULNESS.**
  The board and note (c) both cite
  `dev-archive/recon/2026-09-14-camera-found/ds2_proxy-camera-found.log` as the proof of the whole
  camera finding; `git ls-tree` showed it had never been committed. The reason is the repo's own
  `.gitignore`, which carried a blanket **`*.log`** under "local scratch".

  **This is a silent, repeating failure, not an incident.** `git add` on an ignored file succeeds
  with no error and no output, so every session that thought it was committing evidence had no
  signal that it wasn't. It had already taken **two** logs — the camera one and
  `2026-09-14-proxy-crash-ab-test/ds2_proxy-stage3-standdown.log` — and would have taken every
  capture this project ever makes.

  **Fixed at the cause:** `!dev-archive/recon/**/*.log` un-ignores evidence while leaving stray logs
  in the game folder and `build/` ignored. Both directions checked with `git check-ignore`. Both
  rescued logs are now committed (20 KB and 5 KB, our own output, no game content). The camera log
  independently carries the matrix this derivation used —
  `signature xs=-1.944444 ys=3.456790  seen at: c4` — so the test's ground truth is the log, not a
  re-typing of the note.

  ⚠️ **Worth checking on the other projects:** this `.gitignore` was very likely copied between game
  repos, so the same blanket `*.log` may be quietly eating evidence elsewhere. Not checked here —
  that is another lane's repos and another session's job.
- **`build-tests.sh` added.** The four selftests each had to be rebuilt from a one-off command line
  that survived only in a session transcript, so "the suite still passes" was unverifiable the moment
  the session ended. One script now builds and runs all four: **4/4 pass**
  `[verified-numerically 2026-09-14]`.
