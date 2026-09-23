# Let the game's own animations take over the hands, then hand control back

Order: 9000
From: mod-ideas `games/all-games.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/all-games.md>), copied 2026-09-23

`[raw]` · `[not judged]` — *nothing checked against any engine*

> "play melee animation when swinging a weapon. i don't like actually swinging my arms around in VR,
> so if we can figure out a way to give the game back the original animations when a button is
> pressed - swinging weapons, pulling levers, pushing something, hand animation when jumping,
> picking something up, opening a door, getting hit and putting hands in front to guard, getting
> tackled by the enemy - such things i would like to experiment if the game can take control of hand
> movement somehow and once the action is finished give the hand control back to the player. both in
> and out of the "game takes over hand movement" has to be smooth and not teleport/snap player hands"

Verbatim record: [`inbox/2026-09-19-all-games-canned-animations-take-over-hands.md`](../inbox/2026-09-19-all-games-canned-animations-take-over-hands.md)

This inverts the usual VR-port assumption. Normally the motion controllers drive the arms and the
game's own animations are thrown away. This asks for a button press to hand the arms **back** to the
game for the length of one animation — a melee swing, a lever pull, a door open, a guard, a grab —
and then return them to the player.

**The hard part is named in the idea itself, and it is the right hard part:** the two handovers. Any
port already has a pose the player is holding and a pose the animation starts from, and those will
not match. Snapping between them is exactly what Tefa says must not happen, so each direction needs a
short blend rather than a switch — and a blend that survives the player moving their real hands
mid-animation.

**What it'd take:** per game, a way to (a) trigger an existing animation on demand, (b) suppress
whatever normally overwrites the hand bones from the controllers while it plays, (c) know when it
finished, and (d) cross-fade both ways. Games that kept their original first-person animation set are
the promising ones; anything where the arms were deleted for the VR port has nothing to play.

⚠️ **Not checked on any engine in this repo.** Whether an animation can be fired independently of the
action it belongs to varies enormously per game, and on some it may not be separable at all.

---
