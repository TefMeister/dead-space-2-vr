# Inbox — hand-offs to the research session

This project runs several Claude sessions in parallel (hands-on modding, per-game public
research, a cross-project research sweep), and every file in these repos is curated by exactly
one of them so concurrent sessions never collide. This repo's curated files —
[`INDEX.md`](../INDEX.md) and the topic write-ups — belong to the research session.

So when the modding session reaches a verdict on a lead tracked here ("tried it — it worked" /
"tried it — dead end"), or stumbles on a promising lead of its own, it leaves a short file
**here** instead of editing the index itself, named `YYYY-MM-DD-<mod|sr>-<short-slug>.md`.

The research session drains this folder at the start of every run — updates the lead's status
tag in `INDEX.md`, extends the topic file with the outcome, then deletes the inbox file. If this
folder contains only this README, nothing is waiting.
## ⚠️ Read the whole inbox before draining any of it

Files here are **create-only**: nobody edits or deletes an existing one, not even their own from
an earlier session. That is what keeps concurrent sessions from ever colliding — but it also
means a correction cannot change the file it corrects. It arrives as a **separate, later file**
naming its target in a greppable header line:

```
Supersedes: 2026-08-27-<author>-<slug>.md
```

So before folding anything into the curated files, run:

```
grep -rn "^Supersedes:" inbox/ --exclude=README.md
```

Draining oldest-first without that check writes a claim into the curated docs and only then meets
the correction that withdraws it. A correction may also supersede a curated doc rather than an
inbox file — in that case it names the file and section, and the fix belongs to whoever owns it.

## Tag how well each finding is actually known

Put a confidence tag next to the claim itself — `[verified-live YYYY-MM-DD, n=K]`,
`[measured YYYY-MM-DD]`, `[inferred-static]`, `[reported]`, `[hypothesis]`, or
`[disproved YYYY-MM-DD]`. **`n=1` is not verified** (though a single counter-example is enough to
*disprove* a rule). A finding that arrives untagged is treated as `[hypothesis]`.
Full definitions: `claude-memory/CONVENTIONS.md` → "Claim hygiene".
