# Bullet casings stay on the floor until the level ends

Order: 9003
From: mod-ideas `games/all-games.md` (<https://github.com/TefMeister/mod-ideas/blob/main/games/all-games.md>), copied 2026-09-23

`[raw]` · `[not judged]` — *nothing checked against any engine*

> "bullet casings stay on the floor until the level end, no idea if this is doable or if this tanks
> the performance, but a very cool idea."

Verbatim record: [`inbox/2026-09-15f-all-games-casings-stay-on-floor.md`](../inbox/2026-09-15f-all-games-casings-stay-on-floor.md)

Right now, ejected shell casings presumably despawn after a short time (the usual reason: keeping the
count of physics-simulated objects small). This asks for them to persist for the whole level instead
— a trail of where you've been fighting.

⚠️ **Tefa named the risk in the idea itself:** every casing kept alive is one more object the engine
has to simulate and render, and a long, casing-heavy level could genuinely add up. Whether that's
negligible or a real framerate cost depends entirely on the engine and how casings are implemented
per-game — likely cheap if casings are simple non-colliding decorative meshes past their first
bounce, expensive if they stay full rigid bodies forever. **Nothing checked yet on any of the games
in this repo.**

---

_Other categories appear as they arrive. Nothing is missing; they just haven't been needed yet._
