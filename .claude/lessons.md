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

## L9 — `git commit` takes the whole INDEX; a pre-populated index sweeps in the user's staged WIP (2026-07-18)

**What happened:** asked to commit only the `.claude/` durable-knowledge docs, I ran `git add .claude/ &&
git commit`. The index was NOT empty — the session-start `git status` first column already showed the
user's staged WIP (`AD` Engine `IAppService` moves, `R`/`RM` Sandbox renames). `git commit` committed
*everything staged*, producing a mixed commit (my docs + their in-flight refactor under a docs-only
message) that ALSO missed my unstaged `.gitignore` change. Recovered with `git reset --soft HEAD~1` then a
**pathspec commit** (`git commit .claude/ .gitignore -m …`), which commits only the named paths and leaves
all other staged entries exactly as they were.

**Rule for next time:**
- Before ANY commit, read the index. `git status --short`: the **first column** is what's staged. If it
  shows anything you didn't put there, a bare `git commit` will include it. This is doubly true here — the
  session-start snapshot already listed staged `AD`/`R`/`RM` entries; I had that info and still missed it.
- To commit a specific slice regardless of index state, use a **pathspec commit** (`git commit <paths>`).
  It ignores other staged paths and leaves them staged — the surgical tool when the user has WIP staged.
- Committing the user's staged WIP is touching their work (see [[L5]]): if it happens, STOP, say so
  explicitly, and offer the `reset --soft` + re-scope fix rather than leaving misleading history.
- Don't forget your OWN related unstaged changes (the `.gitignore` narrowing belonged in the same commit) —
  the "what belongs together" set spans staged and unstaged.
- **Corollary, the INVERSE trap (2026-08-19).** A pathspec commit only accepts paths git already knows, so
  a slice containing NEW files needs `git add` first — and after that `git add`, a bare `git commit` takes
  **only what you staged**, silently dropping every modified file in the slice. That is how the panel-host
  commit landed as 4 new files with none of the 20 edits they depended on: a committed state that did not
  compile, i.e. [[L17]]'s "never commit a state you haven't built" reached by the opposite route. Same
  one-line defence either way — read `git status --short` *after* staging and before committing, and check
  the file COUNT against the slice, not just for foreign entries.

## L10 — Dead-code quarantine: the compiler+linker is the authoritative classifier, NOT grep (2026-07-19)

**What happened:** reorganizing `Engine/Source`, I quarantined the old world to `Legacy/` and dropped it
from the build. For the ambiguous `Assets/` dir I classified files "dead" via a grep of `#include
"Assets/…"` referencers and moved the "unreferenced" ones. The build failed three ways grep couldn't see:
(1) `AssetHandle.hpp` includes `"AssetRefBlock.hpp"` and `IAsset.hpp` includes `"AssetTypeList.h"` —
**file-relative, same-dir** includes my `Assets/`-prefixed grep never matched; (2) `TAssetHandle::Get()`
**links** against `AssetRegistry_TryResolveTyped` (a symbol dep, invisible to any include scan) → LNK2019.
Assets was substantially LIVE (renderer + config wired through it). The user had asserted "Assets is
legacy," but the linker disagreed. Per [[L3]] I stopped and reverted the whole Assets move rather than
whack-a-mole moving live deps back one build at a time.

**Rule for next time:**
- To prove a file dead before quarantine, the authoritative test is **"drop it from the build and converge
  to green"** ([[L8]]), NOT a grep of include referencers. Grep misses (a) file-relative/same-dir includes
  (`"Sibling.h"`, not `"Dir/Sibling.h"`), and (b) **link-time symbol deps** (a live TU calling a free
  function defined in the candidate `.cpp`). Grep is a fast SEED for the converge loop, never the verdict.
- When the user asserts "X is legacy" but the compiler/linker says otherwise, **the code wins** — surface
  the contradiction with the exact dependency evidence (the LNK2019 symbol, the includer) and recommend;
  don't force the move.
- Move the whole reachability cluster together and let build errors *extend* it. If a "dead" set keeps
  pulling live deps back (premise balloons — [[L3]]), STOP and revert rather than fragment the tree.
- Include-path rewrites for folder moves must handle BOTH `"…"` and `<…>` delimiters, and remember that
  `#include "X/…"` also resolves file-relative (same-dir) — so an absolute-prefix rewrite can leave a
  relative include silently resolving to the new location (usually fine, occasionally surprising).
- **Confirmed again 2026-07-27 (M2c):** before inserting `ERenderLayer::Debug` mid-list I grepped every
  consumer to prove no value was persisted — but scoped the grep to `Engine/Source Editor/Source Sandbox`
  and **omitted `Engine/Tests`**. `SortKeyTests` hardcoded `LayerField(UI) == 3` and went red. Same moral:
  grep is a SEED, the build is the verdict. When shifting a value, sweep the *whole* tree — tests hardcode
  ordinals that source never does. (Fixed at the root: assert the field against the enum's own value, so
  growing the list cannot break a test about bit *positions*.)
- **THIRD time, 2026-08-20 (the config restructure):** the "who reads this field" grep covered
  `Engine/Source`, `Editor/Source` and `Sandbox`, and again **omitted `Engine/Tests`** —
  `IWindowManagerTests` sets `lData.WindowTitle` and broke. Three occurrences, one directory, so the fix
  stops being "remember" and becomes mechanical: **a rename sweep greps from the REPO ROOT with
  `Legacy/`/`Vendors/` excluded, never an allow-list of source directories.** Tests are consumers.

## L11 — A vendor library's GLOBAL STATE duplicates across the DLL/exe line, exactly like our own statics (2026-07-20)

**What happened (S10):** the ImGui overlay linked fine but crashed at `ImGui_ImplGlfw_InitForOpenGL`
(`PrevWndProc != nullptr`). Root cause: the engine DLL linked glfw **PUBLIC + static**, so the editor exe
relinked a **second glfw copy** with its own global state. `glfwInit` + the window ran in the DLL's glfw;
ImGui (in the exe) called the **exe's** glfw copy, never initialised → `glfwGetWin32Window` null → null
HWND → assert.

**The insight:** I1/I2 ("one static instance across the DLL line") apply to a **vendor lib's global state**
(glfw's init flag / window list / current-context) just as to our own types. A static third-party lib linked
PUBLIC into a DLL gets a second, uninitialised copy in every consumer exe; the vendor's `REQUIRE_INIT`
guards then fire in the consumer even though the DLL "initialised it."

**Rules:**
- Before wiring a vendor with GLOBAL STATE (glfw, an allocator, a logger registry, a GL loader) across a DLL
  boundary, decide WHERE its single instance lives and make every module share it — never PUBLIC-static into
  a DLL. It's the I1/I2 hazard wearing a vendor's hat.
- Prefer the vendor's OWN dllexport/dllimport switch over reshaping the library: glfw ships `_GLFW_BUILD_DLL`
  (export) / `GLFW_DLL` (import). Export the one instance from the DLL that owns it (glfw PRIVATE + exported;
  consumers import). No vendor files touched, no extra DLL shipped, runtime deploy unchanged. (User steer:
  fix at THIS PROJECT's link/ABI layer — don't rebuild the lib as shared or ship a new DLL.)
- Check each vendor's actual proc-loading path before assuming: `imgui_impl_opengl3` uses its OWN GL loader,
  not glad, so only glfw needed unifying.
- Sibling of [[L4]] (our own cross-module type identity). See ARCHITECTURE.md **I6** for the related
  "never dll-export a class template; stateless value types are header-only" rule (proven by the Angle module).

## L12 — Don't hand off a "human eyeball" verification gate with nothing observable to look at (2026-07-20)

**What happened (S11):** I closed the input-seam step reporting automated gates green and left "interactive
hover/click" as a REMAINING human-eyeball check. The user tried and came back: **"no log appeared to check
that correctly!"** The seam (`RouteInput` → ImGui `WantCapture*`) had **zero logging**, so clicking produced
no observable signal — the "needs your eyes" handoff was a dead end.

**The fix:** add a Trace-level `RouteInput` log (discrete mouse-button/key only — never per mouse-move, no
spam) printing the consume decision + `WantMouse`/`WantKeyboard`, to console AND the file sink. The gate flip
became visible (menu bar → CONSUMED; passthru viewport → passed) and the user confirmed at once.

**Rules:**
- When a verification step depends on the USER observing behavior, the observability must EXIST before the
  handoff. A gate phrased "eyeball that X happens" is worthless if nothing prints/renders X — add the
  instrument as part of the SAME step, don't defer and assume they'll see something.
- Before writing "needs your eyes," ask: *what exactly will they look at, and does it exist yet?* (I even had
  the file-sink path available and still handed off a blind gate.)
- Match the instrument to event frequency: discrete events (button/key) log cleanly; high-frequency ones
  (mouse-move) need gating or they flood. A per-event seam's verification log can legitimately STAY as
  permanent Trace observability (invaluable for the follow-on milestone — here M-Input), not a throwaway probe.

## L13 — A "green baseline" is a PREMISE to verify, not assume; converging a mid-reorg branch is a linker-driven, whole-tree job (2026-07-21)

**What happened (M0.5):** I planned M1 "Viewport" on the recorded baseline (89/354, runtime draws 3 quads).
The user then revealed the branch was mid-reorganization and **did not build** — a prior "move all legacy into
legacy" commit had swept the *good* new-path RHI (`IRHIDevice`/`ICommandBuffer`/`IFramebuffer` + GL/VK backends
+ `BackendFactory`) into `Legacy/` (unlinked) alongside the genuinely-old facade (`IRenderAPI`/`RenderCommand`),
and relocated `OpaaxLog.h`/`Core` headers, leaving live code dangling. The whole M1 premise was false. The task
became a prerequisite **re-baseline**: extract the keepers back out of Legacy and converge to green.

**Rules for next time:**
- **Verify the branch builds before planning on top of it.** Session-start git status + a recorded baseline in
  a doc are NOT proof the tree compiles — a refresh/WIP branch can be committed-but-broken ([[L5]], [[L8]]).
  When about to "build feature X," a fast `build.bat fast` (or a status sanity pass) up front catches a false
  green-baseline premise before it wastes a plan.
- **Classify keepers by the LINKER, not grep** ([[L10]]). The keeper set for "pull the good code out of Legacy"
  extended itself twice via build errors I couldn't have grepped: the Vulkan backend (SDK present → the
  `#if OPAAX_HAS_VULKAN` branch compiled) and `RenderCommand` (an entangled `WaitIdle()` call). Seed from the
  include closure, then drop-and-build until green; let unresolved externals *extend* the set.
- **A file-move reorg's blast radius spans the whole tree, not the moved dir.** Moved headers + a retired macro
  family (`OPAAX_CORE_*`→`OPAAX_ENGINE_LOG`) broke Sandbox, the editor, and the test harness — not just the
  render tree. Scope it with one comprehensive grep of the known old→new paths before whack-a-mole; expect
  test-double drift ([[L5]]'s StubPlatform, here StubPaths) and tests of now-Legacy code that must be
  *quarantined* (they can't be repointed — Legacy isn't linked), matching **X1**.
- **Keep entangled old bits transitionally to reach green, with a `// FIXME`, rather than decoupling mid-converge**
  ([[L3]]): the old `IRenderAPI`/`RenderCommand` facade rode back in as a keeper because the new factories +
  VulkanFramebuffer still lean on it. Decoupling is a dedicated follow-up, not a converge-loop detour.
- **Surface premise/scope forks up front and let the user steer** — before touching a broad refactor, confirm
  ownership ("I execute vs you execute"), and answer floated alternatives honestly (NVRHI: declined — no GL
  backend, AAA/RT tier, fights the project's #1 *Simple* value; a 2D engine's thin GL RHI is the right altitude).

## L14 — A STALE object file masks a committed-broken branch; a prior "green" doesn't survive a recompile trigger (2026-07-22)

**What happened (facade decouple):** I retired the `IRenderAPI`/`RenderCommand` facade + VK backend to `Legacy/`
cleanly — all my RHI/Renderer TUs compiled. But the build then failed in `OpaaxApplication.cpp`, a file I never
touched: `switch (InEvent.GetCategoryFlags())` with `case EEventCategory::X:` labels. Root cause was **not mine** —
`GetCategoryFlags()` returns `Uint16` and `EEventCategory` had migrated to a scoped **bitmask** `enum class`
(an input event is `Input|Keyboard`), so single-value cases can't match *and* won't convert. The branch HEAD
(`a829d87`) did **not** compile; a **stale `.o`** from a prior session's green `build.bat fast` had masked it, and
my header change forced the recompile that surfaced it. Proven pre-existing with `git diff HEAD -- <file>` = empty.
The harness flagged the file "modified on disk" mid-task — the **user was editing it concurrently**. I applied a
consumer-side patch (`OnEvent` → `IsInCategory` bit-tests) to unblock, but the user fixed it their own way and
committed it ("Fix app switch state") **while I worked**; my edit was superseded (working tree re-matched HEAD). My
actual task (the RHI decouple) verified green on the shifted base (3 presets, test 80/339, Sandbox 3 quads clean).

**Rules for next time:**
- **A prior session's green is not this session's truth** ([[L13]] sharpened). `build.bat fast` only recompiles
  *dirty* TUs — a committed-broken file with an up-to-date `.o` reports green until something dirties it. Before
  building on a baseline, trigger a real recompile of the blast area (or a clean build) — don't trust a stale green.
- **When the red is in a file you never edited, prove ownership fast**: `git diff HEAD -- <file>` empty ⇒ the broken
  *source* is HEAD's, not your diff ([[L5]]). Say so explicitly; don't absorb blame or thrash your clean diff.
- **When the harness says a file was "modified on disk," the USER is likely editing it live — defer, don't race.**
  My consumer-side `OnEvent` patch was wasted effort: the user was actively fixing that exact WIP file and committed
  their own version. If pre-existing breakage sits in a file the user is touching, surface it and let them own the
  fix ([[L5]]); only patch it yourself if you must unblock AND they're not in it. HEAD can move under you mid-task
  (they committed 3× while I worked) — re-verify on the shifted base and re-read the index before committing ([[L9]]).
- **A `switch` is the wrong tool for a bitmask enum** — combined flags (`A|B`) match no single-value `case` and fall
  through to `default`. Dispatch bitmask categories by bit-test (`IsInCategory`/`&`), not `switch`.
- **Retiring a vestigial facade is clean when you trace live-vs-dead by the linker+grep** ([[L10]]): the two statics
  (`RenderCommand::s_API`, `RenderAPI::s_Backend`) were consumed only by dead code; extract the still-live bits
  (`EBackend`+string map → static-free `RHIBackend.h`), route everything through the instance (`IRHIDevice`), move
  the cluster to `Legacy/` (unlinked — dangling includes there are fine, **X1**), grep-clean outside `Legacy/`.

## L15 — Absence-of-error is not presence-of-result: log the SUCCESS branch, not just the failures (2026-07-26)

**What happened (Editor M1):** after wiring the ViewportPanel, the run log looked conclusive — "Primary render
target set to offscreen", a resize line, 0 err/warn. But `Draw()` has two branches: a non-zero FBO handle draws
`ImGui::Image` (the world), a zero handle falls back to `ImGui::Dummy` (a blank rectangle) — and the Dummy path
logs **nothing wrong**. A clean log was fully consistent with a blank panel. Fixed with a one-shot Info log on the
*success* branch: `Viewport displaying world FBO (handle=2, WxH)`.

**Rules for next time:**
- [[L12]] said "the observability must exist." This sharpens it: it must **discriminate**. When a gate is
  "no errors + a state-change log," ask *does any log positively assert the thing happened, or only that nothing
  failed?* If a silent-fallback branch exists (Dummy / default / early-return no-op), log the success branch.
  One-shot (`if (!m_bLogged)`) keeps a per-frame path from flooding.
- **A render-to-texture panel has a self-referential sizing loop** (window sizes to content, content sizes to
  window): it collapsed to **32x13 px** — correct pipeline, useless demo. Seed `SetNextWindowSize(..., FirstUseEver)`.
- **Operational:** a still-running GUI exe locks the DLL → `LNK1104: cannot open OpaaxEngine.dll` on the next
  build. Not a code error ([[L5]]/[[L8]] — suspect the environment when the red is in something you didn't
  change). A GLFW window can outlive a bash `kill $PID`; kill by image name (`taskkill //F //IM Sandbox.exe`).
  Smoke pattern that behaves: `./app.exe > log 2>&1 & sleep N; taskkill //F //IM app.exe;` then grep the log.

## L16 — Type-check a plan's compile-level claims before building on them (2026-07-26)

**What happened (Editor M2 planning):** a plan authored by another model (Fable, Plan agent) sequenced a step that
landed real `DrawerRegistry`/`PanelRegistry` storage while leaving the M0 `int` placeholders in
`SandboxEditorModule.cpp`, gated on "the placeholders still compile against the new signatures (they do)." They
don't: `Drawers().Register<int,int>()` instantiates the stored lambda with `TDrawer = int` → `int d; d.Draw(*c);`
→ ill-formed; and a factory lambda returning `int` is not convertible to
`TFunction<UniquePtr<IEditorPanel>(EditorContext&)>`. Caught by reading the proposed template body, before writing
any code. The consequence was structural, not cosmetic: registry + consumer + dogfood are **atomic**, which
invalidated the plan's step split and forced a per-slice milestone decomposition.

**Rules for next time:**
- A plan's prose claims ("this still compiles", "runtime is unchanged") are **assertions to verify**, not
  findings. Template-instantiation claims especially: a template body is only checked when instantiated, so
  "nothing else changed" is not evidence a placeholder call site survives.
- Research done by a sub-agent is worth its cost; its *conclusions* still need this session's review. Verify the
  load-bearing factual claims yourself (here: `World::CreateEntity` always emplacing `EntityMeta`, so
  `Each<EntityMeta>` is the all-entities view — true, and it removed a whole "new World API" premise).
- When one deliverable in a milestone can't compile without another, they are one step. Discovering that early is
  what turns an over-large milestone into a correct decomposition ([[L3]]) instead of a mid-build stall.

## L17 — Commit at each step boundary, or the step's SCOPE GATE becomes unprovable (2026-07-27)

**What happened (Editor M2a/M2b):** both slices end in a dogfood step whose gate is *literally a diff shape* —
"a game module adds an editor extension with **zero changes to `OpaaxEditorLib`**", checked as "changed +
untracked paths, filtered for anything outside `Sandbox/Editor/`, must be empty." In **M2a** I committed S1+S2
before building S3, so that check ran against a clean tree and the gate was provable — and it stayed provable in
history (`git show --stat`). In **M2b** I built S1 and S2 back-to-back without committing between. Both were
green, but the working tree now mixed editor-side infra with the game-side dogfood, so the gate held only *by
construction* ("I know which files I touched"), which is exactly the kind of claim this project doesn't accept.
Recovering it was real work: revert the one file both steps touched (`SandboxEditorModule.cpp`) to its S1 state,
**rebuild to confirm that intermediate state actually compiles** (never commit a state you haven't built),
commit S1, re-apply S2, rebuild, commit.

**Rules for next time:**
- **If a step's gate is a property of the DIFF (scope, blast radius, "touches only X"), that step must start
  from a clean tree.** Commit the previous step first. A gate you can only assert from memory is not a gate —
  same standard as [[L15]] (the log must *discriminate*), applied to git instead of logging.
- Plan the commit boundaries when planning the steps, not after. If step N's verification section says
  `git diff --stat` / `--name-only`, that sentence *is* a commit instruction for step N-1.
- When splitting after the fact, the shared file is the whole cost — reconstruct its intermediate state, and
  **build before committing it**. A committed-but-unbuilt intermediate is the [[L14]] stale-green trap with
  extra steps: it looks fine until someone bisects onto it.
- Don't sweep the user's own files into a feature commit while doing this (`Docs/TODO.txt` here) — pathspec
  commits only ([[L9]]); they may be mid-thought in them.

## L18 — A documented caveat is a bug with a comment on it: if you can FIX it, fixing is the deliverable (2026-07-27)

**What happened (M2c / debug-draw channel design):** reviewing a sub-agent's design I caught a genuine
overclaim — it asserted cross-DLL `OpaaxStringID` identity was "already proven in production," when in fact
*every* current interning call site runs on one side of the boundary, so nothing had ever exercised it. Good
catch. But I then proposed to **record it in ARCHITECTURE.md as a caveat to verify later** ("an I2 assertion
to test, not an established fact") and moved on. The user cut straight through it: *"So fix the cross-dll
problem instead of record."* The fix was ~30 minutes — move `OpaaxStringIDPool` and its entry points
out-of-line into the DLL — and it converted a compiler-courtesy into a **link-time guarantee**.

**Why I got it wrong:** I was optimising for slice scope (M2c was about debug draw, not strings) and treated
"write it down so it isn't lost" as the responsible move. But the defect was *latent and silent* — a
duplicated intern pool makes the same string compare unequal across the DLL line, with no crash and no null,
which is the worst failure mode in the codebase's own taxonomy. Documenting a silent, latent, cheap-to-fix
defect is the one case where "record it" is close to worthless: the note only helps someone who reads it
*before* being bitten, which is exactly nobody.

**Rules for next time:**
- When a review turns up a real defect, the default is **fix it now**, not file it. Escalate to "record and
  defer" only when the fix is genuinely expensive, genuinely risky, or genuinely blocked — and say which.
  "It's out of this slice's scope" is not one of those three.
- Weight the decision by **failure mode, not by size**. A silent/latent defect (wrong answer, no signal)
  outranks scope discipline; a loud one (crash, null, red build) can wait, because it will announce itself.
  Same axis as [[L15]]: what matters is whether anything *discriminates* when it goes wrong.
- Fix it so the wrong thing is **impossible, not merely unlikely**. Out-of-line in the DLL beats
  exported-inline-and-hope; a forward-declared type beats a visible one a consumer could duplicate. Prefer
  the shape where the compiler/linker enforces the invariant over the shape where a comment asks nicely.
- **Don't let a sub-agent's verdict close a question you haven't checked** ([[L16]] sharpened): Fable's
  design was good and its recommendation held up, but its one load-bearing "already proven" claim was false.
  Verify the claims the decision *rests on*, and when one collapses, that is a finding to act on — not a
  footnote.

## L19 — Plan artifacts age; an inherited FIXME and a stale name are both defects you now own (2026-07-27)

**What happened (M2d planning):** two premises of my plan came from *artifacts about* the code rather than
the code, and the user corrected both in one message.
(1) The M2 overview called the slice "AssetTypes + AssetBrowser", so I named everything `Asset*` — but
`Asset` is the **retired** vocabulary (`Legacy/Assets`, unlinked), and the live system is
`CResource`/`ResourceManager`. The overview predated that rename; I inherited its words without grepping
them. *"Maybe lets match the terme 'Resource' since the engine use that."*
(2) The browser needed directory enumeration. `IFileSystem` is the facility for that and was unusable —
every method private, non-const behind a `const&` accessor — so `EditorService` already called
`std::filesystem` directly under a FIXME. I planned to add a **second** direct caller plus a NOTE
explaining why. *"Maybe its time to fix IFileSystem."* Repairing it took ~40 minutes, made the browser's
scan go through the seam, deleted the FIXME, and added the first tests the facility has ever had — and it
turned up a real bug nobody had hit: `GetPathIfNCreate` wrapped `CreateDirectories` in try/catch but
called `IsPathExist` **outside** it, and `fs::exists` throws.

**Why I got it wrong:** I wrote [[L18]] one slice earlier and still repeated its shape. L18 was about a
defect *I* found, and I read it as "don't file your own findings". But a FIXME someone already wrote is
not terrain — it is a defect with a note attached, and routing around it is exactly the deferral L18
names. The tell is unmistakable in hindsight: *I was about to write a comment explaining why I was not
using the obvious facility.*

**Rules for next time:**
- **When a comment explains why you are bypassing the right abstraction, that comment is the work item.**
  An existing FIXME/TODO in your path is inherited scope, not scenery. Repairing a facility you are about
  to lean on is cheaper than it looks, and it is the only way the seam ever gets exercised ([[L18]] —
  which applies to defects you inherit, not only to ones you discover).
- **Re-derive names and constraints from the CODE, not from the plan doc that named the slice.** Planning
  artifacts freeze the vocabulary of the day they were written; a rename since then makes them actively
  misleading. Grep the term before adopting it — if `Legacy/` owns it, the name is taken
  (ARCHITECTURE.md **X4**).
- **A skeleton call site is a guess when it names a type nothing defines.** `AssetTypes().Register<TAsset,
  TActions>()` presumed an asset type that never existed; it could not survive contact. Amend the contract
  (**MR1a**) rather than bend a design to fit a placeholder's shape.
- When repairing a facility, make the *contract* honest too, not just the access specifier: `error_code`
  overloads so nothing throws, and `CreateDirectories` answering "the directory exists" rather than
  forwarding `std::filesystem`'s "I created nothing" `false` for an already-present dir.

## L20 — Never spend unrecoverable user state on a test; and check that the test hits the branch (2026-07-27)

**What happened (M2d S1):** to prove `GetPathIfNCreate` had correctly replaced a direct
`fs::create_directories`, I ran `rm -f Sandbox/Editor/Save/imgui.ini` and relaunched the editor. It came
back, so the swap looked verified. But that file is **gitignored and untracked** — unrecoverable — and it
held the user's saved dock layout, which was regenerated at the default arrangement. Worse, the deletion
did not even exercise the branch I was testing: removing the *file* leaves the *directory*, so
`GetPathIfNCreate` took its "already exists" path. I destroyed real state and learned nothing from it.

**Rules for next time:**
- **Before deleting anything to test with, ask whether it is recoverable.** `git check-ignore <path>` /
  `git ls-files --error-unmatch <path>` answers it in one command. Gitignored + untracked = gone. Local
  user state (layouts, saves, configs, logs they are reading) is the user's data even when it is
  regenerable in principle — the regenerated file is not the one they had.
- **Exercise destructive branches against a scratch path.** I had *just written* a temp-dir test suite
  that does exactly this; the right instrument existed and I reached past it for the live file.
- **Name the branch you intend to hit, then confirm the setup reaches it.** "Delete the file" and "delete
  the directory" are different preconditions; only one of them tested the code I had changed.
- If it happens anyway, say so **first**, at the top of the next message — not buried under the green
  gates that followed.

## L21 — Prove the premise with a failing test BEFORE designing on it; and don't let the instrument depend on the thing under test (2026-07-28)

**What happened (`WindowsFileSystem`):** the ask was structural — make `IFileSystem` virtual so each
platform holds its own type. Written that way, `WindowsFileSystem` would have been a byte-for-byte copy
of the base, the kind of class that invites "why does this exist?". Before designing, I noticed
`WindowsPlatform::GetExecutablePath` converts to **CP_UTF8** while `IFileSystem` fed those bytes to
`std::filesystem::path(const char*)`, and I *believed* MSVC decodes that as ANSI — but "I recall MSVC
does X" is not a finding. So I wrote the test first and ran it against the unmodified code: `IsPathExist`
returned **false** for a directory that plainly existed, and `ListDirectory` returned CP-1252 bytes. A
guessed-at concern became a proven, silent, latent defect — and gave the new class its actual reason to
exist ([[L18]]: the default is fix it now).

**The near-miss:** my first version of that test wrote the Unicode name as **literal characters**
(`L"Éclair"`). The build sets no `/utf-8` and the sources have no BOM, so MSVC would decode those source
bytes *using the ANSI code page* — the exact mechanism under test. The test could have gone red because
the literal was mangled, or green because both sides were mangled identically. I caught it before
trusting the result and switched to `\uXXXX` escapes, which mean the same thing regardless of file
encoding.

**Rules for next time:**
- **When a design rests on "X behaves badly", make X fail a test before you build the fix.** It costs one
  build, converts a recollection into evidence, and the failing test becomes the regression gate for
  free. It also tells you honestly when the concern was imaginary and the smaller change was right.
- **An instrument must not share a failure mode with the thing it measures** ([[L15]] sharpened: there,
  the log had to *discriminate*; here, the test must not be decoded by the mechanism under test). Ask
  what the instrument itself depends on. Encoding tests use escapes, not literals; timing tests don't use
  the clock under test; a serialisation round-trip that only checks itself proves nothing.
- **Prefer an assertion that cannot pass by accident.** A CP-1252-representable character (`É`) can
  round-trip through a wrong-but-consistent encoding; a CJK one (`U+65E5`) cannot survive ANSI at all. Pick
  the input whose failure is structural, and verify against a *different* API (the OS's wide call) than
  the one you are testing.
- A "just make it virtual" refactor is worth a look at what the seam has been quietly getting wrong —
  splitting an interface is when you finally read the implementation as a contract.

## L22 — Before inventing a phase to fit an ordering constraint, ask whether something is running in the WRONG phase (2026-07-28)

**What happened (M3 S4).** My approved plan asserted "the subsystem create-pass has already run
(BootEngine), so WorldManager — and the ComponentRegistry it owns — exist". It hadn't:
`ISubsystemManager::StartupAll` constructed *and* started in one call, so no subsystem existed until
`Engine::Startup()`. My bind call therefore reached `Engine().GetWorldManager()`, hit that accessor's
lazy-Startup safety net, and **booted the whole engine 0.7s early** — world created,
`ComponentRegistry` sealed, and the game module's component refused when `RegisterModules` finally
ran. I had taken the claim from **ARCHITECTURE.md's own SE table**, which turned out to be aspiration.

**My fix was machinery. The user's fix was deletion.** I split the create pass out of `StartupAll`
and added an `IEngine::BootSubsystems()` phase to squeeze registration into the new gap. It worked,
it was defensible, I cited LC1 for it — and it was wrong. The user pushed back: *"instead of create
world at start up of the subsystem, lets create a real flow where all the engine boot then at the end
create the world."* The actual defect was that **`WorldManager::Startup` created a world at all** —
a subsystem doing CONTENT work during INFRASTRUCTURE boot. Remove that, and the ordering problem
evaporates: nothing seals during startup, registration has all the room it needs, and `CreateAll` /
`BootSubsystems` get deleted. The "always have a render target" justification for the default world
was already dead — every consumer null-checks.

**Why the tests were green throughout.** All 162 passed, including a `ModuleRegistrar` suite that
registers a component and round-trips it. They construct a `ComponentRegistry` and `ModuleRegistrar`
**directly**, so they verify the route's *logic* while saying nothing about *when the real thing is
wired during boot*. Only the ordered `Sealed with N component type(s)` log line could discriminate —
it exists because [[L15]] says to log the success branch.

**Rules for next time:**
- **When ordering has no legal window, suspect a MISPLACED step before inventing a new phase.**
  [[L7]]/LC1 ("the missing thing is a phase") is real but it is not the first question — it is the
  answer when every step is in its right place and the phases genuinely don't cover the transition.
  Ask first: *is something running in a phase it doesn't belong to?* A new phase that preserves a
  layering mistake is machinery protecting a bug. Adding code to make a wrong thing work should feel
  worse than deleting the wrong thing.
- **Sort a lifecycle step by INFRASTRUCTURE vs CONTENT.** Starting a subsystem brings up capability;
  choosing which world/level/asset to open is content, and content is the host's call, driven by
  config. A subsystem's `Startup` that creates game objects is the smell.
- **A lifecycle/ordering claim is a premise, not a finding — even when the CONTRACT states it**
  ([[L21]] applied to boot order). ARCHITECTURE.md records intent and can drift ahead of the code
  exactly like a planning doc ([[L19]]). Verify by reading the call chain or a log before designing
  on it.
- **Unit tests that construct the subject directly cannot verify wiring.** When correctness depends
  on *where in boot* something is called, the gate is a run of the real host with an ordered log. Ask:
  "could this test pass in a build where the wiring is absent?" Here the answer was yes, for all 162.
- **A lazy "safety net" accessor turns a too-early call into a silent reorder, not an error**
  ([[L6]] from the caller's side). `if (!m_bStarted) Startup()` inside a getter means a premature
  reach *succeeds* while quietly moving the whole boot.

## L23 — "Known gap, stated not hidden" is still a deferral. If the missing gate is cheap, BUILD it (2026-07-29)

**What happened (M4 S3).** I shipped the whole world-subsystem mechanism — registry, `WorldContext`,
per-world creation, tick path — with every unit test green and both hosts booting clean. Then I wrote
into my own review: *"Known gap, stated not hidden: end-to-end filtering in a real world is proven only
link-by-link… the composed gate is S5's dogfood."* I even justified not closing it, with a rule I like:
*"registering a throwaway candidate now would be an API with a disposable caller."* And I moved on to
propose S4. The user asked one question — **"So do we have an concrete example subsystem?"** — and the
answer was no: all six world subsystems in the tree were test doubles, nothing registered one, and both
hosts logged `0 of 0 subsystem candidate(s) created`. The headline feature of the milestone had never
run in an actual game.

Closing it took about thirty minutes and needed **no** S5 machinery, because `ShouldCreate` reads the
world's mode — so a Play-only and an Edit-only subsystem both go through the route that already existed.
The payoff was not cosmetic: the two `Update`-only log lines (`Captured 3 quad baseline(s)` /
`drawing 3 outline(s) per frame`) **proved the tick path**, which no run with zero candidates could, and
the mirrored `1 of 2` in each host turned a link-by-link argument into one observable fact.

**Why I got it wrong, and why this one stings.** This is [[L18]]'s shape (*a documented caveat is a bug
with a comment on it*) and [[L19]]'s (*when a comment explains why you are bypassing the right thing,
that comment is the work item*) — and I wrote the caveat **in the very review section where I had just
quoted both lessons back to myself**. Writing the gap down felt like rigor: it was labelled, scoped, and
assigned to a future slice. That is exactly what makes this failure mode durable — honest disclosure is
indistinguishable from diligence right up until someone asks the obvious question. My "no API without a
caller" rule was also misapplied: the example subsystems are not scaffolding to delete, they are the
module's first real content and they stay.

**Rules for next time:**
- **A plan that defers the only COMPOSED gate has no gate.** When the milestone's headline claim
  ("a world runs a filtered set of subsystems") is proven only as a conjunction of separately-tested
  links, the deliverable is not done. Ask directly: *has the feature ever run in the real app?* If the
  answer is no, that is the next step — not the next slice.
- **Sequencing from the plan is not a reason.** S5 owned the dogfood only because the plan bundled it
  with a toolbar; the *subsystem* half needed nothing from S5. Before deferring to a later step, check
  which part of it you actually depend on — often it is none of it.
- **Judge a first example by whether it SURVIVES, not by whether it is minimal.** "An API with no
  caller" forbids speculative surface; it does not forbid the first real caller. If the example is
  something the project keeps, writing it is delivery, not scaffolding.
- **When you catch yourself labelling a gap instead of closing it, price the close first.** Thirty
  minutes vs a whole slice of unverified machinery is not a trade-off, it is an answer. Escalate to
  "record and defer" only when the fix is expensive, risky, or blocked — and say which ([[L18]]).

## L24 — Confirm the binary you smoke-tested is the one you just built (2026-07-29)

**What happened (M4 S3).** After building `test` + `release` + `release-editor`, I smoke-tested
`Sandbox.exe` from `build/debug-editor/` and got a log that was **completely empty** — which my grep
reported as `err/warn: 0`, i.e. indistinguishable from a clean boot. `build.bat test` builds only
`OpaaxTests`, so that exe was still the **S2 build from an hour earlier**, now paired with an S3 DLL
whose `World` layout and `WorldManager` vtable had both changed. It almost certainly died on load. One
`ls -la` told the whole story: DLL 21:06, exe 20:06.

**Rules for next time:**
- **Before smoke-testing a host, compare the exe's mtime to the DLL's.** One `ls -la` on both. This is
  the [[L14]] stale-object trap wearing different clothes: there, an up-to-date `.o` hid broken source;
  here, a stale exe hid an ABI break. Same root — *a build artifact you did not just produce is not
  evidence about the code you just wrote.*
- **`build.bat test` builds ONLY `OpaaxTests`; `fast` skips editor targets.** Green tests say nothing
  about whether the hosts still link. Run the full preset before touching a host.
- **An empty log is not a passing log.** Grepping only for errors makes "produced no output" look
  identical to "ran cleanly" — the [[L15]] discriminate rule applied to the *absence* of output. Check
  line count (or assert on a known-good startup line) before reading a smoke result as success.

## L25 — A defect caught by an INCIDENTAL compile error is a near-miss: ask what happens when it compiles (2026-07-29)

**What happened (M4 S3).** Injecting `WorldContext&` into world subsystems failed to build with a
confusing C2665 inside `<memory>`. Cause: `ISubsystemManager::RegisterSubsystem` captures its ctor args
**by value** into the factory lambda, and `StartupAll` **clears `m_Factories`** once consumed — so the
context was being copied into a lambda that is then destroyed, and every subsystem's stored
`WorldContext&` would have pointed at freed memory. It only failed to compile because an rvalue will not
bind to a non-const lvalue reference. Fixed with `std::ref`, so what gets copied is a pointer to the
World-owned context.

**The near-miss is the lesson.** Nothing about my design caught this. Had `WorldContext` been taken by
value in the ctor, or been copy-assignable in the wrong way, it would have compiled and shipped as a
silent use-after-free — the codebase's worst failure class ([[L18]]).

**Rules for next time:**
- **When a compile error stops a bug rather than a review doing so, treat it as luck and re-derive the
  invariant.** Ask: *what would have happened if this had compiled?* If the answer is memory corruption
  or a silent wrong answer, the mechanism needs a comment saying why, plus a test that fails when it
  regresses — the compiler will not be there next time.
- **Read the lifetime of anything captured into a stored callable.** "Forwards its arguments" usually
  means *copies* them, and a factory list that is cleared after use makes those copies short-lived. A
  reference handed to such an API is a dangling reference waiting for a caller.
- **Pin it with an assertion on IDENTITY, not on contents.** The gate compares a started subsystem's
  context address against `World::GetContext()`; comparing a *field* would pass anyway, since freed
  memory usually still holds the old value ([[L15]]).

## L26 — A design doc's ORDERING claims are the ones to distrust: they read as obvious and only execution falsifies them (2026-07-31)

**What happened (M4 S4/S5).** `Editor.md` had said for months that PIE's runtime state is *"rebuilt in
`WorldSubsystem::Initialize`"*. Building `CloneWorld` proved it cannot be: the clone is `Capture` →
`CreateWorld` → `Instantiate`, and `CreateWorld` starts the subsystems, so at `Startup` the cloned world
is still **empty**. This is the second time the same document was wrong in the same way — S3 found that
its subsystem *injection* rule ("the registration site captures the dependency into the factory") was not
implementable against a no-argument call site. Both sentences were plausible, both survived several
readings, and both were only falsifiable by writing the caller.

**Why ordering specifically.** A doc's claims about *structure* (what exists, what owns what) get checked
constantly, because every reader compares them against the file tree. Claims about *sequence* — what has
run by the time X runs — are checked by nothing until something actually runs in that order.

**Rules for next time:**
- **Before trusting a doc sentence of the form "by the time X happens, Y has already happened", find the
  two call sites and read the order.** If the caller does not exist yet, mark the claim as unverified
  rather than as contract.
- **When execution contradicts the doc, fix the doc in the same change** (CLAUDE.md §0), and fix the
  *substance*, not the vocabulary. Renaming `Initialize` to `Startup` in that sentence would have left it
  just as false.
- **Prefer the uniform contract over the locally convenient one.** Instantiating before starting would
  have made the doc's claim true for clones — and made a cloned world the only populated-at-`Startup`
  world in the engine. "Depends how your world was made" is a worse answer than a flat no (**WS7**).

## L27 — When a transformation works, know WHY: it may be working by accident (2026-07-31)

**What happened (M4 S5).** `DeriveTypeLeafName` turns a C++ type into a registry name by stripping
everything up to the last `::`. It had been correct for two milestones. Moving one subsystem into the
editor module — whose types live in the **global namespace** — produced the registry entry
`'class QuadBoundsSubsystem'`. MSVC's `entt::type_name` is *elaborated* (`"class Opaax::Foo"`), and the
`::` strip had been removing that keyword **as a side effect**. No `::`, no strip. For a subsystem the
name is only logs and editor UI; for a **component** it is the key written into map files, so the first
global-namespace component would have written a map nothing could read back.

**Rules for next time:**
- **A helper that handles every input you have tried is not the same as a correct helper.** Ask what
  *class* of input has never been tried — here, "no namespace at all" had literally never occurred,
  because every prior type was in `Opaax::` or a test namespace.
- **When two transformations happen in one step, verify each independently.** "Strip the namespace" and
  "strip the elaborated-type keyword" were one line pretending to be one operation.
- **Moving code to a new context is a cheap fuzzer.** The relocation cost nothing and exposed a latent
  defect no test would have found, because every test type was namespaced too. When a placement change is
  otherwise neutral, the fact that it exercises a new shape is a reason to do it, not a risk.
- **Read the whole smoke log, not the lines you went looking for.** This was found in a `Registered ...`
  trace line during a run whose purpose was checking something else — the [[L15]] discriminate rule
  applied to output nobody asked for.

## L28 — "Per-frame" is meaningless until you name WHOSE frame, and which readers are outside it (2026-07-31)

**What happened (M-Input).** `InputManager::EndFrame()` — the call that closes the input frame — went at
the end of `Engine::Loop`. The reasoning was sound as far as it went: the host polls OS events *before*
`Loop`, so a subsystem `Update()` hook would run after the very events it must precede. What I missed is
that **`Loop` is not the end of the host's frame either.** The editor draws its whole UI *after* `Loop`
returns, so every panel read edges, mouse delta and scroll that had already been cleared. The user found
it in one glance: the Input panel showed held keys and nothing else.

**Why it survived my own verification.** `EndFrame` clears the *transient* state and leaves the *held*
state alone — and held state is the only part a log line or a boot smoke test can show. The tests passed
because they call `EndFrame` themselves, in the order the design assumed. Every instrument I had was
blind to it by construction.

**Rules for next time:**
- **Before placing a per-frame boundary, list the READERS and where each one runs.** Here they were a
  game system in `Update` (inside `Loop`) and an editor panel in the UI pass (outside it). A boundary is
  only correct if it sits outside *every* reader — which made the host loop the one honest place.
- **`Engine::Loop` is the engine's tick, not the frame.** The frame belongs to `RunApplication`, which
  also owns `PollEvents` and `Present`. Anything that must bracket the whole frame belongs there.
- **When a feature "half works", the working half is a clue, not a comfort.** Ask what distinguishes the
  part that works from the part that does not — here, "cleared by EndFrame" versus "not cleared", which
  named the bug immediately once asked.

## L29 — A third-party flag answers ITS question, not yours (2026-07-31)

**What happened (M-Input).** D5's step 1 is "ImGui `WantCaptureMouse` → the UI eats it", and that is what
was implemented. But `WantCaptureMouse` means *"the pointer is over some ImGui window"* — and in this
editor one of those windows **is the game**, an ImGui image with the world rendered into it. So a game
running inside the editor could never receive a click, a drag or the wheel. The mouse position in the
Input panel appeared to update only when the cursor crossed a gap between windows.

**The general shape.** The flag was not wrong; the *question* it answers stopped matching mine the moment
the UI framework started hosting the thing the UI is supposed to keep its hands off. Keyboard was fine
under the identical rule, because `WantCaptureKeyboard` only goes true for a text field — a genuinely
narrower question that still matched.

**Rules for next time:**
- **Translate a borrowed predicate into your own words before gating on it.** "Is the pointer over an
  ImGui window" is not "should the UI own this input" once one of those windows is the viewport.
- **Ask which of your surfaces the library considers its own, and whether that is still true.** The
  moment the world renders *into* the UI (M1's render-to-texture), every "is the UI busy" flag needed
  re-reading — two milestones later.
- **Check the exemption asymmetry.** Mouse needed the carve-out and keyboard did not; blanket-applying
  either answer would have been wrong in one direction or the other.

## L30 — A field two things share only because one of them is always empty is a latent bug (2026-08-03)

**What happened (M5 S3).** `OpaaxApplication::GetStartupWorldSpec` put
`IProjectManager::StartupLevel()` straight into `WorldSpec::Name`. That had been correct for three
milestones — because `startupLevel` was `""` in every project file, so the fallback `"Main"` was
what actually ran, every time. The moment M5 made the key real
(`"Levels/Main.opaaxlevel"`), the same line would have produced a **world named
`Levels/Main.opaaxlevel`**. The fix was to split the field: `LevelPath` for the path, `Name`
derived from its stem.

**Why it survived so long.** Nothing was wrong with the code *as executed*. Every test passed,
both hosts booted, and the log printed `world 'Main'` exactly as intended. The defect lived
entirely in the branch that had never been taken — and the only reason it had never been taken was
that the FEATURE the field existed for had not been built yet.

**Rules for next time:**
- **When a config key finally gets a real value, re-read every consumer of it.** A key that has
  been empty for the whole life of the codebase has consumers that were only ever exercised on
  their fallback path. Grep the key, not just the feature you are adding.
- **Two meanings in one field is the smell, and "it's always empty" is what hides it.** A *name*
  and a *path* are different things; they were one field because nothing had ever made them
  differ. Ask what the field would hold if the feature it serves actually worked.
- Sibling of [[L27]] ("know WHY a transformation works — it may be working by accident"), one level
  up: there the helper was right for every input tried, here the *caller* was right for every value
  tried. Same question in both cases — **what class of input has never occurred?**

## L31 — A derived answer is cheap to get right and expensive to get frequent (2026-08-03)

**What happened (M5 S5).** The editor's "unsaved changes" marker is DERIVED — capture the world,
serialize, compare against the text last written. That design is right, and I defended it in the
plan against a tracked flag for good reasons (nothing to hook, no drift, and it correctly reports
*clean* when an edit is undone back to the original). Then I implemented it **in the per-frame draw
call** — which my own approved plan had explicitly said not to do ("evaluated on menu-open / Play /
quit, never per frame"). A ten-second smoke run produced **1694 identical log lines**, and the
readout I had reached for as evidence was the thing that exposed it.

**Two distinct defects from one mistake.** The cost (a full capture + serialize per frame, fine for
three quads and not for a real map) and the noise (an Info-level log on a path that now ran at
framerate). Fixing only the log would have left a per-frame O(map) walk nobody would notice until
a map got big.

**Rules for next time:**
- **When a design's whole premise is "compute it instead of storing it", the frequency is part of
  the design, not an implementation detail.** Decide *when it runs* in the same breath as *what it
  computes*, and write both down. I did write it down — and then did not read my own plan when it
  came time to place the call.
- **Put the throttle where the clock is, and keep the computation pure.** `IsDirty` stays a plain
  function of (world, registry) — testable, unable to go stale — and the caching lives in the
  frame-owning caller. A cache inside the pure thing would have made it neither.
- **A log level is a claim about frequency.** `Info` says "this happens when something happens."
  A pure transformation with several callers cannot promise that, so `MapSerializer::Capture`
  belongs at `Trace`; the Info lines belong to `MapFile::Save`/`Load` and `MapFactory::Instantiate`,
  which are things that happen *to* something. [[L12]]'s "match the instrument to the event
  frequency" applies to the code being measured, not only to the probe.
- **Read the log's line COUNT, not just its errors.** 145 lines vs 1796 was the entire signal, and
  a grep for `error|warn` reported 0 in both. Same shape as [[L24]]'s empty log: the absence of
  complaints is not evidence of correctness.

## L32 — Deriving an identity from a FILE PATH is mining, not sourcing: ask where the authored value lives (2026-08-04)

**What happened (code review).** M5 split `WorldSpec` into `{Name, LevelPath}` after [[L30]] caught the
two-meanings-in-one-field bug, and I derived the `Name` from the path's stem —
`Name = DeriveWorldName(LevelPath)`. I was pleased with it: static, pure, unit-tested against six path
shapes, documented as "the one place the path-vs-name distinction is decided." The user read one line and
said **"This is bad."** They were right. The fix to [[L30]] had corrected the *symptom* (one field holding
two things) while keeping the actual mistake: **the world's identity was still a function of where its
file happened to sit.** Move or rename the file and the world silently renames; two levels in different
folders with the same filename become indistinguishable. The authored value — what the designer *calls*
that level — had no home in the format at all.

**The fix was to add the source, not to relocate the derivation.** `LevelData::Name` from a `name` key,
stem as fallback. And once the level names the world, `WorldSpec::Name` has no source, so it **left the
seam entirely** along with `DeriveWorldName` — a host that cannot supply a level cannot invent a name for
one either. My instinct had been to move `DeriveWorldName` down into the engine, which would have kept the
string surgery and just hidden it one layer lower.

**Rules for next time:**
- **When code computes an identity (name, id, key, title) from a path, stop and ask where the AUTHORED
  value is supposed to live.** If the answer is "nowhere yet", the deliverable is a field in the format,
  not a cleverer parser. A path is a *location*; a name is *data*. Deriving one from the other couples
  identity to the filesystem, and the coupling is silent — nothing fails, things just quietly rename.
- **A stem fallback is fine; a stem SOURCE is not.** Keep the derivation as the answer for files that
  do not state a name, and put it next to the parser that has the path — not in the caller, and not in a
  seam three layers up.
- **Deleting the derived field is usually the real simplification** ([[prefers-deletion-over-machinery]]).
  Once the value has a genuine source, ask which callers were only passing it along; here the whole
  `Name` field and its host-side helper and its six tests all went, and the seam got smaller.
- **A test suite over a bad rule proves the rule, not the design.** Six passing `DeriveWorldName` cases
  made the stem convention look settled. Coverage measures whether code does what you said; it never asks
  whether what you said was the right thing to say ([[L27]]'s "know WHY it works", one level up).

## L33 — A plan that ADDS is not a plan that DELETES: enumerate the removals before you start (2026-08-06)

**What happened (preset cleanup).** My plan described the destination — three presets, a new
`OPAAX_DEV_BUILD` flag — and disposed of the thing being removed in a subordinate clause: "Drop
`release-editor`." The user, on that exact section: **"make sure to delete previous correctly."** The
`git grep` I then ran found `release-editor` in **8 live places across 2 files** and the dead
`OPAAX_BUILD_EXAMPLES` option in **4 more**, plus a `RelWithDebInfo` branch in `Engine/CMakeLists.txt`
that existed only to serve the deleted preset. Any one left behind is a half-existing preset: a
`build.bat` dispatch line pointing at something CMake no longer defines.

**Why the framing caused it.** Adding is self-verifying — the new thing either builds or it does not.
Removing is not: every leftover reference still compiles, still looks intentional, and only fails for
whoever types the dead name months later. So a deletion gets *no* feedback from the thing that gives
implementation its confidence, which is exactly why it needs the enumeration up front instead of a verb.

**Rules for next time:**
- **Run the exhaustive `git grep` while PLANNING, not while implementing, and paste the hit list into
  the plan as its own first step.** "Drop X" is a verb, not a step. The enumerated list is the step, and
  a zero-hit re-grep is what proves it finished ([[L8]]'s "grep the marker" applied to source).
- **Split live references from historical records, explicitly.** Plans, lessons and archives naming the
  deleted thing must SURVIVE — they describe what was true then, and rewriting them is falsifying a log.
  Say which files are exempt and why, or the sweep silently eats the project's memory of itself.
- **Name what grep cannot reach.** A deleted `option()` lingers in every existing `CMakeCache.txt`
  forever; the orphaned `build/<preset>/` tree, cached IDE profiles and generated `.sln`s are all state
  no source-tree search will show you. Only a fresh tree drops them — list them for the user rather than
  reporting the sweep as complete ([[state-blast-radius-of-fixes]]).
- **This is the deletion-shaped case of [[prefers-deletion-over-machinery]].** The user reaches for
  removal often, so "remove X and add Y" is a recurring plan shape here — treat the removal half as
  first-class work with its own verification, never as the preamble to the interesting part.

---

## L34 — An authoring feature is justified by what the author stops REDOING, not by what the runtime preserves (2026-08-06)

**What happened (the PersistentMap / `RootLevel` call).** To decide whether WM1's `RootLevel` was
needed, I asked what I thought was the deciding question: *"is there entity state that must outlive a
Level change?"* — and offered to drop `RootLevel` if the answer was no. The user answered a **different
question**: the persistent map exists so the player, the lights and the managers are authored **once**
and never dragged into another map again; open a decor map, hit Play, the player is there. That
reframing deleted an entire runtime object. State survival needs a second `Level` above the first;
authoring cost needs **one key in a manifest** (`persistentMap`, absent ⇒ first entry, no version bump).

**Why the framing caused it.** I reasoned from the runtime data model — who owns what, what outlives
what — because that is the vocabulary `ARCHITECTURE.md` §WM is written in, and the question sounded
rigorous. But a persistent map is an **editor affordance**, and its value is measured in actions the
author does *not* take. A runtime question about an authoring feature gets a runtime-shaped answer, and
runtime-shaped answers are always bigger: they add objects and lifetimes where the authoring answer
added a field.

**Rules for next time:**
- **For anything the author touches, ask "what does this stop them from redoing?" BEFORE "what does this
  preserve?"** The second question builds objects; the first builds data. Both can be right, but only one
  of them was the motivation, and the motivation is what sizes the solution.
- **Test every proposed new noun against the workflow that motivated it.** When the workflow is *"I don't
  want to do X twice"*, the answer is almost always DATA — a key, a flag, a mount order — not a new
  runtime owner ([[prefers-deletion-over-machinery]]).
- **When I ask a design question and the user answers a different one, theirs is usually the load-bearing
  question.** They reason from their own authoring loop, which is the loop the engine exists to serve.
  Re-derive from their framing rather than restating mine and asking them to pick.
- Corollary for the contract: a settled invariant can be settled *for the wrong reason*. `RootLevel` was
  in WM1 since 2026-07-28 and nothing had contradicted it — it survived because it was never asked the
  authoring question, not because it had answered one ([[L32]]'s "ask where the authored value lives",
  applied to a design instead of an identity).

## L35 — A race is a symptom; find the INVARIANT under it and test that deterministically (2026-08-12)

**What happened (the `OpaaxStringIDPool` dangling reference).** `Get()` returned a `const OpaaxString&`
into a `std::vector` and released its `shared_lock` *on return*, before the caller copied — so a
concurrent `GetOrAdd` reallocation left the reference naming freed memory. I fixed it (the lookup map
owns the text, the id→text array holds pointers, entries never move) and wrote the obvious guard: four
writer threads interning while four readers resolve names. It passed. Then, to check the guard was real,
I reverted the storage to the broken shape and ran it again — **it passed there too, five runs out of
five.** The window between dropping the lock and copying is a few instructions wide and a vector
reallocates only log2(n) times, so millions of reader iterations sampled it zero times.

The invariant the fix actually establishes is not concurrent at all: **an entry's address is stable
across growth.** Take a `CStr()`, intern 4096 names, check the pointer still names the same text —
single-threaded, deterministic, and it failed the broken storage on the first assertion, with the
pointer resolving to *different bytes*. The threaded case was kept, but demoted to what it really pins:
the interning contract under contention (same text from two threads ⇒ one id).

**Why I reached for the wrong instrument.** The bug was *described* as a data race, so I wrote a race.
But concurrency was only what made the defect **observable**; what made it a defect was a single-threaded
property of the container. Threaded tests are probabilistic by construction — passing one is evidence of
nothing, and I would have shipped a green suite claiming a guard it did not provide.

**Rules for next time:**
- **After fixing a race, state the invariant the fix establishes as a sentence with no threads in it.**
  If that sentence exists — "entries never move", "this pointer stays valid", "this counter only grows" —
  test *that*, deterministically. If it genuinely cannot be stated without threads, say so explicitly.
- **A concurrency test that passes proves nothing until it has been run against the BROKEN code.** Revert
  the fix, run it; if it still passes, it is documentation, not a guard. This is the cheap check and it
  cost one build cycle here ([[L21]]: the instrument must be able to fail).
- **Pick fixture data that can actually fail.** The long interned names survived even broken storage — a
  moved `OpaaxString` steals its heap pointer, so `CStr()` kept answering the same address *by accident*
  ([[L27]]). Only a short, SSO-stored name, whose bytes live inside the entry, discriminates. Ask which
  input distinguishes the hypotheses before writing the assertion.
- Corollary for reviews: "the pool is in the DLL, so it is DLL-safe" answered **where the table lives**
  and was read, for the two weeks since, as if it had answered **what the table does**. A settled invariant covers the
  question it was asked ([[L34]]'s corollary) — I2 had never once looked inside the pool it placed.

## L36 — Read what the convenience layer COSTS before recommending the terser call (2026-08-12)

**What happened.** Having made `OpaaxStringID::CStr()` zero-copy, I told the user their log sites could
go further and drop the accessor entirely — `"{}", lName` instead of `"{}", lName.CStr()` — because a
fmt formatter for the type already existed. They said do it. Opening the formatter to run the pass, it
read `fmt::formatter<std::string>::format(std::string(StringID.CStr()), CTX)`: **it copies into a
`std::string` on every call.** So the terser form I had just recommended *added* a heap allocation per
argument, while `.CStr()` — the thing I was proposing to remove — passed a `const char*` that fmt
formats in place with none. My advice was backwards, on the exact axis (cheapness) that motivated the
whole change. The same copy sat in `OpaaxString`'s formatter, taxing ~100 existing log sites.

The fix made the advice true rather than retracting it: both formatters now inherit
`fmt::formatter<fmt::string_view>` and format a view over bytes already held (`OpaaxString` passes its
length, so not even a `strlen`). Same format spec, zero allocation — *then* the conversion pass ran.

**Rules for next time:**
- **Before recommending "you can just pass X directly", open the adapter that makes it work.** A
  formatter, a converting constructor, an `operator T()` — each is a small function nobody reads, and a
  copy hidden in one silently inverts the cost argument you are making. One `tail -12` would have
  caught this before I said it.
- **When the point of a change is CHEAPNESS, the terser spelling is not automatically the cheaper one.**
  Terseness and cost are independent axes; I merged them because the change so far had improved both.
- **A defect in a convenience layer is multiplied by its call sites, so it is worth finding even when
  you arrived by accident.** This one was taxing every logged string in the tree, not just the sites
  under discussion — fixing it was a bigger win than the pass that uncovered it ([[L25]]: a defect found
  incidentally is a near-miss; ask what it costs where nobody is looking).
- Corollary: **when a conversion pass makes an existing call site look wrong, suspect the pass.** Mixed
  `Label` / `AbsPath.CStr()` arguments on one line was the tell that I had a rule covering one type and
  not the other, and the reason was that neither should have needed the accessor.

## L37 — A display-only MIRROR of another system's state is coupling wearing a convenience hat (2026-08-19)

**What happened (editor menu refactor).** My plan gave the menu node a `SetShortcut("Ctrl+S")` — text
only, no binding, just the hint ImGui right-aligns. It felt free: one string, and Ctrl+S was genuinely
undiscoverable. The user cut it in one line: *"Shortcut shouldnt be here too. Short cut should
independent things that user can edit etc... Shortcut only trigger commands too."*

**Why they were right.** A rebindable shortcut means the truth lives in a key→tag table. A hint string
on the node is a **second copy of that truth**, hand-written at registration, which goes stale the first
time anything is rebound — and *silently*, since nothing compares the label to the binding. The correct
shape is the menu ASKING the binding system what key carries this tag: costs the menu nothing, cannot
drift. That system did not exist yet, so the right amount of shortcut in this change was **zero**, not
"the cheap half".

**The tell I walked past:** I justified the field by the SYMPTOM ("Ctrl+S is undiscoverable") instead of
asking who OWNS the fact. A field that must be kept in agreement with another subsystem's state is not a
display detail, it is a denormalisation, and the only question is who the source is.

**Rules for next time:**
- **Before adding a field that merely SHOWS what another system decides, name that system and ask
  whether the node can query it instead.** If the system does not exist yet, do not build the mirror as
  a placeholder — build the *precondition* (here: make every verb a command, so a binding has something
  to point at) and leave the display for when there is a source to read.
- Same family as [[L30]] (a field two things share only because one is always empty) and [[L32]]
  (deriving identity from a file path is mining, not sourcing): all three are "the value is written
  where it is convenient rather than where it is owned."

## L38 — Two ways in is one too many: if a route exists for behaviour, close the side door (2026-08-19)

**What happened (same refactor).** The plan kept `AddItem(label, lambda)` beside `AddItem(label, tag)` —
the tag form for the editor's own entries, a closure "escape hatch" for game modules and for
`File > Exit`, which needed a `Window*` only the composition root could resolve. The user:
**"Use Command !"**

**What the escape hatch actually cost.** With a closure form available, a game module's verb lives inside
the menu entry that shows it — invocable from exactly one place and never by tag: not by a key binding,
not by another panel, not by a second entry. `File > Exit` was the same defect in different clothes: a
command whose payload only the composition root can supply is a command **nothing but a menu can
invoke**, because a binding carries a tag and nothing else. Deleting both forms forced the real fix —
`EditorContext` carries the window, `QuitParams` is gone — and the API got *smaller*.

**Rules for next time:**
- **When a mechanism exists for a kind of thing, an alternate path that bypasses it is not flexibility —
  it is a second class of that thing with fewer capabilities.** Before adding the convenience overload,
  ask what the bypassing caller LOSES. If the answer is "everything the mechanism was built to give",
  the overload is the bug.
- **The one awkward call site an escape hatch exists for is usually pointing at a real gap.** `QuitParams`
  had carried a `// because a command cannot ask for one` comment for a milestone — [[L19]]'s shape
  exactly: the comment explaining the bypass IS the work item.

## L39 — Sharing STATE is not sharing a code path: one bool with two writers is still two writers (2026-08-19)

**What happened (editor panel toggles).** Panel visibility is one `bool` per panel. The Window menu
ticks it through `TogglePanelCommand`; ImGui's window close button writes it directly, because
`ImGui::Begin(label, &bVisible)` is handed the bool itself. I wrote — in the plan, in the header
comment and in the commit message — *"the close button and the menu tick read the same bool, by
construction, so they cannot disagree."* Every word of that is true, and I read it as covering more
than it did. The user found the hole in one click: **closing a panel with its X logged nothing**, while
the menu entry logged twice.

**Why the claim was too small.** "They cannot disagree" is a statement about **correctness** — the two
front-ends always show the same answer, and they did. It says nothing about **observability**, undo, or
anything else that lives on the *path* rather than in the *value*. Two writers converging on one
variable share the variable; they do not share the code that runs on the way in. Anything I attach to
one path — a log line today, an undo record or a dirty flag tomorrow — silently does not exist for the
other. The fix was to stop handing ImGui the real bool: `Begin` gets a local, the result is routed back
through `SetVisible`, and that method becomes the single mutation point where the log lives and cannot
be bypassed.

**Rules for next time:**
- **When two paths write one piece of state, ask what runs ON each path, not just what each path
  writes.** "One source of truth" is about the value; it is not a claim that the paths are equivalent.
  The test question is: *if I hang a side effect off this write, do both callers get it?*
- **Prefer one mutation POINT to one mutation TARGET.** A shared variable that two places assign is a
  latent fork; a setter both are forced through is the shape where a later side effect cannot be
  forgotten ([[L18]]: make the wrong thing impossible, not merely unlikely). It costs a local variable.
- **A third-party API handed a reference to your state IS a second writer** — [[L29]]'s shape (a
  borrowed mechanism answers its own question, not yours). `p_open` is ImGui's convenience; the moment
  the write matters to anything of mine, it has to come back through my door.
- **The gap was invisible from the side I tested.** I verified the menu path end to end with a probe and
  watched the tick update, which is exactly what a correct-but-uninstrumented second writer looks like
  ([[L15]]: the instrument has to discriminate). When one of two front-ends is unreachable from a test,
  say so rather than letting the reachable one stand for both.

## L40 — A mechanical rename that preserves semantics exactly has delivered NOTHING; check the change you made is the change that was wanted (2026-08-19)

**What happened (the emplace_back sweep).** Asked to *"use emplace_back when you can"* and then to
convert the remaining ~60 sites, the obvious execution was one `sed` over the tree. I nearly ran it and
stopped on the arithmetic: **`push_back(T{a,b})` and `emplace_back(T{a,b})` are the same thing.** Both
materialise a temporary and move it in. The entire benefit — constructing the element in place — only
exists once the type name is *gone*: `emplace_back(a, b)`. A pure rename would have touched 34 files,
looked like the requested change, passed every gate, and improved nothing at exactly the ~15 sites the
request was about.

So it became two passes: the rename for one idiom in one tree, then dropping the redundant type name
where it actually earned something. The second pass is also where the real constraints surfaced — a
braced-init-list cannot be deduced (`push_back({a,b,c})` needs positional args), and a bare `{}` cannot
bind to a forwarded parameter, so one aggregate legitimately keeps its braces. A sed would have found
the first as a compile error and silently kept the second as a non-improvement.

**Rules for next time:**
- **Before running the bulk edit, hand-evaluate ONE site and state what changed.** If the honest answer
  is "nothing, but it reads consistently", that is a fine goal — say so, and then go find where the
  substantive version of the change actually lives. Do not let a consistency pass wear a performance
  pass's justification.
- **A style instruction usually names a mechanism but means its EFFECT.** "Use emplace_back" means
  *construct in place*; the token is the shorthand. Ask what the rule is FOR, then check the diff
  delivers that, not just the token.
- **Green gates cannot distinguish a real refactor from a no-op one** — both compile and both pass.
  When a change is semantics-preserving by design, the build says nothing about whether it was worth
  making, so the reasoning has to happen before the edit ([[L15]]: the instrument must discriminate).
- Mechanical sweeps still need the per-site read: the two constructs that broke here were invisible in
  a `grep` listing and obvious in the source.

## L41 — "Show me all of X" is a CONTAINER question before it is a verb question (2026-08-20)

**What happened (the Config panel).** The ask was *"puts configs on Editor Menu — menu category, all
registered configs appear, a new config shows up on its own"*. I took *menu* as given, designed a live
`IEditorMenuNode` that enumerates the registry at draw time — plus `EditorMenuCategory::AddNode`, a
command tag, a params struct and a stub command — and spent my one clarifying question on the **verb**:
what happens when you click an entry. The user answered "list only for now", then one turn later named
the real shape: *"we can make a complete panel. More like unreal. A panel that lists all config, click
on 'Config Title' to set it current, draw configs?"* That deleted all five new types. A panel registered
through `Panels()` gets its menu entry free from `BindPanelToggles`, so the entire menu story became one
`PanelDesc`.

**Why I got it wrong.** Their word was "menu", and a menu is where you *reach* a feature, not where a
feature lives. The tell was in my own design: the moment a menu node needed live data, a selection and a
payload, it was a window being spelled as a menu. The evidence was already in the tree — every other
list in this editor (Hierarchy, Resource Browser) is a panel, and none of them is a menu.

**Rules for next time:**
- **Ask where that data lives in the tools being copied.** The user names them — Unreal, Unity, Godot —
  and configs live in a Project Settings *window* in all three. Answer the container question first; the
  verb usually follows from it.
- **A menu is a list of VERBS. When the entries are nouns, the container is wrong.** An entry that needs
  live data + selection state + a payload is a panel.
- **Count what an alternative DELETES, not only what it adds.** The panel route removed five new types
  and reused a registration path that already existed — [[L22]]'s shape (the user's fix is deletion,
  mine is machinery) caught one step earlier, at design time instead of after building.

## L42 — When a slice makes an action CHEAP, price that action on every path it touches (2026-08-20)

**What happened (component properties).** The whole slice existed to make one action trivial: add a
field to a component and have the editor draw it. The user did exactly that — three floats, added to
`OPAAX_PROPERTIES` *and* to the NLOHMANN macro — and the app died at boot with
`[json.exception.out_of_range.403] key 'Size' not found`, thrown out of `from_json` inside
`Level::MountAll` → `FinishStartup`, where nothing catches.
`NLOHMANN_DEFINE_TYPE_INTRUSIVE` reads every field with `at()`, which throws on a missing key, so the
first field added to a component refuses **every map already saved**. One macro name — `_WITH_DEFAULT` —
was the entire difference.

**Why I missed it.** I read that macro three times while planning: I quoted it, I wrote
`OPAAX_PROPERTIES` directly beneath it, and I wrote a paragraph about the two field lists being able to
drift. I was checking whether the *lists* agreed with each other, never what either does against a file
written before it grew. `MapFactory` even had the answer written above the line that threw — it
tolerates an unknown component TYPE with a warning, and had no such tolerance for an unknown FIELD.

**Rules for next time:**
- **A feature that makes an action easy is a claim about every path that action reaches**, not only the
  one you built. Ask what the newly-cheap action does to data that already exists — before shipping the
  thing that encourages it.
- **A generated serializer encodes a MIGRATION POLICY, and the strict one is usually the default.**
  `at()` versus `value()` is "adding a field is routine" versus "adding a field bricks every save". Read
  what codegen emits for the ABSENT case, not just the present one — [[L27]] (know *why* it works)
  applied to inputs the code has never seen.
- **Symmetry is a checklist.** Where a loader already tolerates one kind of mismatch, ask what it does
  with the neighbouring kinds (unknown field, wrong type, non-object). The tolerance was already written
  and reasoned one line above the gap.
- **Boot-path throws are a severity multiplier** — before a window exists, an escaping exception is a
  crash, not a broken file. That is why the fix was two-sided: defaults for the ordinary case, a catch
  plus a Warn for the corrupt one.

## L43 — Regenerating a tracked file is a MIGRATION, not a rebuild (2026-08-20)

**What happened (config format unification).** Moving configs onto the shared nlohmann macro changed
their key names, so the two `.config` files had to be regenerated: delete, boot a host, commit the
result. `Engine.config` came back identical in value — every field was at its default. `Renderer.config`
came back with `ClearColor` at the struct default, **black**, silently discarding the `0.1` dark grey
the user had set. Caught only because I diffed the regenerated file against `git show HEAD:` before
moving on, and carried the value across by hand.

**Why it is worth a lesson.** Every gate was green while the data was wrong. The build passed, the tests
passed, the host booted, the file was well-formed and matched the new schema perfectly — the only thing
that had changed was a value the user chose, replaced by one the code chose. "Regenerate it" sounds like
a build step and is actually the narrowest possible data migration, with no tooling and no diff review
unless someone asks for one.

**Rules for next time:**
- **Before regenerating any file under version control, diff the new one against what it replaced**, and
  account for every value that moved. `git show HEAD:<path>` costs one command.
- **Ask which values in that file a HUMAN chose.** Defaults regenerate perfectly and prove nothing; the
  customised value is the entire risk, and it is usually one line among thirty.
- Prefer the order *back up, regenerate, port, verify* over *regenerate and eyeball* — even when the file
  is tracked, because "it's in git" only helps someone who notices in the first place.
- Same family as [[L20]] (never spend unrecoverable user state on a test): this one WAS recoverable, and
  that is the only reason it is a lesson rather than an apology.

## L44 — A convenience seam with NO CALLER is not yet a seam; grep who calls it before assuming your extension took effect (2026-08-20)

**What happened (resource formats).** I added a third route to `ModuleRegistrar` and wired it inside
`BindEngineRegistries` — the one call **MR0** documents as existing so the binding *"does not grow an
argument per registry."* All three presets built, 374 tests passed. The first `Sandbox.exe` boot then
said:

```
[error] [ModuleRegistrar] Resources().Register — route is not bound to a ResourceFormatRegistry; registration dropped.
```

`OpaaxApplication::PopulateEngineRegistries` had never called it. It bound each route by hand, two lines,
and the aggregate helper had sat at **zero callers** since the day MR0 introduced it. So the new route
shipped unbound and dropped every game-module registration — the precise failure MR0's one-call design
exists to prevent, hiding inside the contract that describes the prevention.

**Why I missed it.** I read the contract, found the seam named there, edited it, and treated "the seam
now handles my route" as done. I never asked *who calls this*. A contract states intent; the call site
had drifted from it years of commits ago, and **a contract is not a grep**. This is [[L8]] one level up —
verify the mechanism *ran*, not that the mechanism *exists*.

**Rules for next time:**
- **When you extend a shared seam, grep its CALLERS before believing the extension does anything.**
  Editing the function everyone "uses" is worthless if nobody uses it. One `grep -rn` showed two
  hand-binds and zero uses.
- **Two ways to do one wiring means one of them is stale**, and the stale one is usually the documented
  one. Fix the cause — call the aggregate — rather than adding a third line to the hand-written list,
  or the next person to add a registry repeats this exactly.
- **The thing that saved it was the route's own unbound-Error**, written for [[L16]]'s reason. An
  "impossible" wiring branch that logs earns its lines the first time somebody adds a route; a silent
  `return false` would have made this a content bug reported days later, with nothing pointing at boot.
- Generalises past wiring: whenever a doc names a helper as *the* way to do something, confirm the tree
  agrees before relying on it. Same session, same file, `Engine::RegisterNativeTypes()` turned out never
  to have existed either.
## L45 — A generic UI system composes independent authors into ONE namespace; ask what the key is and who can collide in it (2026-08-20)

**What happened (④ textures, S3 — the USER caught it, not any gate of mine).** `SpriteComponent` was
written deliberately parallel to `DummyComponent`: same `Position`, same `Size`, same `Color`, because
they mean the same things. The moment one entity carried both, ImGui reported *"visible items with
conflicting ID"*. An ImGui widget's identity **is its label**; a label in the generic drawer is a
*property name*; and `InspectorPanel` stacks every applicable drawer into one window. So two `Color`
rows were one id, twice — the widgets shared hover and active state, and dragging one could drive the
other. The user named the general case in the same breath: *"since we have drawer for LinearColor,
(other will collide too)"*.

**The fix is a SCOPE, and its PLACEMENT is the lesson.** `ImGui::PushID(<drawn type name>)` per registry
**entry**, in `TDrawerRegistry` — not inside each `TPropertyDrawer`. A drawer sees one field and cannot
know what else the window holds; an entry is exactly the boundary between two independently-authored
types. One line covered components, configs, hand-written drawers and every future subject — including
a latent copy of the same bug in the **Config panel**, which had no scope of its own and would have
collided for two configs sharing a field name. Nobody had tried, because `EngineConfigData` and
`RendererConfigData` happen not to overlap.

**Why every automated gate was blind.** Three presets, 383 tests, two smoke runs, all green. The
conflict needs *an entity carrying two components with overlapping fields*, selected, with the pointer
over one of the rows — ImGui detects it on **hover** (`ItemHoverable`). No compile error, no log line,
and a boot smoke test never selects anything.

**Rules for next time:**
- **When a generic system composes independently-authored pieces into one namespace, name the KEY and
  ask who can collide in it.** Here the key was the field name. The tree already scopes every other
  such namespace — map component keys, config keys, command tags, menu ids — which is why this one
  stood out only in hindsight. The question is cheap and mechanical; ask it when writing the composer,
  not when a user hovers a row.
- **A new type modelled on an existing one INHERITS its field names, and the resemblance IS the hazard.**
  The more faithfully `SpriteComponent` mirrored `DummyComponent`, the more certain the collision. "It
  looks just like the one that already works" is a reason to check the shared namespace, not to relax.
- **An interactive gate needs the NEGATIVE question.** [[L12]] says the observability must exist and
  [[L15]] says it must discriminate; this adds: a handoff that lists what to *look at* still misses what
  to *look for going wrong*. My S3 handoff had five numbered steps and not one of them was "does
  anything complain?".
- **Scope by NAME, not by index**, when the id also keys persisted UI state — an index-based scope
  silently rebinds every stored header state the day a drawer is registered ahead of it.

## L46 — A dead-code grep whose FILTER matches call syntax proves the opposite of what it reports (2026-08-21)

**What happened (the cleanup sweep).** Asked to find unused code, I ran a whole-tree search for
`World::Clear` — and I did include `Engine/Tests` in the paths, the omission that had already cost
three sessions ([[L10]]). The command was:

```
rg -n "\bClear\s*\(" … Engine/Source Editor/Source Sandbox Engine/Tests | rg -v "\.Clear\(\)|…"
```

That second `rg -v` was a noise filter, meant to drop `m_Foo.Clear()` housekeeping. But a **call
site** is spelled `lWorld.Clear()`, which matches `\.Clear\(\)` — so the filter deleted precisely
the evidence the search existed to find. What survived was declarations and definitions only, which
reads exactly like "declared, never called." I reported `World::Clear()` as dead, the user approved
deleting it, and it had **five callers** (`WorldEntityTests` ×3, `MapSnapshotTests`,
`ModuleRegistrarTests`). The build caught it in one pass, and restoring it pulled back
`Level::OnWorldCleared` and `WorldGuidRegistry::Clear`, which had only looked dead because they
serve it.

**Why this is not just [[L10]] again.** L10's rule is *"grep is a SEED, the build is the VERDICT"*
and its failure mode has always been **too narrow a path list**. This was a correct path list and a
**self-defeating pattern** — a stricter search that was wrong in the one direction that matters,
producing a confident false positive rather than a miss. Widening the sweep would not have helped;
nothing about the output looked incomplete.

**And the grep was the SECOND wrong witness, not the first.** `CLAUDE.local.md`'s STILL OPEN list
already said *"`World::Clear()` has zero callers outside `World` (③ removed the last one)"* — so I
went in believing it and read the grep as confirmation. Two independent-looking sources agreed, and
they were not independent: that note was almost certainly written from the same kind of search. This
is [[L22]]/[[L26]] wearing a third hat — **a doc's claim is a premise, not a finding, and my OWN
notes are the easiest one to forget that about**, because I trust them like memory rather than like
a document that can go stale. When a note and a grep agree, ask whether the grep is the note's
source before counting it as a second opinion.

**Rules for next time:**
- **[[L21]] applies to a GREP, not only to a test.** The instrument must not share a failure mode
  with the thing it measures. A search for "is this called?" whose filter can match **call syntax**
  cannot answer that question. Before filtering a dead-code search, ask: *could this `-v` pattern
  match a real caller?* If yes, read the noise instead — it is cheaper than a wrong deletion.
- **For "is X used", grep for the USE, never for the declaration and then subtract.** `\.X\(` /
  `->X\(` / `::X\(` as the primary query, with the declaring file excluded by path. Counting all
  mentions and reasoning about the remainder is where a filter gets invented in the first place.
- **A deletion the USER approved on my evidence is worse than one I got wrong alone.** They answered
  "delete all 14" against a list I had verified badly, so my error consumed their decision too. When
  a proposal's whole value is the verification behind it, the verification is the deliverable —
  re-run it unfiltered before acting, not after the compiler objects.
- **Restore the whole reachability cluster, not the symbol.** `Level::OnWorldCleared`'s own doc said
  *"World::Clear already emptied us"* — a member that exists to serve one caller is dead or alive
  with it, in both directions ([[L10]]'s cluster rule, run in reverse).
- **The compiler stayed the honest gate, and it was cheap.** One `OPAAX_BUILD_FAIL` naming five
  files, ~9 minutes. Never close a delete-only change on grep evidence alone, however careful the
  grep looked — that is what a build is for ([[L8]]: grep the output, the exit code was 0 here too).

---

## L47 — A blocker I wrote down is a claim about PLACEMENT until proven otherwise; re-derive it before costing an invariant change (2026-08-24)

**What happened (④b, the texture preview).** Both `todo.md` and `CLAUDE.local.md` recorded the
preview as blocked on a contract question: `TPropertyDrawer::Draw(label, value, meta)` has no
`EditorContext` by design, so a drawer can reach neither the UI backend nor the ResourceManager, and
I had written *"Decide that first; it is the whole design question, and **I15** is the invariant it
touches."* I carried that into the next session and put it to the user as a fork — browser-only, or
amend **I15**. Their answer was neither: *"The preview is double click action, what do you think?"*
`ResourceTypeBuilder::SetActivate` has BEEN "what does a double-click do" since M2d, Map and Level
open documents through it, and a registered panel has an `EditorContext` by construction. **I15
never had to move.** The registration I was editing even said so: *"No activation: double-clicking
an image has nothing to open until a texture viewer exists."*

**Why I framed it wrong.** I asked *"how do I get a context into a drawer?"* — a real question with
only expensive answers — instead of *"where does a preview belong?"*. The blocker was genuine **for
the location I had already assumed**, and assuming the location is the step that never got examined.
Writing it down twice, months apart, laundered an assumption into a finding.

**Rules for next time:**
- **When a plan says "X is blocked on amending an invariant", re-derive WHY X is where it is before
  costing the amendment.** The invariant is usually load-bearing; the placement usually is not. Ask
  what already does this job elsewhere in the tree.
- **My own notes are the easiest premise to mistake for evidence** — [[L46]]'s second-wrong-witness
  shape, and [[L22]]'s. A carried-forward blocker deserves the same suspicion as a carried-forward
  test assertion, *especially* when I wrote it and have since restated it.
- The user reframes by asking **what does the user DO**, not what the code allows
  ([[justifies-design-from-authoring-cost]]). Twice now that has collapsed a design instead of
  growing one.

---

## L48 — Before a smoke run, name the log line that will PROVE the feature ran; absence of errors is not evidence (2026-08-24)

**What happened.** Browser type-icons loaded lazily, resolved on the first tile that needed one, with
a rationale I invented ("a handful of types, most never seen in a session"). Build clean, tests
green, smoke run clean, 0 err/warn. Then I grepped the log for `Icon loaded` and found **nothing** —
the browser opens at *Home*, where the tiles are the roots themselves and no **file** tile draws, so
the icon path had never once executed. Every green signal was real and none was about the feature.

**Why it nearly passed.** I checked the things that fail loudly and read the absence of failure as
success. A lazy path that is never entered logs exactly like a correct one.

**Rules for next time:**
- **Name the expected positive log line BEFORE the run, then grep for that line.** If no such line
  can exist, the run is not a verification of this change — it is a verification that nothing else
  broke, which is a different claim.
- **Laziness is a VERIFIABILITY cost, not only a performance choice.** Where the set is finite and
  known — a sealed registry, a fixed list — eager is simpler *and* self-proving, and it moves the
  failure to a known moment. Ask "what makes this set finite?" before choosing lazy.
- Same family as [[L23]]: *has this run in the real app?* is not answered by *did the real app run?*

---

## L49 — "Absent" and "placeholder" are different answers; gate on IsValid(), never on Get() != nullptr (2026-08-24)

**What happened.** The icon cache stored a `ResourceRef` even when the load failed, reasoning that
caching a failure prevents a per-frame retry — which `RendererManager::ResolveTexture` does, with a
comment saying exactly that. But `ResourceRef::Get()` is `manager->Resolve(handle)` →
`pool.Get(handle)` → `PlaceholderOrNull()`, so a **failed** claim answers the type's placeholder: for
a texture, the magenta 2×2. A missing icon file would have painted a magenta square precisely where
I had promised the glyph fallback. Found by reading `Resolve` while designing `Find` — not by
running anything, because every icon file happened to exist.

**Why it hid.** The placeholder is *correct and valuable* for its designed consumer — a sprite
drawing magenta is louder than a sprite drawing nothing. It is wrong for a consumer that has its own
fallback. And the comment I copied described the behaviour accurately; what did not transfer was the
**decision** behind it.

**Rules for next time:**
- **`Get()` non-null means DRAWABLE, not FOUND.** Wherever those differ to you, gate on `IsValid()`.
- **Copying a cache's shape copies its POLICY.** Before reusing an idiom that carries a comment, ask
  whether the sentence in that comment is still true of the new caller. "The empty ref resolves to
  the magenta placeholder" was the point at one call site and the bug at the other.
- When adding a query API, make the miss **structurally** honest rather than documented: `Find`
  returns a null-manager ref, so `Get()` is `nullptr` — the choice `Pin` had already made, which I
  only found by looking rather than by assuming.

---

## L50 — A PLACEMENT argument dies with the feature it rested on; re-derive it after every scope cut (2026-08-26)

**What happened (① camera).** I planned `CameraSubsystem` as a **world** subsystem and defended it at
length: `ShouldCreate` gives the Edit/Play split for free, camera state is world state, PIE keeps two
worlds alive. The user then cut follow and shake from the block and asked, in five words,
*"CameraSystem can be Engine no?"* — and they were right. Every one of my arguments rested on
**behaviour that ticks**. With follow gone there is no per-world behaviour and no per-world state at
all: a camera's position lives on its entity in the world's own registry, so the resolve is a pure
function of the active world. The world tier would have cost a new `RegisterNativeWorldSubsystems()`,
the first engine-native world subsystem, a `WorldContext` it barely touches, and an instance per world
including every test world — all to avoid one `if` on `GetMode()`.

**Why I did not catch it myself.** I made the cut and re-read the placement decision in the *same*
reply, and treated the placement as settled because I had written it down the day before. The plan
document had become an input rather than a claim to re-check. The tell was sitting in my own text: the
paragraph justifying the tier used the word *"ticks"*, and "ticks" had just been deleted from scope.

**Rules for next time:**
- **When scope is cut, grep your own plan for the cut feature's name.** Every hit is a decision whose
  justification just changed and has to be re-derived, not inherited.
- **A placement argument is a claim about STATE and LIFETIME, not about vocabulary.** "It is a camera,
  cameras belong to worlds" is a category feeling. "It holds nothing per world" is the real test, and
  it answers in seconds once it is actually asked.
- **Express the surviving argument as CODE.** The reason the engine tier is right is "the resolve needs
  only the world", so `Resolve` became a `static` pure function. That made the claim checkable instead
  of asserted — and incidentally made the positive branch unit-testable against a bare `World`, which
  a smoke log could never have covered.
- Sibling of [[L47]] from the other direction: L47 is about not trusting a written-down *obstacle*,
  this is about not trusting a written-down *decision*.

---

## L51 — Guard a "seed from a measured size" against the value BEFORE the first measurement (2026-08-26)

**What happened (① S3).** `EditorCamera::SeedFromViewportHeight` adopts the viewport's height as its
starting `OrthoSize`, once, so the editor opens on the framing it had before cameras existed. But
`ViewportPanel` starts at **1×1** and only learns its real size on the second frame — the
deferred-resize handshake it has had since M1. Seeded on frame one, the editor would have opened
zoomed into half a world unit: an empty-looking viewport, produced by a feature whose entire job is
*"nothing should look different"*. The guard is one clause, and the smoke log is what then proved the
right thing happened — `seeded from a 469px viewport — orthoSize 234.5`, not `from a 1px viewport`.

**Rules for next time:**
- **Before consuming a measured value, ask what it reads as BEFORE the first measurement.** Deferred
  handshakes are everywhere in this tree (viewport resize, hover/focus, the input route) and every one
  of them has a "not yet" value that is a **legal number**, not an obvious null.
- **A one-shot latch makes the not-yet case PERMANENT.** `if (m_bSeeded) return;` plus a bad first
  reading is not a one-frame glitch, it is the state for the whole session. One-shot code needs its
  input validated harder than per-frame code does.
- **Log the value you seeded FROM, not just that you seeded.** `from a 469px viewport` is what made
  this verifiable with no eyes on it; `seeded` alone would have been printed just as cheerfully by the
  broken version ([[L48]]).
