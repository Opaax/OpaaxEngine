# Lessons

## L1 — Place systems by Gregory's layer, not by a "services vs subsystems" gut call (2026-06-30)

**Mistake:** I suggested `IAssetSystem` as an app-level *service*. The Resource/Asset Manager
sits *above* Core Systems in Gregory's runtime-engine diagram (Fig 1.16) — it's pure **engine**.
The app layer must stay ignorant of game concepts (textures, scenes).

**Rule for next time:**
- The App/Engine split is the horizontal Gregory line: **App = Platform-Independence + Core-Systems
  layers** (passive facilities — Platform, Paths, Logger, Config, ProjectManager, JobSystem, Window).
  **Engine = Resources/Assets and everything above.**
- Before proposing where a system lives, name its Gregory layer. Resources-and-up => engine. If it
  ticks per frame => engine. If it's a passive facility you submit-to/query => app service.
- Init-order invariants (user, locked): **Config is loaded from disk in `Bootstrap()` before the
  Engine exists**; **JobSystem is provided after Config** (worker count is config-driven) and stays
  an app service. IEngine is created in the Initialize phase (post-window), not in Bootstrap.
- Architecture is already specced in `.claude/Old/milestone/Engine_Refresh_Program.md` (4 instance-owned
  tiers by lifetime). Read it before re-deriving the model from scratch.

## L2 — Optimize the invariant the USER stated, not the one I prefer (2026-06-30)

**Mistake:** I argued IEngine should NOT be a service in the locator, on a "services are passive,
they don't tick" purity rule. The user's actual goal is **one single static root** (`AppServiceLocator`),
with the whole graph instance-owned beneath it — which kills the DLL-static hazard by construction.
Engine-as-service serves that invariant; my purity rule was the weaker concern.

**Rule for next time:** when the user states the invariant they're optimizing (here: "AppServiceLocator
the only static thing"), evaluate proposals against THAT, not against a textbook taxonomy. A
"violation" of a clean category (a ticking service) is fine when it buys the invariant that actually
matters for this codebase. Concede fast when their reason is stronger.

## L3 — When a plan's PREMISE balloons mid-build, STOP and re-surface a scoped fork (2026-07-03)

**What happened:** the approved M-RES-2 plan assumed "keep Resolve lock-free." Implementing it, the
chosen async model forced that premise into a two-tier mutex + atomics + pre-reserved registry +
destroy-outside-lock — roughly 3x the concurrency code, against the project's #1 value (Simple:
"understandable end-to-end by one person"). I paused and asked (AskUserQuestion) coarse-uncontended-lock
vs strictly-lock-free, recommending the simpler one; the user picked simple and it collapsed the machinery
to a single recursive mutex.

**Rule for next time:**
- Distinguish a plan DETAIL that drifts (adjust inline) from a plan PREMISE that turns out costlier than
  sketched (STOP and re-surface). The latter is the CLAUDE.md "deviate → STOP and re-plan" trigger.
- Before committing hundreds of lines to a premise, estimate its true cost. If it fights the user's stated
  top value (here Simple > max-concurrency), present the cheaper alternative as a scoped fork with a
  recommendation — don't silently build the expensive version they haven't seen.
- "Uncontended lock ≈ lock-free in steady state" for a solo 2D engine: don't pay large complexity for a
  concurrency guarantee the workload won't measure.

## L4 — Old+new coexistence: give the NEW system collision-proof identities; don't rely on include order (2026-07-13)

**What happened:** building the refresh Event system beside the retired old one, I hit `C2365 redefinition`
twice. Cause: the old `EEventCategoryOld` enum kept UNSCOPED enumerators named `EEventCategory_*` (the TYPE got
the `Old` suffix, the enumerators didn't), and my new unscoped `EEventCategory` reused those exact names. No clash
until ONE translation unit saw both headers. My first fix — forward-declare `Event` in `Window.h` — stopped the
leak into TUs that only needed a reference, but the clash returned in `OpaaxApplication.cpp`, which LEGITIMATELY
needs both (new event headers to route + the old event system transitively via `Engine.h`→subsystems). Include
tricks can't save a TU that genuinely needs both. Real fix: scope the NEW `EEventCategory` as `enum class` + a
`constexpr operator|` — enumerators become `EEventCategory::X`, never injected into the namespace.

**Rule for next time:**
- When old and new systems must COEXIST, give the NEW one collision-proof identities up front — **scoped
  `enum class`** or distinct names — never rely on include order / hiding. Two unscoped enums sharing enumerator
  names WILL collide the moment one TU needs both.
- A rename to `*Old` must carry through to enumerators/consumers, not just the type name. An incomplete rename
  (type suffixed, enumerators bare) is a latent collision waiting for the first shared TU.
- Still forward-declare a type in headers that name it only by-reference/pointer (`Window.h` → `class Event;`) —
  good hygiene that shrinks the blast radius, even though it isn't the root fix.
- Cross-module type identity: hash a compiler-stable per-type string (`__FUNCSIG__`), NOT a function-local
  template-static counter (duplicates per DLL/exe — the standing DLL-static hazard). Same rule that makes
  out-of-line `.cpp` statics DLL-safe.

## L5 — First build after your change is red? Separate YOUR diff from a pre-broken branch, and respect WIP boundaries (2026-07-16)

**What happened:** after my World→WorldOld rename, `./build.bat` failed with 5 errors about `IEngine::GetEventBus`
— in `OpaaxApplication.cpp` / `RendererManager.cpp` / `IEngine.cpp`, files my rename never touched. The branch HEAD
simply didn't compile before I started (an unfinished `GetEngineEventBus`→`GetEventBus` migration the user was
mid-way through). I proved my blast radius with `git status` (every modified file was a World consumer I swept),
completed the minimal migration to unblock, and **told the user I'd touched their in-flight work**. The user reverted
my version and fixed it their way, then said "don't touch the EventBus subsystem." Later a runtime restart-loop bug
lived right next to that same wiring; I fixed it in **Engine boot only** and left the EventBus class alone.

**Rule for next time:**
- A red build right after your change is NOT proof your change broke it. `git status` shows your true scope; the
  compiler's error *file paths* tell you whose code it is. If the errors are in files you never edited, it's
  pre-existing — say so explicitly, don't silently absorb blame or thrash trying to "fix" your own clean diff.
- Unblock with the MINIMAL fix and name it as touching the user's WIP. When they've fenced off a subsystem
  ("don't touch X"), fix adjacent bugs on your side of the fence (Engine boot / accessors), never inside X.
- The refresh branch can be committed-but-non-compiling (WIP commits). Don't assume HEAD builds; verify first.

## L6 — Diagnose per-frame lifecycle loops by the log's repeating unit; a subsystem needing a sibling during Startup = resolve-from-manager, never lazy-Startup (2026-07-16)

**What happened:** the render stack + ResourceManager re-initialised every ~25ms. Reading the log's repeating unit
across cycle boundaries (not guessing) showed the whole `Engine::StartupAll` re-running. Root cause: `RendererManager::Startup`
reached the bus via `IEngine::GetEngineEventBus()`, whose safety-net `if(!m_bStarted) Startup()` re-entered `Startup`
while `m_bStarted` was still false (it's set only AFTER `StartupAll`), so `StartupAll` ran again — factories not yet
consumed — rebuilding every subsystem. Fix: the accessors resolve their pointer from the owned `m_Subsystems` FIRST
(the manager's create pass populates it before any subsystem's Startup), self-`Startup()` only as a genuine last resort.

**Rule for next time:**
- To diagnose a per-frame loop, find the exact repeating log sequence and its ONE trigger before proposing a fix;
  shifting the cycle boundary often reveals the real order (here: Resource.Startup → Renderer.Startup rebuild).
- A subsystem that needs a SIBLING during its own `Startup` must reach it through the owning manager
  (`m_Subsystems.GetSubsystem<T>()`), never via a lazy accessor that can re-enter the owner's boot. Set a flag/cache
  AFTER creation but expose it via resolve-from-manager so it's reachable mid-boot. Initialise all cached ptrs `= nullptr`.
- See memory `project_engine_boot_reentrancy` for the accessor shape.

## L7 — When cleanup has no good answer inside the existing phases, the missing thing is a PHASE (2026-07-16)

**Distilled from the world-events task (task lesson L-E), because it is now doctrine, not an anecdote.**

`WorldManager::Shutdown` destroyed worlds without announcing them. I proposed three patches — broadcast
during destruction (subscribers half-dead), re-enter `DestroyWorld` from `Shutdown` (same problem), or
document the hole. All three were bad. The user's answer was a new **TearDown phase**, which dissolved
the problem instead of patching it.

**The insight:** the real axis is not "teardown vs shutdown", it is **"everything still alive" vs
"things are dying"**. No reordering *inside* `Shutdown` could ever have worked — by then Engine has
unbound and the bus is going down.

**Rule for next time:**
- When a cleanup problem has no good answer inside the existing phases, check whether the lifecycle is
  missing a "stopped but still alive" step BEFORE writing defensive code into a destructor path.
- This is now one doctrine at every scope: `ISubsystem::TearDown` / `ISubsystemManager::TearDownAll` /
  `IEngine::TearDown` / `OpaaxApplication::EngineTeardown`. Apply it one scope down without re-deriving
  it — world subsystems broadcast `WorldDestroying` BEFORE deinit, `WorldCreated` AFTER init
  (Editor.md §3, lock L3). Two-phase events are the same rule wearing a different hat.
- **Corollary:** `Flush()` only runs inside `Loop()`, which has already stopped by teardown — an
  *enqueued* teardown event is never delivered at all. Bridge teardown-time events with `Publish`,
  never `Enqueue`.

## L8 — `./build.bat` exit code is unreliable; verify by output, and dead-but-compiled code links against symbols (2026-07-17)

Two operational rules from Editor M0.

**`./build.bat` lies about success.** It uses `goto end` on failure, so it exits **0 even when the build
FAILED**. A background build reported "completed (exit code 0)" while actually erroring. **Always grep the
output** for `Build complete` vs `error C\d`/`error LNK`/`Build FAILED` — never trust the exit code. Also:
run it directly (`./build.bat <preset> </dev/null`), not via `cmd //c` (not found). And the Visual Studio
generator is multi-config: presets share build dirs; a fresh preset build recompiles all vendors (~5 min
in Release) with **fully buffered** output (the file stays 0 lines until done) — wait for the completion
notification, don't thrash polling.

**Dead-but-compiled code still links against symbols.** "Just drop the `Editor/*` glob" (S0) broke the
link because dead `CoreEngineApp`/`SceneManager`/`ComponentRegistry` — still compiled — reference editor
symbols under `#if OPAAX_WITH_EDITOR`. Removing the symbol without compiling-out its dead consumers =
unresolved externals. **Rule:** before an edit framed as "just remove X," grep who references X *under the
same flag/condition*; the fix is usually to flip the CONDITION (here: engine DLL always
`OPAAX_WITH_EDITOR=0`, which `release` already proved), not to hunt every consumer. When you flip a
load-bearing flag, check what ELSE it gated (here `IPaths`' workspace-dir branch) and decouple it.
