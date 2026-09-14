# 2026-09-14 (b) — the instrument stood down. The fix, and an end-to-end proof without the game.

**Tefa played to a save point.** The log is the finding. **No game was launched by this session.**

## What the launch said

```
[15:29:04.527] REFUSING to hook IDirect3D9 slot 16: it holds 73794E60, not owned by
               the real d3d9.dll. Standing down - the game still runs, only the
               instrument is lost.
```

⭐ **The guard did exactly its job and the design was still wrong.** Something was already in
CreateDevice's vtable slot before we ever saw it — the real `d3d9.dll` was at `0x738D0000` and the
slot pointed at `0x73794E60`, a different module. Almost certainly the **Steam overlay**, which hooks
D3D9 in every Steam game.

The fatal part is *when* the stand-down happens: **before a device exists.** So the camera instrument
never installed, and no amount of playing would have changed that. Tefa played to a save point for a
log that could not contain the answer.

⚠️ **My own instrumentation was weaker than it should have been.** The refusal message printed the
*address* of the occupying pointer but not the owning module's **name** — even though the code had
already looked the module up. One field, and it would have said "gameoverlayrenderer.dll" instead of
`73794E60`, turning a puzzle into a fact. The sibling guard in `camhunt.c` does print the name; the
one in `proxy.c` did not. Fixed by deleting that code path entirely (below), but the lesson stands:
**a guard that refuses should say who it lost to.**

## The fix: stop competing for a slot we cannot win

**Do not chain into the other hook.** Chaining is how mutual recursion starts: if the overlay
re-applies its hook later it captures *our* pointer as "the original", and the two call each other
forever. The sibling project recursed `CreateDevice` 1669 times and killed a launch that way.

Instead — **we own the `Direct3DCreate9` export, so we hand back our own object.** `wrap_d3d9.c`
returns a COM wrapper implementing all seventeen `IDirect3D9` methods; sixteen forward, and
`CreateDevice` forwards and then installs the camera instrument on the device that comes back.

⭐ **Nothing is written into any shared vtable.** No other hook can be ahead of us and none is
disturbed — the overlay's hook still runs, layered underneath every one of our forwarders.

This is a port of the same fix on `staging/alan-wake-vr/proxy-d3d9`, which met **the identical
stand-down on the identical slot** on 2026-09-08. The reasoning there is preserved verbatim in the
new file's header so it does not have to be rediscovered a third time.

**The device is still reached by vtable patch, deliberately.** `IDirect3DDevice9` has 119 methods and
a hand-written wrapper is a lot of mechanical code in which one wrong slot silently corrupts a call.
There is no evidence that slot is contested — the stand-down happened before a device existed, so it
has never been reached to find out. `camhunt_install()` carries the same guard, so if it *is*
contested we get a clean stand-down of the instrument alone, and a log line naming the owner. That is
when the device earns a wrapper, and not before.

## ⭐ End-to-end proof, with no game

`test/wrap_selftest.c` creates Direct3D twice in our own process — once through our proxy (getting
the wrapper) and once straight from the system DLL (the real object) — then requires identical
answers from both. The real object is independent ground truth, so a wrapper bug cannot hide behind
it.

**16/16 checks pass** `[verified-numerically 2026-09-14]`: `GetAdapterCount`,
`GetAdapterDisplayMode` (hr and contents), `GetAdapterIdentifier` (hr and description),
`GetAdapterModeCount`, `GetDeviceCaps` (hr and shader version), `CheckDeviceType`,
`GetAdapterMonitor`, `QueryInterface` returning *ourselves* rather than the real object, and balanced
`AddRef`/`Release`.

And the harness got a real HAL device, so **the whole chain ran outside the game**:

```
returning OUR IDirect3D9 00DCDD28 wrapping the real 00D389C0...
IDirect3D9::CreateDevice (through OUR wrapper): ... (not a pure device)
  -> hr=0x00000000 device=0315F2C0
CAMHUNT: SetVertexShaderConstantF hooked at device vtable slot 94
PERSPECTIVE-SHAPED 4x4 at c8, layout R
    signature xs=0.974279 ys=1.732051   seen at: c8
    reading: w-from-z = +1 -> clip.w = +view.z, LEFT-handed.
wrapper IDirect3D9 released (real=00D389C0)
CAMHUNT: hook removed. 1 uploads observed, 1 distinct projection signature(s), 0 dropped.
```

⭐ **Every link is now proven except the game itself:** proxy loads → wrapper returned → device
created through us → instrument installs → a projection is detected at the right register with the
right handedness → clean release with the hook removed. The only untested variable left is whether
Dead Space 2 uploads *its* projection through `SetVertexShaderConstantF`.

That is exactly the ambiguity that cost this project an afternoon earlier today, closed properly this
time: a crash or a silence in the game can no longer be blamed on our plumbing, because the plumbing
is tested on its own bench.

## Verified

| Claim | Evidence |
| --- | --- |
| Builds clean | `-Wall -Wextra`, zero warnings `[compile-verified 2026-09-14]` |
| Wrapper vtable is 17 slots, `CreateDevice` at 16 | compile-time negative-array assertions against d3d9.h |
| All seventeen signatures and their order are correct | the vtable is typed as d3d9.h's own `IDirect3D9Vtbl`, so the compiler checks them |
| Forwarding is correct | 16/16 against the real object `[verified-numerically 2026-09-14]` |
| The instrument installs and detects | end-to-end in the harness, log above |
| Detector still correct | 11/11 re-run `[verified-numerically 2026-09-14]` |
| Thunks still correct | stage-2 self-test re-run, still passes |
| Build reproducible | byte-identical over two builds, `sha256 b92aacb56b80…` |

## What is NOT established

- **Nothing has been run against the game since the change.**
- Whether Dead Space 2's projection travels through `SetVertexShaderConstantF` **at all** — still
  unknown, and still the question. If the log counts uploads but finds nothing perspective-shaped,
  that is a real finding and the approach changes.
- That the occupant of slot 16 *is* the Steam overlay. It is a different module at a plausible
  address and the overlay is the usual occupant, but that is inference `[hypothesis]`. It no longer
  matters — we stopped competing for the slot — which is why it was not worth another launch to pin
  down.

## The next launch

Play until any 3D scene is on screen, then quit normally. Read `Dead Space 2\ds2_proxy.log`.

| What it shows | Meaning |
| --- | --- |
| `returning OUR IDirect3D9` then `CreateDevice (through OUR wrapper)` then `CAMHUNT: ... hooked` | the plumbing is live in the game — everything below is real data |
| `PERSPECTIVE-SHAPED 4x4 at cN` with `ys/xs` ≈ 1.7778 | ⭐ the camera, with register, packing and handedness |
| Several signatures, some square (ratio 1.0) | the square ones are shadow passes; the display-aspect one is the camera |
| `NOTHING perspective-shaped seen yet` despite uploads | the projection does not travel this way — a real finding |
| `REFUSING to hook device slot 94` | the device slot is contested too; that is when it earns a wrapper |
