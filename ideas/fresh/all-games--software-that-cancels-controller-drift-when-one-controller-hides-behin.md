# Software that cancels controller drift when one controller hides behind the other

Order: 9002
From: mod-ideas `games/all-games.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/all-games.md>), copied 2026-09-23

`[raw]` · `[looks doable]` — *a first version is built for RE Village and NOT yet worn; nothing proven*

> "if there is a way to make some sort of a software that gets rid of motion controller drift when
> one controller is behind the other one, even if it stops the gun front part from moving, that
> would be amazing. right now it is sometimes impossible to aim, because the weapon just starts
> moving by itself." — 2026-09-21

When a two-handed weapon is held, one controller ends up behind the other, the headset's cameras
lose sight of it, and the headset has to **guess** where it is from its motion sensors. The guess
slides, and because the front of the gun follows that hand, the gun slides with it.

The idea: notice when the hand's position has stopped being *seen* and started being *guessed*, and
while that lasts **hold the front of the gun still** — then carry on smoothly from where it was
held once the hand is seen again. Tefa's own words set the bar: holding the front still is an
acceptable price.

**What it'd take:** the headset software already tells a game, every frame, whether a controller's
position is really tracked or only estimated. Most VR mods never read that. A mod that steers a gun
from the second hand can read it and hold the steering while it says "estimated". Where a headset
or streaming app does not report it honestly, the fallback is judging it from the movement itself
(a hand that slides steadily while its wrist does not turn) — harder, and easier to get wrong.

⚠️ **Unchecked where it matters:** whether Quest 3 through Virtual Desktop ever reports "estimated"
honestly is **not known** — the first RE Village build measures exactly that. The known cure that
needs no software is the grip itself: **left controller ABOVE the right, never behind it**.

Verbatim record: [`inbox/2026-09-21e-all-games-cancel-controller-drift-when-occluded.md`](../inbox/2026-09-21e-all-games-cancel-controller-drift-when-occluded.md)
· Live work: `re-village-scope-vr` dossier §9ch.

---
