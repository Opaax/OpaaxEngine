# ④ STATS & DEBUG — record (closed 2026-08-31, user-verified)

> `.claude/plans/engine-sequence.md` §④. Contract: **ST1–ST8**, amending **F1** and **I4**.
> Lessons **L58**–**L61**. Six commits.

## What shipped

| Commit | |
|---|---|
| `19ecea6` | **Frame stats as an app service** — named scopes, opt-in, off in ship |
| `d593432` | **Renderer counters** as named stats |
| `8a4f126` | **GPU frame time** — the device times itself and never waits |
| `d907821` | VSync probe removed (a user experiment; **X5** — a setting with no reader is deleted) |

Side task, same session: `0124ecd` delete `QuadBoundsSubsystem` · `a0c3dad` the selection outline as
ONE hollow quad (**F4d**) · `607bb21` a negative scale no longer costs an entity its bounds.

**Gates at close:** 3 presets `OPAAX_BUILD_OK` · tests **446 → 486 / 7214** · editor logs
`6 scope(s), 3 counter(s)` and `GPU timing first reading — 0.029 ms` · the **shipped** `Sandbox.exe`
runs clean with stats off and reports 7 scopes with `Stats.EnableInShipBuild` on.

## The shape, and the three corrections that produced it

**It took three user corrections on one axis, each because I did not go far enough.** The record is
worth keeping in that order:

1. *"i do not realized that it was so much 'integrated' in core"* + *"input manager is write down on
   render slice even no input is in render"*. I had wired `ISubsystemManager` to wrap every subsystem
   in all three tick loops, which put a pure-virtual `GetStatName()` into **Core**'s `ISubsystem` AND
   emitted a row per subsystem per phase — including empty overrides. **One mistake, two symptoms.**
   Fixed by DELETION: Core knows nothing; measuring is opt-in at the site the author picks. → **ST3**,
   [[L58]]
2. *"still, engine have FrameStats"*. `Engine` still owned the snapshot. → it became an **app
   service** (**I4**'s own test says passive-facility, and **IN2** had already ruled that the HOST
   owns the frame boundary). → **ST2**
3. *"Can it be app service? what is the cost of StatsServices::Null on ship game?"* — an instruction
   to evaluate, not a choice. Pricing the null path (**one predicted branch**) retired the
   `OPAAX_STATS` compile flag I had just built, because it forbade the shipped-game profiling they
   wanted. → **ST6**, [[L60]]

**The end state:** `OPAAX_STAT_SCOPE(profiler, "Name")` — Unreal's `SCOPE_CYCLE_COUNTER` adapted to
**I1** (those engines reach a global stat manager; here a scope takes its profiler by pointer, from
`IStatsService::GetProfiler()` or `WorldContext::Profiler`). Not providing the service **is** the off
switch (**I3**); `Stats.EnableInShipBuild` turns it on in a shipped game with no rebuild.

## Defects worth carrying forward

- **115 scopes in the first frame.** `MAX_FRAME_DELTA` clamps a long first delta to 15 fixed steps,
  so every scope inside a `FixedUpdate` body ran 15 times. Fixed by merging a re-entered scope into
  one row with a `Calls` count — Unreal's stat-row shape. **Caught by a smoke run only because the
  one-shot log printed a COUNT.** → **ST1**
- ***"visually its very glitchy"*** — the measurement was right and the DISPLAY was wrong, three
  ways: text churning at 60 Hz, the row SET changing (a frame with no fixed step has no `FixedUpdate`
  children), and a continuously-rescaling graph ceiling. → **ST5**, [[L59]]
- **A negative scale broke picking, not just the outline.** `Bounds2D::HalfExtent` went negative and
  `Contains` is `fabs(...) <= HalfExtent` — false for every point. The reported symptom was the only
  visible one. → [[L61]]
- **My own new test caught my own bug**: `MakeOutlineInnerHalf` clamped only the lower bound, so a
  negative thickness produced an inside-out hole that discards the whole quad.

## Named, not built

- **A runtime FPS/stats overlay for `Game.exe`** — the data is already there (`IStatsService` is
  engine-side and works with no editor), but drawing it needs ⑥'s text rendering.
- **A per-PASS GPU timer** on `ICommandBuffer`. ④'s sequence entry named it; it has no caller, and
  the frame bracket covered the question ([[L23]]).
- **`EditWorldSystems()` has no caller** since `QuadBoundsSubsystem` was deleted — the editor-side
  world-subsystem route is unproven (the runtime one still is, via `QuadOscillatorSubsystem`).
  Re-adding one is a single `Register` call.
