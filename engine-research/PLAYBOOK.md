# The Unattempted-Engine VR Playbook

The full playbook — the reusable, engine-agnostic, phase-by-phase method for taking a game whose
engine nobody has ever brought into VR and getting it into a headset — lives canonically in the
[flat-to-vr-RE-toolkit](https://github.com/TefMeister/flat-to-vr-RE-toolkit) repo:

**Read it here: [PLAYBOOK.md (canonical copy)](https://github.com/TefMeister/flat-to-vr-RE-toolkit/blob/main/PLAYBOOK.md)**

Until 2026-08-26, every `-engine-research` repo carried its own full copy of the playbook. The
copies were still byte-identical, but they were guaranteed to drift apart the first time any one
of them was edited, so they were all replaced with this pointer — an improvement now lands in one
place instead of sixteen.

The North Star is unchanged: **the game renders in a headset and the view tracks the player's
head.** Everything else in a VR mod is built on top of that.

Everything specific to this game lives next door in [`ENGINE-DOSSIER.md`](ENGINE-DOSSIER.md).
