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
