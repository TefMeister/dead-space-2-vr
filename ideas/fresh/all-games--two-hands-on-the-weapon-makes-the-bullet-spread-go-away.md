# Two hands on the weapon makes the bullet spread go away

Order: 9001
From: mod-ideas `games/all-games.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/all-games.md>), copied 2026-09-23

`[raw]` · `[looks doable]` — *the spread half is PROVEN on RE Village; the two-hand half and every other game are unchecked*

> "putting two hands on the weapon makes the spread go away … any weapon, even pistols, when two
> handed the weapon bullet spread is 0, when having one hand on the gun, the bullet spread is like
> vanilla game."

Verbatim record: [`inbox/2026-09-21-all-games-two-hands-on-the-weapon-removes-spread.md`](../inbox/2026-09-21-all-games-two-hands-on-the-weapon-removes-spread.md)

A rule about what the player's hands *mean*: steadying a gun with your second hand is what makes it
accurate, the way it is in life — instead of a button. One hand, the game's own spread. Two hands, none.
It replaces "hold the aim button to be accurate", which in VR undoes what your arms just said.

**It is two separate jobs, and only one of them is proven:**

1. **Switching the spread off** — ✅ **done and verified on RE Village** (2026-09-21): the game builds
   each bullet with a deliberately wobbled direction, and that can be replaced with the clean aim at
   the moment the bullet is built. Five hip shots with up to 11.7° of wobble each left dead straight,
   and Tefa confirmed it by eye. Full recipe: `re-village-scope-vr` dossier §9bk / §9bl.
2. **Knowing that two hands are on the gun** — ⚠️ **not built anywhere.** Needs the second controller's
   position relative to the weapon and a "close enough to the fore-grip" test per weapon.

**What it'd take per game.** RE Engine games (RE2, RE3, RE7, RE4) very likely share the same
make-the-bullet steps, so the Village recipe should carry over — ⚠️ **unchecked**: RE2's type list
has a spread setter (`set_Diffusion`) but nobody has traced how RE2 builds a bullet. Other engines
need their own hunt, and the method matters more than the code: *measure the wobble per shot first,
then find the step where the bullet is BUILT, not a step that merely reports it.*

⚠️ **Decided for RE Village for now (see `decisions.md`, 2026-09-21): NO two-hand condition there —
the rifle is simply always accurate.** This idea stays floating for RE2 and everything else.
