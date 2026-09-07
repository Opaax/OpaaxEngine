# OpaaxEngine — Architecture Contract

> **What this file is.** The *positive* statement of the engine's load-bearing invariants — the rules
> I check every change against. `lessons.md` holds the *negative* record (mistakes + why). This holds
> the *contract*. Read both at session start.
>
> **Rule IDs (`I1`, `L1`, `S1`…) are citable.** In review I say "violates **I1**" instead of re-arguing it.
> If a change must break an invariant here, that is a STOP-and-re-plan trigger (CLAUDE.md §1) — we amend
> this file deliberately, we don't drift past it.
>
> **Scope:** the *new* engine (branch `refresh_engine`). The old world (`CoreEngineApp`, `*Old` classes,
> `EventOld/`) is dead-but-compiled — see **X1**. When code and this file disagree, the code wins *and you
> fix this file in the same change*.
>
> **This file holds *why*, never *how to*.** Step-by-step recipes live in `.claude/howto/` — currently
> [configs & drawers](howto/configs-and-drawers.md). Add a recipe there rather than growing this file.

---

## I — Prime invariants (non-negotiable)

**I1 — One static root.** `OpaaxApplication::m_Services` (a `static AppServiceLocator`) is the *only*
mutable static in the engine. Everything else is instance-owned beneath it: the locator owns the services
(`TUniquePtr<IAppService>`), the `IEngine` service owns the `EngineSubsystemMgr`, the manager owns the
subsystems. No `s_Data`, no function-local `static`, no singletons past the locator. This is the invariant
the whole design optimizes (see **L2**); a proposal that adds a static is wrong by default, even a "clean" one.
*The old static `RenderCommand`/`IRenderAPI` facade was **retired to `Legacy/RHI`** (2026-07-22, [[L14]]) —
the render path is now instance-owned via `IRHIDevice` (`RenderSystem::m_Device`), zero facade statics; do not resurrect it.*

**I2 — DLL-safe type identity.** A type's tag must be a *single instance across the DLL/exe boundary*.
Two proven ways to get that — the deciding factor is **whether the tag is dll-exported**, not inline-vs-`.cpp`:
- **Out-of-line (services):** `OPAAX_SERVICE_TYPE(IFoo)` *declares* `StaticTypeID()` in the header; the
  interface's `.cpp` *defines* it. One tag, shared. `ServiceTypeID = uintptr_t`.
- **Exported-inline (subsystems):** `OPAAX_SUBSYSTEM_TYPE` keeps `StaticTypeID()` inline (a function-local
  static). It is still single-instance *because the subsystem class is `OPAAX_API`* — dllimport imports the
  one exported inline definition into the exe rather than re-emitting it. **Proven by S9 (2026-07-20):**
  `GetSubsystem<WorldManager>()` is non-null across the exe/DLL line (DLL-side instance tag == exe-side
  `StaticTypeID()`, same address). So I2 is *not* "out-of-line only" — it is "one exported tag."
- The "duplicates per module" hazard (**L4**) bites a tag that **two modules both instantiate** — a
  header-only template static, or a type compiled into the DLL *and* the exe. The axis is **how many
  modules emit the tag**, not whether it is exported. If `GetSubsystem<T>()` across the boundary ever
  returns null, this is still the first suspect.
- **A NON-exported subsystem is fine when it lives in exactly one module — proven by the M4 S1 probe**
  (2026-07-29, `Engine/Tests/Core/World/WorldSubsystemIdentityTests.cpp`). This corrects an earlier,
  broader caveat here which claimed a subsystem in a **static lib / game module** "would get a per-module
  copy" and must go out-of-line or be exported. It does not: a game type is compiled into the **exe
  only**, so there is no second copy to disagree with, and the exe linker folds the per-TU COMDATs of the
  inline `StaticTypeID()` into one address. The probe pins it the way M4 needs — probes defined in a
  header used from **two** TUs (a single TU cannot tell "one tag per type" from "one tag per TU"), with
  external linkage (an anonymous namespace would have guaranteed the wrong answer, [[L21]]), registered
  in one TU and resolved in the other through a DLL-owned `WorldSubsystemMgr`. Structurally identical
  probe types get distinct tags, and an exe-side tag does not collide with `WorldManager`'s DLL-exported
  one. **So a game module needs no ceremony: derive, stamp `OPAAX_SUBSYSTEM_TYPE`, done.** Until this
  probe, every live user of that macro was an `OPAAX_API` engine subsystem, so the non-exported case had
  never once been exercised — the "S9 proof" above covers only the exported case ([[L21]]/[[L22]]: the
  contract's own claim was a premise, not a finding).
- **Exported-inline is for *tags*, NOT for shared mutable STATE.** S9 proved MSVC imports an exported
  inline function's local static for an identity tag — where the only requirement is address identity, and a
  wrong answer fails *loudly* (a null `GetSubsystem<T>()`). A function-local static holding a **mutable
  table** is a different risk class: if a module emits its own copy nothing crashes, the two tables simply
  disagree, silently and forever. State-bearing accessors therefore go **out-of-line in the DLL**, where one
  definition is a *link-time guarantee* rather than a compiler courtesy. Applied 2026-07-27 to
  `OpaaxStringID::GetPool()` (the intern table): pool + its entry points (interning ctor, `ToString`) moved
  into `OpaaxStringID.cpp`, and `OpaaxStringIDPool` reduced to a forward declaration so a consumer cannot
  even see its layout. Pool-free members (comparison, `GetId`, `IsValid`) stay inline. Had this drifted, the
  *same string* would intern to different `Uint32`s across the DLL line — and since every `OpaaxStringID`
  compare is an integer compare, it would have failed **silently**. Guarded by `Core/StringIDTests.cpp`.
- **Owning the pool was only half of it — what the pool DOES was never audited until 2026-08-12.**
  Three defects lived inside the correctly-placed table, and the DLL-safety argument above says nothing
  about any of them. **(a)** `Get` returned a `const OpaaxString&` into a `std::vector` and released its
  `shared_lock` *on return*, before the caller copied — so a concurrent `GetOrAdd` reallocation left the
  reference naming freed memory. Live, not theoretical: the resource loader interns paths on a worker
  (`ResourceManager.h`, *"may run on a worker"*) while the main thread logs the same names. **(b)** The
  pool ctor read `OpaaxGlobal::String_None`, then a *dynamically* initialised global — and the pool is
  built lazily on the first `OPAAX_ID(...)`, which `Renderer/RenderLayer.h`'s `g_RenderLayerIDs` already
  reaches during static init, inside the same DLL, where TU order is unspecified. **(c)** The pool was a
  function-local *object*, destroyed at exit, so any `ToString()` ordered after that read a dead table.
- **The fix is a STORAGE property, and it is what now licenses handing raw text across the DLL line.**
  The lookup map owns every string and the id→text array holds only pointers into it: `unordered_map`
  keeps element addresses across rehash, nothing is ever erased, and the pool itself is deliberately
  leaked, so **an entry's address is valid for the life of the process**. That is what makes
  `OpaaxStringID::CStr()` (a `const char*` into the pool) legal at all, and it also halves the table —
  the old shape stored every string twice, once as a vector element and once as a map key.
  `String_None` became `inline constexpr const char*` (see **I9**), which removes (b) outright.
- **The guard for this is `StringIDTests.cpp`'s ADDRESS-stability case, deliberately single-threaded.**
  A threaded test is the wrong instrument here and was *tried first*: the window between dropping the
  shared lock and copying is a few instructions, a vector reallocates only log2(n) times, and the
  concurrent case **passed against the broken storage** on every run. The deterministic case — take a
  `CStr()`, intern 4096 names, check the pointer still names the same text — fails it immediately. It
  uses a **short** name on purpose: a long one survives even broken storage, because a moved
  `OpaaxString` steals the heap pointer and `CStr()` keeps answering the same address by accident.
  Same lesson as [[L21]] — an instrument must not share a failure mode with the thing it measures.
- New cross-module identity must hash a compiler-stable per-type string (`__FUNCSIG__`), never a
  template-static counter.

**I3 — Resolution never returns null.** `AppServiceLocator::Get<T>()` returns `T::Null()` (an inert null
object), never `nullptr`. Every service interface provides `static T& Null()` + `IsNull()`. Callers may use
a service unconditionally; they check `IsNull()` only when absence is a *real* branch. Do not add
null-pointer checks around `Get<T>()`.

**I4 — Gregory layer split decides App vs Engine.** Before placing any system, name its layer in Gregory's
runtime diagram (Fig 1.16):
- **App service** = Platform-Independence + Core-Systems layers — *passive facilities* you submit-to/query:
  Platform, Paths, Logger, Config, ProjectManager, JobSystem, WindowManager, **Stats** (**ST2**).
  They do not tick. *Stats is the case that tests this rule: it is told `BeginFrame()` once per
  frame, which LOOKS like a tick and is a submission — the host says when, exactly as it does for
  input (**IN2**). Being driven per frame is not the same as ticking.*
- **Engine** = Resources/Assets layer *and up* — anything that ticks per frame or owns game concepts
  (Resources, Renderer, World, Physics, Input). Lives as an `EngineSubsystem`, never as an app service.
- Test: *ticks per frame or knows about textures/worlds* ⇒ engine. *Passive facility* ⇒ app service.
  (See **L1**.) `IEngine` is itself an app service — a deliberate exception that serves **I1**, not a
  counter-example to this rule.

**I5 — Ownership follows lifetime, top-down.** Four tiers, each owned by the one above, destroyed
bottom-up: `static locator → app services → IEngine → engine subsystems`. A subsystem never owns a
sibling; it *borrows* one by resolving from the manager (**F3**). Convenience pointers to siblings are
non-owning, initialised `= nullptr`, and re-resolved, never `delete`d.

**I6 — Never dll-export a class *template*; stateless value types are header-only.**
`__declspec(dllexport/dllimport)` (`OPAAX_API`) on a class *template* exports nothing — a template is not
code until instantiated, so marking it `dllimport` makes every consumer expect the instantiation *from the
DLL*, which never exports it → **LNK2019** (proven 2026-07-20: the `Angle` module linked from the editor exe
against the engine DLL). Two correct shapes:
- **Header-only value template** — `TFloatValue`, `TDegree`/`TRadian`/`TAngle`, math vectors: **no
  `OPAAX_API`.** Instantiated per-TU; DLL-safe *by construction* — no shared state, no vtable, no identity
  tag to unify (the opposite end of the axis from **I2**: identity/export matter only for types with shared
  state or a cross-module tag; a stateless value has neither). Out-of-line member defs go in a `.inl` that
  is **`#include`d where the type is declared**, or they're unresolved too.
- **Explicit instantiation** (only to hide a body / cut bloat): `template class OPAAX_API TFoo<float>;` in a
  DLL `.cpp` + `extern template class OPAAX_API TFoo<float>;` in the header. Overkill for a solo 2D engine's
  small value types (project value **Simple**) — prefer header-only.
`OPAAX_API` belongs on **non-template** classes with real compiled members (services, the `Maths` static
struct, `Engine`), never on the template itself.
- **Second proof, and a latent violation fixed (M4 S1, 2026-07-29): `ISubsystemManager<T>` was exported.**
  Its *member templates* (`RegisterSubsystem<T>`, `GetSubsystem<T>`) linked fine — MSVC always instantiates
  those locally — but its **non-template** members (`StartupAll`, `ShutdownAll`, …) were `dllimport`, so a
  consumer expected them from the DLL, which never exported that instantiation → **LNK2019**. This hid for
  as long as only `Engine.cpp` (*inside* the DLL) drove a manager; the first outside caller surfaced it.
  Fix per this invariant: drop `OPAAX_API` from the template (every member is inline in the header, no
  static, no identity tag — the header-only shape) and keep it on the **derived** `EngineSubsystemMgr` /
  `WorldSubsystemMgr`, which are non-template classes. No C4275, no explicit instantiation needed.
- **The tell for this class of bug: a template whose exported-ness is only tested from inside the DLL.**
  An `OPAAX_API` template compiles and links indefinitely while every caller is DLL-internal, because
  dllexport-side instantiation is what the DLL does anyway. It fails the day something outside calls a
  non-template member. When exporting reaches a template, ask *who will call this from the exe* — the
  answer "nobody yet" is how the defect stays latent.
- **It is not only templates, and it has now bitten THREE TIMES — treat it as a checklist item, not a
  hazard to remember** (2026-08-19/20). Each time the symbol was fine for months and broke the moment
  something outside the DLL named it, and each time the *trigger* was a feature that made an engine type
  reachable from a new module rather than any change to the symbol itself:
  1. `ISubsystemManager<T>` — an exported template whose non-template members the first outside caller
     could not resolve (M4 S1, above).
  2. `Config_Renderer` — `DECLARE_T_CONFIG` (unexported) while `IMPL_T_CONFIG` defines `StaticTypeID()`
     in a DLL `.cpp`; the editor registering a drawer for it was the first exe-side mention.
  3. `ToString(EBackend)` / `ResolveSupportedBackend` — DLL-internal free functions, until
     `EngineConfigData` held a real `EBackend` and **every TU that serializes a config called them**,
     tests included.
  **The mechanical check: a symbol becomes exe-reachable the moment it is named in a header the exe
  includes — INCLUDING indirectly, through a member's serializer or a template it instantiates.** So
  when a field changes type, or a type gains a member, ask what that drags across the boundary. The
  answer arrives as `LNK2019` at the worst moment otherwise, which is cheap but always a surprise.
  4. **`MakeViewProjection` / `ScreenToWorld` (① 2026-08-26) — the first one paid AHEAD of the break.**
     Two free functions in `Renderer/CameraView.h`, exported the day they were written because the
     question above was asked *while* writing them: the editor camera calls `ScreenToWorld` from
     `SandboxEditor.exe`, so they are exe-reachable by design rather than by accident. The checklist
     works; the trick is running it at authoring time instead of at link time. *(They are also
     out-of-line for a second reason — inline bodies would drag
     `glm/gtc/matrix_transform.hpp` into every TU that includes `World.h`.)*
  5. **`AnimationClipData::TotalTicks` (⑥ S3, 2026-09-03) — the FIFTH strike, and the cheapest tell
     yet: the sibling had already answered it.** A MEMBER defined in the DLL's `.cpp` on a plain
     data struct with no `OPAAX_API`; the test exe could not resolve it. `SpriteSheetData` — the
     type being mirrored, open in the next tab — keeps `FrameCount` and `FrameAt` inline for exactly
     this reason and exports only its free functions. **So the check has a cheaper form than the
     question in strike 3: when copying a type's SHAPE, copy where its members LIVE.** A plain
     aggregate's members go inline in the header, full stop; `OPAAX_API` is for the free functions
     beside it (`MakeFrameUV`, `SliceGrid`, `SampleClip`).
- **The MIRROR-IMAGE strike, and it fails WORSE (2026-09-01): `OPAAX_API` on a type the engine does not
  compile.** `Config_EditorImgui` — the tree's first non-DLL config, the decision **GIZ10** left open —
  was declared with `DECLARE_OPAAX_T_CONFIG` in `OpaaxEditorLib`, where `OPAAX_API` is `dllimport`. That
  points an import declaration at a definition the *same module* supplies, and MSVC answers with
  **warning C4273 and keeps going**, so it builds green until a second TU calls it and the linker asks
  for `__imp_`. Every strike above announces itself as `LNK2019` *where the mistake is*; this one warns
  in one file and breaks in another. **So exported-ness is a question with two wrong answers, and the
  axis is WHICH MODULE COMPILES THE TYPE** — engine → `OPAAX_API`, editor lib or game module → none
  (static libs folded into one exe, so **I2**'s one-module case: the tag is emitted once). Proven both
  ways: the editor config registers, draws and round-trips `Configs/EditorImgui.config`; a throwaway
  `DECLARE_T_CONFIG` probe in `SandboxModule` named from a `SandboxRuntime` TU linked to **one**
  out-of-line tag. Costed **nothing to prevent and was left to chance**: `DECLARE_T_CONFIG` had existed
  since the codec rewrite with **zero users** and a comment reading "if DLL-internal" — which names the
  engine's inside, i.e. the one place it is *not* for.
- **Corollary (M3): `OPAAX_API` instantiates every IMPLICITLY-declared member**, so an exported class
  holding a move-only member (`TDynArray<TUniquePtr<T>>`) fails to compile on its implicit *copy*-assign
  (C2280) even though nothing ever copies one. Declaring copy/move `= delete` is therefore **required**,
  not hygiene — the shape `World` already uses, now also `ComponentRegistry`.

**I8 — A component is defined by a CONCEPT, and there is NO component base class** (landed M3,
2026-07-28). `CComponent` (`World/Components/ComponentConcept.hpp`) requires nlohmann
`to_json`/`from_json` by ADL — the same concept-over-base-class shape as `CResource`, and the only shape
available: **entt stores components by value**, so `Save`/`Load` cannot be member virtuals the way the
retired `Legacy/ECS/ComponentRegistry` did it. A game component costs one
`NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` and one `Components().Register<T>()`; the engine names it
nowhere.
- **`_WITH_DEFAULT` is REQUIRED, not a preference** (2026-08-19, from a live crash). The plain macro
  reads every field with `at()`, which **throws** on a missing key — so the first field added to an
  existing component refuses every map already saved, and it does it inside `Level::MountAll` during
  `FinishStartup`, i.e. **at boot, with nothing above it catching**. `_WITH_DEFAULT` keeps the
  default-constructed value for an absent key, which makes "add a field" the backward-compatible
  change it appears to be. Adding a field is the single most ordinary edit a component gets; a format
  where it is a hard failure is the wrong default, and the macro name was the whole difference.
- **What defaults cannot cover, `MapFactory::Instantiate` catches**: a wrong-TYPED value or a payload
  that is not an object (hand-edited, truncated). It warns per component and leaves it at its defaults,
  the same "skip and keep going" the unknown-component-name branch beside it already did — **BO4c**'s
  rule one level down. Pinned by two cases in `MapSnapshotTests.cpp`, both of which fail against the
  plain macro with the exact exceptions the crash produced (`out_of_range.403`, `type_error.304`).
The empty `ComponentBase`/`IComponent` markers were **deleted** once the concept took over their stated
job (user call): an empty non-virtual base is an attractive nuisance — the first person to add a virtual
to it silently breaks by-value storage and nothing complains. **Do not reintroduce one.** Per-entity
behavior is authoring data + a world subsystem (D7), never a polymorphic component.
`EntityMeta` deliberately does **not** satisfy it — identity is not user data, and the snapshot core writes
those fields by hand rather than nesting an entity's identity inside its own payload.

**I7 — Every string the engine carries is UTF-8; conversion happens at the PLATFORM boundary** (landed
2026-07-28). `OpaaxString` is a byte container with no encoding of its own, so the encoding is a
*convention* — and the platform layer already fixed it by converting `GetExecutablePath` through
`CP_UTF8`. Therefore any code that hands an engine string to an OS or CRT entry point must convert
explicitly, because the narrow entry points do **not** assume UTF-8:
- On MSVC, `std::filesystem::path(const char*)` and every `*A` Win32 function decode using the **ANSI
  code page**. Feeding them UTF-8 resolves a *different file than the caller named*, with no error
  anywhere — `IsPathExist` answering false for a directory that exists (proven, `WindowsFileSystem`).
- **The conversion is `Core/String/OpaaxUtf8.h`** (`Opaax::Utf8::ToFsPath` / `FromFsPath`, plus
  Windows-only `ToWide`/`FromWide`). It lives in **Core**, not in the platform folder, because the layers
  that need it — Core's config IO, the portable `Renderer`'s shader loading — sit *below* Application and
  cannot reach `IPlatform`. **Never build an `fs::path` or open an `fstream` from `OpaaxString::CStr()`;**
  `std::fstream` takes an `fs::path` (C++17), so obeying this is a one-line call. Interfaces above the
  platform state UTF-8 in their contract and never see a `wchar_t`.
- **`IFileSystem` is one CONSUMER of this rule, not the only legal way to touch a file.** It is reachable
  only through the service locator, which Core and Renderer must not use; forcing them through it would
  mean injecting a filesystem into `TConfig` and the `CResource` contract. The encoding is the invariant;
  the facility is not.
- **This fails silently and symmetrically, which is why it needs an invariant rather than care.** A
  mis-encoded write followed by a mis-encoded read agrees with itself; only checking against the OS's
  *wide* API reveals the truth. Tests that pin encoding must use `\uXXXX` escapes, not literal
  characters — an instrument must not share a failure mode with the thing it measures ([[L21]]).
- **The build sets `/utf-8`, so the SOURCE side of this is settled by the compiler** (root
  `CMakeLists.txt`, 2026-08-05). *This corrects an earlier claim here that "the build sets no `/utf-8`
  and the sources carry no BOM" — the second half was simply false: 48 compiled files carried a BOM
  and ~150 did not, so MSVC was decoding the BOM'd ones as UTF-8 and the rest as the ANSI code page.
  Two encodings in one target is exactly what this invariant exists to forbid, and the flag was the
  one-line fix.* `/utf-8` sets **both** charsets, so a literal now means the same thing in every file
  and reaches the binary as UTF-8 regardless of BOM.
  - It was **byte-neutral** to apply, which is why it was safe: all 103 non-ASCII literals in the tree
    are U+2014 in *non-BOM* files, where the UTF-8 bytes were already passing through un-decoded
    (mojibake in, mojibake out). Verified across all three presets — zero C4819, tests 262/1257/2, both
    hosts clean, and the em dash confirmed as `E2 80 94` in the smoke log.
  - **The flag does not retire the rule.** It fixes what the *compiler* reads; the `*A`/`fs::path`
    hazard above is about what the **OS** reads and is untouched. `OpaaxUtf8.h` stays mandatory.

**I9 — A constant lives with the CONTRACT that owns it; a shared file is only for constants NOBODY
owns** (settled 2026-08-05, rejecting a proposed central `OpaaxStatics.h`). Two shapes, both already in
the tree and both correct:
- **Owned by one contract → its header.** `MAX_FRAME_DELTA` in `Engine.h` (**F1**'s clamp),
  `NULL_LEVEL_WORLD_NAME` beside `WorldSpec` (**BO4c**), `ENTITY_NONE` in `EntityTypes.h`,
  `LEVEL_EXTENSION`/`KEY_*` in `LevelFile.h`. Anyone who needs the constant already includes the header.
- **Shared domain, owned by nobody → a statics file.** `Core/Maths/MathsStatics.h`. Everyone needs PI,
  no module owns it, and it never changes — so the "one header rebuilds the tree" cost is nil.

`Engine::Loop`'s fixed step shows the split working on one line: the timestep is `D60_HZ` from
`MathsStatics` (a generic *ratio*), the clamp is `MAX_FRAME_DELTA` from `Engine.h` (a *policy*).
**Graduation trigger:** two unrelated modules need it and neither owns it — until then, one home.
Centralizing instead would **invert layering** (Core would hold a world-naming policy — **I4**) and
**group by storage kind rather than meaning** (the [[L30]] shape: `NULL_LEVEL_WORLD_NAME` next to
`MAX_FRAME_DELTA` tells a reader nothing). That decentralized form already scales here: `LogCategory` is
a constant, there are ~10 of them declared per-header via `OPAAX_LOG_CATEGORY`, and nobody has ever
wanted them in one file.
- **`OpaaxGlobal` is a NAMESPACE of constants, not a struct of statics** (2026-08-05). It held two
  out-of-line `static const` members, so `ID_None`'s *value* was invisible to consumers and
  `OpaaxStringID`'s default ctor could not be `constexpr` while its `operator==` was. `ID_None` is now
  `inline constexpr` (header-only, no export, no `.cpp`) and the ctors / `GetId` / `IsValid` are
  `constexpr`.
- **`String_None` followed it on 2026-08-12, and the reason it had not is worth keeping.** This bullet
  used to say it "stays out-of-line — an `OpaaxString` is not a constant expression". True of the
  *OpaaxString*, and irrelevant: the constant is a **name**, and `inline constexpr const char*` states
  it perfectly. The out-of-line form was not merely heavier, it was **unsafe** — a dynamically
  initialised global read from the intern pool's constructor, which the tree already reaches during
  static init (**I2**). `OpaaxGlobal.cpp` is deleted; the file existed only to hold that one line.
  **The general shape: "X is not a constant expression" is a claim about the TYPE you reached for, not
  about the constant.** Ask what the constant *is* before concluding it needs a `.cpp`.

**I10 — An engine type keeps its `Opaax` prefix; do not alias it away** (settled 2026-08-05, deleting
`Core/OpaaxForward.hpp`). Note which direction the aliases in `OpaaxTypes.h` run: `TDynArray`,
`TUniquePtr`, `Mutex` take a **std** type and *add* engine identity. `using String = OpaaxString` ran the
other way — it *stripped* identity from a type that already had it, so a reader seeing `String` had to
know the alias existed, which is the exact clarity the prefix buys. `OpaaxTypes.h`'s own header comment
already named `OpaaxString` canonical, and `OpaaxHash.h` has a parameter literally named `String` inside
`namespace Opaax` — a shadow that stayed harmless only because one file included the alias.
- That file was also **not a forward header**: it `#include`d the full definition, so it forward-declared
  nothing and broke no cycle, despite saying so. **A real forward header is `class Foo;` with NO
  includes.** The tree's ~175 *local* forward-decl lines (e.g. `WorldManager.h`'s four siblings) are the
  better pattern — precise, self-documenting, nothing to keep in sync. Reach for a per-module `…Fwd.h`
  only when a specific header's include cost shows up in build times, never preemptively.

**I11 — One spelling for text: an ENUM gets a free `ToString(E)` found by ADL, a VALUE TYPE gets a
member `ToString()`** (settled 2026-08-06). The tree had three spellings for one idea — `ToString`,
`BackendToString`, `WindowModeToString` — so a user could not *guess* the name, which is the whole cost.
- **Enum → free `const char* ToString(E)`, declared in the enum's own header.** No allocation, no prefix,
  found by ADL from any layer that can see the enum. `EWorldMode`, `EPlayState`, `EInputRouteState`
  already had it; `EBackend` and `EWindowMode` were renamed into it.
- **Value type → member `OpaaxString ToString()`** — `OpaaxStringID`, `Guid`. Returns by value, so it is
  a member; the free form is for the zero-cost enum label.
- `ToStringID(ERenderLayer)` keeps its own name: it answers the canonical `OpaaxStringID` used for
  *lookup*, not a log label.
- **A central `ToString` header is not merely unwise — it cannot exist.** One file covering `EWorldMode`
  (Application), `EBackend` (RHI) and `EPlayState` (Editor) must include all three, and Core cannot see
  Application, nor the engine the editor (**MR1**). Same shape as **I9**'s rejected `OpaaxStatics.h`:
  grouping by *storage kind* rather than by meaning. Discovery comes from the uniform overload set, which
  is also **layer-correct** — it offers a TU exactly what that TU may legally reach.
- **The PARSE direction IS unified now, and the reason it was not is worth keeping** (corrected
  2026-08-20). This said `FromString` carries a per-enum fallback **policy** — `BackendFromString` →
  OpenGL + Warn, `WindowModeFromString` → Windowed + Warn — and therefore could not be one overload set.
  What that missed is that **the two halves were different things sharing a function**: `BackendFromString`
  parsed a string *and* decided Vulkan is unavailable. Only the parse was per-enum boilerplate.
  - `OPAAX_ENUM_VALUES(E, …)` (`Core/Reflection/OpaaxEnum.h`) declares the enumerators as data, which
    C++20 cannot derive, and **the list is the parser**: matching a label against `ToString` of each
    value. So `WindowModeFromString` is deleted, and the availability policy survives under its own
    honest name, `ResolveSupportedBackend(EBackend)`, keeping its Warn.
  - The *fallback* is gone with it, deliberately. An unknown label **throws**, which `TConfig::Load`
    turns into "defaults kept, `false` returned" and `ConfigSystem` into a Warn naming the file
    (**BO1b**) — the same event as any other unreadable value, instead of a silent correction. What
    made that affordable is that the field became a real enum, so the editor's dropdown cannot author
    a bad one.
  - **Making a field a real enum EXPORTS its `ToString`.** `ToString(EBackend)` had been called only
    from inside the DLL; the moment `EngineConfigData` held an `EBackend`, every TU that serializes a
    config called it, and the tests failed to link. That is **I6**'s tell again — exported-ness only
    ever exercised from one side. Both it and `ResolveSupportedBackend` are `OPAAX_API` now.
- The mapping lives with the enum, **full stop** (the exception is retired, 2026-08-20).
  `ToString(EWindowMode)` sat in `IWindowManager.h` because "an unknown mode must be loud and Core does
  not log" — true of `WindowModeFromString`, which logged, and never of `ToString`, which is total and
  silent and was only carried along. With the parse generic it moved home to `Core/Window/Window.h`,
  beside its enum and its value list.
- **A `CStringable` concept + a constrained fmt formatter** — so `OPAAX_LOG(Cat, Info, "{}", lMode)` needs
  no explicit call — is the natural next step and is **named here, deliberately not built**: nothing
  constrains on it yet, and this is the `CComponent`/`CResource` shape (**I8**), so it costs nothing to
  add later. Trigger: the second place that wants to format an engine enum generically.

**I12 — Dev-vs-ship and editor-vs-game are TWO axes, one CMake flag each** (settled 2026-08-06).
- `OPAAX_DEV_BUILD` answers *where do assets resolve from*. ON bakes `OPAAX_WORKSPACE_DIR`, so
  `Paths` (`IPaths.cpp`, `#if defined(OPAAX_WORKSPACE_DIR)`) resolves against the **source tree** and no
  deploy runs. OFF bakes nothing, so resolution falls back to the **exe dir** — which is why the ship
  deploy (`Sandbox/CMakeLists.txt`) is gated on exactly the same flag. Never bake a build-host path into
  a shipped binary.
- `OPAAX_EDITOR_SUPPORT` answers *does the editor exist* — `OpaaxEditorLib` + `<Name>Editor.exe`, and
  nothing else (**D4**: the engine DLL is always `OPAAX_WITH_EDITOR=0`).
- **Editor implies dev; dev does not imply editor.** The root `CMakeLists.txt` `FATAL_ERROR`s on
  `EDITOR AND NOT DEV` (an editor resolving to its own bin dir would edit the deploy copy). The other
  three-quarters of the matrix is the point: `{dev, no editor}` is a debuggable `Game.exe`, the build
  **F4** already assumes exists when it calls DebugDraw engine-owned.
- One preset per useful combination, one app each — `debug-editor` → `SandboxEditor.exe`,
  `debug` → `Sandbox.exe`, `release` → `Sandbox.exe`. `VS_STARTUP_PROJECT` (root) and
  `build.bat run [preset]` both derive that app from the same flag, so F5 and the script agree.
- These flags gate **build composition only**. Behaviour that differs per *configuration* keys off
  `$<CONFIG>` (as `OPAAX_DEBUG` does) — never off `CMAKE_BUILD_TYPE`, which is meaningless under the
  multi-config VS generator.

**I13 — Text has THREE forms and one currency: `OpaaxString` owns, `OpaaxStringView` borrows,
`OpaaxStringID` identifies** (landed 2026-08-13). The view is a `{const char*, Uint32}` value —
header-only and stateless, so **no `OPAAX_API`** (**I6**'s first bullet, the call `Opaax::Utf8` already
makes). It speaks the engine's vocabulary (`Uint32` lengths, `Int32`/`-1` from `Find`, the same member
names `OpaaxString` uses), which is the point: `std::string_view` survives only as a **boundary**
conversion for the vendors that demand it (fmt, nlohmann, entt's `type_name`).
- **It is not null-terminated, so it deliberately has no `CStr()`.** A view is usually a SLICE, so the
  byte past its end belongs to somebody else — no member calls `strlen`/`strcmp`/`strstr`, and anything
  needing a terminator goes through `ToString()`. This is the whole reason the type is hand-rolled
  rather than aliased: `CStr()` on a view is the one mistake that would compile, and it cannot be made
  here. `StringViewTests.cpp` pins it with a slice whose buffer continues (`"hello world"` cut to
  `"hello"` must answer `Find("world") == -1`) and a `char[3]` with no terminator anywhere — a
  `strstr`-based implementation passes the easy assertions and fails exactly those two ([[L21]]).
- **The bridge is one implicit conversion, in one direction.** `OpaaxString::operator OpaaxStringView()`
  is implicit, so a view-taking function accepts a string, a literal or a `const char*` with no
  call-site noise; the reverse (`OpaaxString(OpaaxStringView)`) is **explicit**, for the reason the
  `std::string_view` ctor beside it already gives. `OpaaxStringView::ToString()` is declared in the
  view's header and *defined* in `OpaaxString.hpp` — it returns by value so it needs the complete type,
  the same reason `std::hash<OpaaxString>` lives in `OpaaxHash.h`. `std::hash<OpaaxStringView>` joins it
  there and agrees with the owning string's hash byte-for-byte, so a view can look up a string's key.
- **`OpaaxStringID` interning still takes an `OpaaxString`** — a view ctor would have to be out-of-line
  in the DLL (**I2**) and no caller needs one yet. One line when one appears.
- **`PathString::Stem` (`Core/String/OpaaxPathString.h`) is now the ONE path-stem rule.** It had been
  copied into `MapFile::StemId` and `LevelFile`'s `FileStem`, each first copying the path into a
  `std::string` — while `MapFile.h`'s own doc claimed `StemId` was public *"because a second copy of a
  naming rule is how two of them drift."* That claim is true now. Byte-wise on `/`, `\` and `.` because
  they are ASCII and an `fs::path` would decode them as ANSI (**I7**); it returns a view INTO its
  argument and allocates nothing. Path *composition* stays `IPaths`' job — this header is text.

**I14 — A TAG's hierarchy is DERIVED from its text; there is no tag registry** (landed 2026-08-13).
`OpaaxTag` (`Core/Tag/OpaaxTag.h`) is Unreal's `FGameplayTag` minus the one part that needs shared
mutable state: a 4-byte value holding one interned `OpaaxStringID`, where `A.MatchesTag(B)` is
`A == B` **or** *`A`'s text starts with `B`'s and the next byte is `'.'`*. That single prefix test
**is** the hierarchy — no declaration table, no ini file, no boot ordering, and **no second mutable
static** (**I1**). Exact compare stays one integer compare; a hierarchical compare is one `memcmp`
over pool bytes that are immortal and address-stable (**I2**), which is also what licenses
`OpaaxStringID::GetView()` — added here, in the id→**view** direction, so a match does not put a
`strlen` in front of every prefix compare (**I13**'s "one line when a caller appears"; the
view→id ctor still has no caller).
- **The price is named, not hidden: a MISSPELLED tag is a valid tag that matches nothing**, and
  there is no editor dropdown. *Structural* typos are caught — `IsValidTagText` is `constexpr` and
  refuses empty text, a leading/trailing `'.'`, an empty segment (`"A..B"`) and any byte `<= ' '` —
  and those rules are load-bearing rather than tidy: an empty segment yields a parent that is not a
  prefix of its own child. Malformed text asserts (Core cannot log, **I11**), which is the right
  trade for a hand-written literal and the wrong one for a file or a text field — so **untrusted
  input gates on `IsValidTagText` first**, which `from_json` and the Inspector drawer both do.
- **The invalid tag is inert in BOTH directions, and that needs an explicit guard.** `ID_None`
  resolves to the pool *text* `"None"`, so without it `OpaaxTag("None.Thing").MatchesTag(OpaaxTag())`
  answers true. For the same reason `GetView()` returns an **empty** view for an invalid tag while
  `ToString()` still prints `"None"`: text surgery must not see the placeholder, a log line must.
- **`OpaaxTagContainer` stores exactly what was added** — parents are implied by the match rule,
  never expanded into storage the way Unreal does it. A linear scan over a handful of tags beats the
  bookkeeping at this engine's scale, and it keeps "what I read back is what I put in" true, so
  `Add`/`Remove` cannot drift. `HasAny` on an empty query is false, `HasAll` is true.
- **Trigger for adding a registry later: an editor tag-picker, or the first typo that costs real
  time.** Its home is already decided — an `EngineRegistries` member (**MR0**), sealed at the first
  world like `ComponentRegistry`, **never a static**. It would *validate* tags, not define them, so
  `OpaaxTag` itself would not change. Both headers are header-only value types with **no
  `OPAAX_API`** (**I6**), and the nlohmann bridge is split into `OpaaxTagJson.h` the way
  `MathsJson.hpp` is split from `MathTypes.h`, so matching costs no json.

**I15 — A type DESCRIBES its fields as data; the editor draws them; per-type dispatch is a
SPECIALIZATION, never a registry** (landed 2026-08-19). `OPAAX_PROPERTIES`
(`Core/Reflection/OpaaxProperty.h`) declares a `static constexpr` tuple of
`{name, member pointer, hint}`. The **member pointer carries the type**, so a field states its name
once and never its type — which is what keeps new field types out of the engine: supporting one is an
editor-side `TPropertyDrawer<T>`, never a new `FLOAT_PROP`/`INT_PROP` macro here.
- **It is NOT editor-gated, and that is not a preference.** A component header compiles into the
  engine DLL (`OPAAX_WITH_EDITOR=0`) *and* into the editor exe (`=1`, **I12**/D4), so an `#if` around
  the block is one type with two definitions in one program. It also costs a shipped binary nothing —
  an unreferenced `constexpr` table is never emitted — so there is nothing an `#if` would buy. The
  real constraint is the other one: Core cannot see ImGui, so the macro emits **data, never widgets**.
- **The primary `TPropertyDrawer<T>` is DECLARED AND NEVER DEFINED** — `TConfigCodec`'s trade
  (**I9**'s neighbour in `TConfig.hpp`), and here it is load-bearing rather than tidy. A missing
  drawer is a compile error at the `Register<T>()` line; a specialization visible to one TU and not
  another therefore **cannot** instantiate the fold two different ways, which is what a silent ODR
  break would look like. A runtime registry would buy only "a drawer for a type you cannot include",
  and would cost a lookup per field per frame plus something to seal (**I1**).
- **`Drawers().Register<T>()` is the generic form of the call that already existed**, not a second
  route: `Register<T, TDrawer>()` stays as the override for fields that need judgment (a tag picker).
  The generic header's label is `DeriveTypeLeafName<T>()` — the same function that produced the
  component's key in a `.opaaxmap`, so the Inspector and the file agree by construction, not by care.
- **`EPropertyHint` exists because a type is not always a widget**: a `Vector4F` is four numbers or an
  RGBA colour and only the author knows. One value today, one caller — a growth point, not a taxonomy.
- **A DRAWER NAMES NO BACKEND** (2026-09-01, amending this rule's stated contract). The signature is
  `Draw(IEditorWidgets&, const char* InLabel, T& InValue, const PropertyMeta&)` — the seam comes
  first. It changed because *"supporting one is an editor-side `TPropertyDrawer<T>`"* is a promise to
  **games**, and a drawer that called ImGui directly made that promise backend-bound. Everything else
  here is untouched: still a specialization, still declared-and-never-defined, still no registry.
  See **MR2h** for the vocabulary and for why it is not simply **MR2d** being violated.
- **The engine's own components register their drawers in `EditorService::RegisterNativeDrawers()`**
  (2026-09-01). They were registered by the GAME module, so a fresh project had a blank Inspector for
  every engine type until it remembered four it does not own. Drawers were the last native route
  without a `RegisterNativeX()`. No drawer code exists for any of them — all four are `CReflected`,
  so the registration IS the implementation.
- **Properties do NOT drive serialization yet.** `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` still
  owns `to_json`/`from_json` (**I8**), so a type states its fields twice. Deliberate: map files are
  byte-exact-verified (**MP6**), so collapsing the two lists is its own step with its own gate.
  **Trigger: the first field added to one list and forgotten in the other.**
- **A field whose type is itself `CReflected` is a GROUP, not a widget** (2026-08-20). `DrawProperty`
  and `DrawProperties` are mutually recursive, so one declaration produces the nesting in the file,
  in the C++ and in the UI at once — which is what makes a config like `EngineConfigData` (nested
  settings structs) drawable at all: `TPropertyDrawer<WindowSettings>` could never sensibly exist.
- **Facets carry BEHAVIOUR, never presentation** (2026-08-20, user call). The first hint was
  `EPropertyHint::Color`, i.e. "draw this `Vector4F` as a colour" — a weaker version of a type, and
  it was **deleted** for `LinearColor` (`Core/Color/`), which dispatches by type like everything
  else. What a type genuinely cannot state lives in `PropertyMeta`: `SetRange(min, max)` (an unset
  range is `Min == Max`, which every ImGui drag already reads as unbounded, so the ordinary property
  needs no flag) and `SetFlags(EPropertyFlags::NeedRestart)`. The restart flag sits on the **group**
  — one marker on `Window`, not fourteen — and it replaced a blanket "changes apply on restart"
  sentence on the panel, which would have gone stale the day one value became live.
  - **`SetTooltip(text)` (2026-09-02) is the third facet and the one that is NEITHER**, so it is
    worth saying why it does not break the rule: it selects no widget and cannot be derived from the
    type, because it is authored English about what the field MEANS. The trigger was the user asking
    what `MaxQuadsPerBatch` was for — **a question about a field is a missing tooltip**, and
    answering it in chat leaves the next reader to ask again.
  - **It is drawn by `DrawPropertyNote`, beside `(restart)`, and the PLACEMENT is again the rule.**
    A `TPropertyDrawer` sees one value and one label, so a tooltip threaded through the drawers
    would have to be re-remembered by every game that adds a field type; in the fold, every described
    property gets one — generic, hand-written and grouped alike. The same argument that put `PushId`
    on the registry entry rather than inside each drawer. The widget it uses is **MR2h**'s
    `HelpMarker`, an item of its own rather than a decoration of the previous one.
- **An enum gets a dropdown from ONE constrained partial specialization** (2026-08-20).
  `TPropertyDrawer<CEnumWithValues T>` walks `TEnumValues<T>::Values` in a combo, labelling each with
  **I11**'s `ToString` — so a new enum field is a dropdown the moment its enum stamps
  `OPAAX_ENUM_VALUES`, with nothing per-type on the editor side. It is also what retired the last
  stringly-typed config fields: `Window.Mode` and `Render.Backend` are `EWindowMode`/`EBackend`, a typo
  is no longer representable, and the file did not move because an enum is written as its label.
- **ONE registry serves every subject: `TDrawerRegistry<TSubject>`** (2026-08-20). Components and
  configs were the same thing twice — a list of "are you applicable, and if so draw yourself"
  closures — so the difference collapsed into one customization point, `TDrawerResolver<TSubject,
  TTarget>`, which answers *what is drawable* (a component **is** the target; a config **holds** its
  data), *how to resolve it* (entt's `TryGet` vs an integer compare on `ConfigTypeID`) and *whether
  the entry frames itself* (the Inspector stacks components so each needs a header; the Config panel
  already names the config above the fields). Both forms — `Register<TTarget, TDrawer>()` and
  `Register<TTarget>()` — work for both subjects, so a config gets a hand-written override for free
  and the next subject is a specialization rather than a third registry. The **routes stay
  explicit**: `Drawers()` is `TDrawerRegistry<Entity>`, `ConfigDrawers()` is `TDrawerRegistry<IConfig>`.
- **An ENTRY is an ID scope, because a widget's identity is its LABEL** (2026-08-20, user caught it).
  A label here is a *property name*, and the Inspector stacks every applicable drawer into one
  window — so two types that share a field name are one ImGui id, twice: the widgets fight over
  hover and active state, and ImGui 1.92 reports *"visible items with conflicting ID"* on hover.
  Not an edge case: `DummyComponent` and `SpriteComponent` share `Position`, `Size` and `Color`
  **by design**, so the collision arrived with the first entity carrying both. `TDrawerRegistry`
  therefore wraps every entry — generic *and* hand-written — in `ImGui::PushID(<drawn type name>)`.
  **The placement is the rule**: a `TPropertyDrawer` sees one field and cannot know what else the
  window holds, while an entry is exactly the boundary between two independently-authored types.
  Keyed by the type name rather than the registration index, so a stored header open/closed state
  survives a drawer being registered ahead of it. It also closed the same latent bug in the Config
  panel, which had no scope of its own and would have collided for two configs sharing a field
  name — nobody had tried. [[L45]]

**I16 — A resource that needs the GPU splits `Load` (any thread) from `Initialize` (main thread), and
reaches the device through `IEngine`** (landed 2026-08-20 with `TextureResource`, the first GPU-backed
`CResource`). `CResource` always had the two-phase shape and this is the type it was designed for:
`Load` is file IO + CPU decode and runs on a worker, `Initialize` runs once on the pump and uploads.
- **It holds no device pointer, and that is the invariant, not a convenience.** The pool's
  `Initialize` hook takes no arguments, and a cached `IRHIDevice*` would outlive the device across a
  reset. The route is `IEngine::CreateTexture(pixels, w, h, channels)` → `RendererManager` →
  `RenderSystem` → device, the same chain `CreateFramebuffer` already had for the editor's viewport
  (**F2a**: only the device creates a GPU resource). `IEngine::Null()` answers `nullptr`, so a
  headless test decodes normally and simply gets no GPU handle (**I3**) — which is what makes the
  decode half unit-testable at all.
- **The BACKEND must not read files.** `OpenGLTexture2D`'s path ctor was deleted with its private
  copy of `stb_image`: decoding is CPU work every backend shares, and `stbi_load(path)` opens the
  file through a narrow CRT call that decodes the path with the ANSI code page (**I7**) — a
  silently-wrong file, the failure class this contract exists to prevent. `Load` reads bytes through
  `FileIO::ReadAllBytes` and decodes from memory, with the *thread-local* stb flip.
- **A PLACEHOLDER is a fully-initialised payload.** `ResourcePool::PlaceholderOrNull` now runs
  `MaybeInitialize` on the substitute it builds. Without it the magenta texture would be the one
  resource in the pool that never logs in — and it would fail *silently*, as a black quad, which is
  exactly what the Placeholder policy exists to avoid. Pinned by `ResourceSystemTests`.
- **A CONSUMER caches the claim, and the consumer is the adapter.** `RendererManager` holds
  `path id → ResourceRef<TextureResource>` and releases it in `Shutdown` — before the
  `ResourceManager`'s (reverse registration order), while the GL context is alive. It lives there
  because that class is *"the only render-side code allowed to reach host globals"*; `Renderer2D`
  stays portable and is handed an `ITexture2D&`.
- **A resource REFERENCE is typed: `TResourcePath<T>`** (`Engine/Subsystems/Resources/ResourcePath.h`,
  header-only, no `OPAAX_API` — **I6**). The path could have been a bare `OpaaxString`; the type
  parameter is what lets the editor's drop target **refuse** a `.wave` dragged onto a texture field,
  and what makes the next resource-referencing field one line with no editor code — **I15**'s
  one-specialization-serves-every-type shape. `T` is only ever *named*, never completed, so a
  component header needs a forward declaration instead of the RHI. It serializes as a **bare string**.
- **The editor's drag payload is ONE payload for every resource type** — `[TypeId][path bytes]`,
  variable length — so the browser drags whatever the format table already says a file is, with no
  per-type code, and the *drop* side decides. The target **peeks before accepting**, so a wrong type
  gets no accept highlight and the drag stays live. Asset-relative conversion happens at the SOURCE,
  which has `IPaths` through `EditorContext` (**MP8**); the drawer contract has no context, by design.
- **A reference may name a MOUNT, and there is exactly one: `/Engine/`** (④b, 2026-08-24).
  `Engine/Assets/Textures/` ships prototyping content (checkers, black/white squares) that a game
  could *see* and never *reference*, because a `TResourcePath` was project-relative and nothing else.
  `IPaths::ENGINE_MOUNT` fixes that in the ONE place that already answers path questions, so every
  existing consumer — `RendererManager::ResolveTexture`, the browser's drag, level manifests,
  `Engine::ResolveStartupLevel` — gained it with no edit of its own.
  - **Unprefixed stays PROJECT-relative, and there is deliberately no symmetric `/Game/`.** Every
    `.opaaxmap` and `.opaaxlevel` on disk already names its assets unprefixed, and **MP6** verifies
    those byte-for-byte: adding `/Game/` would rewrite every one of them to say what the *absence*
    of a prefix already says. The discriminator is the **leading `/`**, not the word — a project
    folder may legitimately be called `Engine`, and `lexically_relative` never emits a leading slash.
  - **`AbsoluteToAsset` tries the project FIRST**, so project content keeps its historical spelling
    and a project may shadow an engine path with one of its own. Both directions live in `Paths`
    over one `RelativeUnder` helper, so the inverse cannot drift from the forward.
  - The release deploy step for `Engine/Assets/Textures` is **not** optional: a shipped build
    resolves `EngineRoot` from the exe dir (**I12**), so an un-copied mount resolves to nothing.
    `Engine/CMakeLists.txt`'s comment claiming "nothing has loaded one since" was true until this
    change and is corrected with it.
- **A type icon is an IMAGE with the GLYPH as fallback, resolved against TWO roots** (④b). The two
  are separate facets — `SetIcon(path)` and `SetGlyph(text)` — rather than one replacing the other:
  a type that ships no image, or whose image is missing, still draws something. **The names had to
  be exactly these** (user, 2026-08-24): `Icon` first meant the text and then also the picture, so
  the field, the setter and their own comments disagreed. `Icon` is the picture, `Glyph` is the
  text, everywhere. The path is editor-assets-relative and
  searched **project editor space first, then the editor tool's own**
  (`EditorPaths::ToolAssetsDir()` = `<WorkspaceRoot>/Editor/Assets`, the editor binary's content as
  `EngineRoot()` is the engine's) — which is what lets a *game* ship an icon for its own resource
  type under the same relative name, and override an editor one.
  - **Icons load EAGERLY at the browser's `Startup`, and the registry is what licenses that**:
    `ResourceTypes()` was sealed at `OnModulesRegistered`, so the icon set is finite and known right
    there. Lazy loading was written first and was wrong for a reason worth keeping — the browser
    opens at Home, where no file tile draws, so **nothing exercised the icon path at all** and the
    smoke log proved nothing. Eager also makes the draw path a plain lookup.
  - **A FAILED icon claim is DROPPED, never cached, and this is a trap the whole tree shares:**
    `ResourceRef::Get()` on a failed claim answers the type's **placeholder**, not `nullptr`. Caching
    one would have drawn a magenta 2×2 square exactly where the glyph fallback belonged. The general
    rule: **gate on `IsValid()`, never on `Get() != nullptr`, whenever "absent" and "placeholder" are
    different answers to you.**
- **A PREVIEW is what a DOUBLE-CLICK opens — which is why I15 did not have to move** (④b, user's
  call). The open question was how a preview reaches the UI backend and the ResourceManager when
  `TPropertyDrawer::Draw(label, value, meta)` deliberately has no `EditorContext`. The answer was not
  to widen that contract but to notice that `ResourceTypeBuilder::SetActivate` **already is** "what
  does a double-click do", and Map and Level have opened documents through it since M2d.
  `ResourcePreviewPanel` is an ordinary registered panel, so it has a context by construction.
  - `ResourcePreview` (an `EditorContext` member, the `EditorSelection`/`PIE`/`MapDocument` shape) is
    the meeting point, because the WRITER (a stateless activate closure) and the READER (a live
    panel) are different objects and `EditorPanels` owns panels as `IEditorPanel` with no typed
    getter — by design. It stores WHAT was asked for; **the panel owns the claim**, so whoever draws
    the pixels is whoever keeps them alive.
  - The panel branches on one `TypeId`. **Growth point, named not built:** the second previewable
    type promotes that to a `SetPreview` chrome facet beside `SetActivate` — where per-type
    presentation already lives — never a chain of `if`s.
- **`ResourceManager::Find<T>(path)` answers "already resident?" and NEVER loads** (④b). The question
  an asset browser has to be able to ask: `Load` would answer it by pulling every file in the folder
  into memory, which is precisely what Legacy's *"only thumbnail an already-loaded texture, never
  force-load a whole folder"* forbids. It is `AcquireSlot`'s dedup half with the allocate half
  refused; `Loading` counts as **not found**, because the caller wants something it can display now.
  A miss returns an **empty ref with a null manager**, so `Get()` is honestly `nullptr` rather than
  the placeholder (`Pin`'s existing choice, and the trap above).
  - The payoff is emergent: a browser tile thumbnails not only what the *browser* opened but every
    texture the **game** loaded, because `RendererManager`'s cache put them in the same pool.
- **`EditorImage` carries its UVs so no call site has to remember the flip** (④b). One struct serves
  both display seams — `GetViewportImage` (an FBO) and `GetTextureImage` (a loaded texture) — because
  orientation is the BACKEND's to state: GL stores an FBO bottom-up, and `TextureResource` decodes
  bottom-up *because* GL samples that way. Legacy is the argument: four separate copies of the
  literal `(0,1)-(1,0)`, each with its own comment explaining it. Renamed from
  `EditorViewportImage` when the second caller arrived — the type was never viewport-specific.

**I17 — ONE position per entity, and every entity has one: `TransformComponent`** (landed ②,
2026-08-27). `World::CreateEntityWithGuid` emplaces it beside `EntityMeta`, so it is present
unconditionally and `Each<TransformComponent>` is a complete view. That guarantee is not tidiness —
it is what the editor stands on, and it was pulled a whole block forward *because* of that.
- **The hole it filled:** `Position` used to live separately on `SpriteComponent`, `DummyComponent`
  and `CameraComponent`, so one entity could carry three of them and they could disagree — and an
  entity carrying none had **no position at all**, which meant it could not be picked, framed, or
  drawn an icon. Unreal (`USceneComponent` on every `AActor`), Unity (a `Transform` you cannot
  remove) and Godot (`Node2D`) all hang their editor billboard/gizmo icon off exactly this
  guarantee. **Icons presuppose a universal transform**; that is why ② could not ship without one.
- **`Size` stays on the components.** An extent is what a thing IS, not where it is; `Scale`
  MULTIPLIES it. `Rotation` is DEGREES — what an author types — converted at the draw call, which
  takes radians.
  - **`Scale` arrived in ③ (2026-08-28), and X5 was honoured to the letter:** it landed in the same
    change as all three of its readers — both `RendererManager` passes and
    `EntityQuery::TryGetBounds`, so a scaled entity is clickable exactly where it draws (**SEL1**,
    one body, five call sites). ImGuizmo forced the timing: its decompose always answers a scale, and
    discarding one the gizmo had authored would have been a silent lie.
  - **Its default is the field where the default MATTERS.** `1`, not `0` — a zero would render every
    entity authored before ③ as nothing at all, with no error anywhere. `_WITH_DEFAULT` (**I8**) is
    what lets every `.opaaxmap` on disk keep loading unchanged; pinned in `MapSnapshotTests`.
  - **Adding a field to a component ALWAYS costs one MP6 warning per map** until it is next saved,
    because the writer emits a key the file does not have. First observed here (`first difference at
    byte 880`), expected, and self-clearing. Not a regression — the check working.
- **A component `CreateEntity` emplaces is ESSENTIAL and cannot be removed.** Stated at the
  REGISTRATION (`Register<T>(name, bEssential)`), next to the call that guarantees it, and enforced
  in `IComponentEntry::Remove` rather than by every UI remembering to check.
- **The migration cost was real and is the honest half of this entry.** Three of seven authored
  entities carried two positions. The rule applied: the transform takes the quad's, a camera that
  must sit elsewhere becomes **its own entity** (the model every reference engine has), and a
  sprite's own offset is dropped. A per-sprite local `Offset` (Godot's `Sprite2D.offset`) is the
  growth point for art that genuinely sits off its origin — named, not built.

---

## LC — Lifecycle: three states, not two

The engine has **three** states, and the middle one is the doctrine that keeps cleanup correct (**L7**):

| State | When | Guarantee | What may run here |
|-------|------|-----------|-------------------|
| **Running** | the frame loop | everything live | Update / FixedUpdate / Render / Present |
| **TearDown** | loop stopped, nothing destroyed yet | window, GPU, bus, **all siblings still alive** | release anything that needs a *live sibling*; broadcast "X-ending" events |
| **Shutdown** | reverse-order destruction via locator | siblings may **already be gone** | free only your *own* resources; never reach out |

**LC1 — The missing-phase rule.** If a cleanup problem has no good answer inside the phases you have, the
fix is usually a *new phase*, not defensive code in a destructor. The real axis is **"all alive" vs
"things dying"** — no reordering *inside* Shutdown can fake TearDown. This is one doctrine at every scope:
`ISubsystem::TearDown` / `IEngine::TearDown` / `OpaaxApplication::EngineTeardown`. Two-phase events wear
the same hat: broadcast `XCreated` *after* init, `XDestroying` *before* deinit.

**LC2 — Teardown-time events use `Publish`, not `Enqueue`.** The queue is flushed inside `Loop()`, which
has already stopped by TearDown; an *enqueued* teardown event is never delivered. Bridge it with `Publish`.

**LC3 — Idempotent + reverse order.** Startup is registration order; Shutdown/TearDown are reverse.
`Shutdown` is idempotent. Destruction is **reverse order of PROVISION** — and the last service provided
is *not* always `IEngine`. The base `Bootstrap` ends with it, but the `OnProvideServices` seam runs
**after** that, so a host service registered there tears down **before** the engine: the runtime order
is `IEngine → WindowManager` (no host overrides the seam), the editor's is
`IEditorService → IEngine → WindowManager`.
*Corrected 2026-08-05: this rule read "`IEngine` is provided last, so it tears down first", which is
false in the editor and misdescribes the mechanism the editor relies on.* That gap is where
`ViewportPanel::Shutdown` frees a panel-owned FBO while the engine, its device and the GL context are
all still alive — the device is freed later still, in `RendererManager::Shutdown`. Verified in the
editor's shutdown log: `Engine torn down` → `ViewportPanel shutdown` → `RendererManager shutdown`.
**`RendererManager` deliberately has NO `TearDown`**, because the LC table above promises every
subsystem that the GPU is alive for the whole of TearDown; releasing the device there would break that
promise for each of them. The two phases exist so the device outlives everything holding GPU resources.

---

## BO — Boot order (app services, in `Bootstrap()`)

Provided into the locator in strict dependency order — do not reorder without cause:

```
Platform → Paths → Logger(Paths) → Config(Paths)+PreRegisterConfig
        → ProjectManager(Paths) → JobSystem → WindowManager → Engine → OnProvideServices()
```

**BO1** — Config is loaded from disk here, *before* the Engine exists. JobSystem comes *after* Config
(worker count is config-driven). (See **L1**.)
**BO1b — a config serializes EXACTLY as a component does, and tolerance lives in ONE place**
(2026-08-20, user call: *"if one side is one type of code then in the other place is another type of
code its very annoying"*). A config data type carries `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`
and nothing else; `TConfigCodec`'s **default** does the rest (`dump(4)` out, `parse` + `get_to` in),
where it used to be an undefined primary every data type specialized by hand. That deleted ~220
lines of defensive parsing, two key-constant namespaces, `DECLARE_CONFIG_DATA`,
`DECLARE_T_CONFIG_CODEC`, and a `version` key that was **written but never read**.
- **The codec THROWS; `TConfig::Load` catches**, keeps the in-memory defaults and returns **false** —
  a value `ConfigSystem` used to discard and now logs as a Warn naming the file. The hand-written
  parsers were tolerant per field (`contains()` + `is_string()`, times fourteen); this is the same
  guarantee stated once. Core still does no logging (**I11**), so the Application layer says it.
- **Nested C++ structs mirror the file**, so `Engine.config` keeps its groups while the keys become
  the C++ names. Both `.config` files were regenerated in the same change; they are tracked, so a
  format change is a reviewable diff rather than a surprise. `OpaaxString` gained the json bridge it
  never had (`Core/String/OpaaxStringJson.h`) — every config had been hand-converting with `.CStr()`
  at each field, which is precisely the per-field labour the macro removes.
- **`CJsonSerializable` (`Core/Serialization/JsonConcept.h`) is that requirement stated once**, and
  `CComponent` (**I8**) is defined in terms of it rather than repeating the same `requires` block.

**BO1a — the config registry is the one registry that NEVER seals** (2026-08-19). `Get<T>()`
auto-registers on a miss, so `PreRegisterConfig` is a *convenience*, not the registration window:
`Config_Renderer` first appears in `RendererManager::Startup`, and a game system reading its config on
frame 500 appears then. Storage is therefore a `TDynArray` in **registration order**, owned by
`IConfigSystem` itself (both systems share it; they differ only in `OnConfigRegistered`), and every
consumer reads it **live** — the editor's `ConfigPanel` walks `GetConfigs()` each frame rather than
building a list at seal time, which would have missed exactly those late arrivals, silently.
**BO2** — The Engine service is **constructed** in `Bootstrap` (pre-window) but **started**
(`Engine().Startup()`) later, in `EngineStartup()`, *after* the window exists.
*(This supersedes L1's note that IEngine is created post-window — it is created in Bootstrap, started post-window.)*
**BO3** — The **window** is created in `InitializeApplication()`, not Bootstrap — it needs a live GL/VK context.
`WindowManager` (the service) is booted in Bootstrap; the window object comes later.

**BO4 — Starting a subsystem is INFRASTRUCTURE; creating a world is CONTENT, and the host does it last**
(landed M3, 2026-07-28 — user correction). `EngineStartup` reads as three plain stages:

```
Engine().Startup()                     → every subsystem constructed and started. NO WORLD EXISTS.
RegisterModules(...)                   → components -> ComponentRegistry, world subsystems -> its registry
OnModulesRegistered()                  → editor extensions; editor seals its own registries
Engine().FinishStartup(GetStartupWorldSpec())
                                       → host says WHICH; engine creates it. The first CreateWorld
                                         SEALS the registries, then makes the world.
```

**`Startup` and `FinishStartup` BRACKET the registration window** (M4 S2, 2026-07-29). Between them every
subsystem is up and no world exists — the only moment a module may register a type. That used to be stated
only in a comment; now the API says it, which is the whole point of the second name.

`WorldManager::Startup` **must not create a world.** It used to spin up a default "Main" so there was
"always a render target" — dead weight, because every consumer already handles no-active-world
(`RendererManager::Render` guards; `HierarchyPanel` renders "No active world."). What it *did* do was make
the boot order unfixable: the first `CreateWorld` seals `ComponentRegistry`, so a world born inside
subsystem startup locked the registry before any game module could reach it.

**The host states POLICY, the engine performs MECHANISM** (M4 S2 — two user TODOs in the tree). The seam is
a **pure query**: `virtual WorldSpec OpaaxApplication::GetStartupWorldSpec() const` returns
`{ Name, EWorldMode }` and *does nothing*; `IEngine::FinishStartup(spec)` does the `CreateWorld` +
`SetActiveWorld`. The old `CreateStartupWorld()` seam is **deleted** — it reached through the engine to drive
a subsystem (`Engine().GetWorldManager().CreateWorld(...)`), the same smell **MR0** removed for registries.
A side-effect-free query is also testable without booting an engine, and a host can no longer half-create a
world. `FinishStartup` **refuses loudly** (Error + null) if called before `Startup` — no lazy safety net,
because that is precisely what silently reordered the boot in [[L22]].

**The world comes from config, not from code**: the base `GetStartupWorldSpec` reads
`IProjectManager::StartupLevel()` — the project's `.opaaxproj`, key `startupLevel`, with `startupScene` /
`defaultScene` as Scene-era fallbacks (**X4**). Hosts override to open something else.

**BO4b — `WorldSpec` carries a `LevelPath` and a `Mode`, and NO `Name`: a world is named by the LEVEL it
opens** (M5 2026-08-03; naming reworked 2026-08-04). The seam briefly carried both, with `Name` **derived
from the path's stem** — and mining a name out of a file path is the wrong source, because it makes the
world's identity a property of where the file happens to sit rather than of what the author called it.
`LevelData::Name` is now that source: `LevelFile::Load` fills it from the `name` key, falling back to the
file's stem so an unnamed level still names its world. A host that cannot supply a level therefore cannot
invent a name for one either, which is why the field left the seam entirely along with
`OpaaxApplication::DeriveWorldName`.

**The consequence is an ORDERING one: `FinishStartup` reads the level BEFORE it creates the world**
(`Engine::ResolveStartupLevel` → `CreateWorld` → `OpenStartupLevel`). Only the *read* moved forward;
**instantiation still happens after `CreateWorld`, so WS7 is unaffected** — the boot log shows the world's
subsystems starting before the entities land, the same order a PIE clone gets.

**BO4c — A startup level that does not resolve is a FALLBACK, never a refusal to boot.** Absent, empty or
unreadable, the engine creates a world named **`NullLevel`** (`NULL_LEVEL_WORLD_NAME`, beside `WorldSpec`)
and opens nothing. The two cases are deliberately different volumes: an **empty `LevelPath` logs Info**
(a test host, or a game that fills its world in code — a supported answer, unchanged), while a path the
project **names but cannot open logs Warn** (a real misconfiguration). Resolution goes through
`ResourceManager::Load<LevelResource>`, whose FailFast null **is** the existence check — and it covers a
corrupt file too, which a bare path-exists test would wave through.
*The general lesson is [[L30]]'s: a field two things were sharing only because one of them was always
empty is a latent bug, not a simplification.*

**BO4a — `EWorldMode` is fixed at construction, and that is load-bearing** (M4 S2). A `World` takes its mode
in the ctor and exposes `GetMode()` with **no setter**; changing mode means creating another world. That is
what makes PIE-by-clone coherent: Play runs a *clone*, so Stop restores the edit world by discarding the
clone rather than undoing anything. A settable mode would quietly re-introduce the "put the world back after
playing" problem cloning exists to avoid. Runtime hosts answer `Play`; `EditorApplication` overrides the
query to `Edit` and lets the base keep owning where the *name* comes from. `EWorldMode`/`WorldSpec` live in
`Application/WorldSpec.h` — **Application, not World**: no Application header may include from `World/` or
`Engine/` (**MR1**) and a by-value return needs the complete type, but the better reason is that Edit-vs-Play
is a *host* mode. The World merely records the label it was born with.

**The rejected fix is worth remembering.** The first attempt kept the world in `WorldManager::Startup` and
split `StartupAll` into `CreateAll` + start, adding an `IEngine::BootSubsystems()` phase to squeeze
registration into the gap. It worked and it was wrong: it added machinery to preserve a layering mistake
instead of removing it. Deleting the world from subsystem startup deleted the phase too. **When ordering has
no legal window, check whether something is happening in the wrong phase before inventing a new one**
([[L22]]).

**Full frame of the run:**
`Bootstrap() → InitializeApplication() [window] → RunApplication{ EngineStartup → loop → EngineTeardown } → ShutdownApplication()`

---

## F — Frame contract

**F1 — Tick order** (`IEngine`, driven by the host): `Update(dt)` → `FixedUpdate(fixedDt)` *[may run 0..N
times]* → `Render(alpha)`. Then the **host** calls `PresentBackbuffer()` — outside the engine. Delta is clamped
to `MAX_FRAME_DELTA = 0.25`.
- **All four are MEASURED as of ④** (**ST1**), and `Present` is one of them precisely because it is
  outside `Loop`: under vsync it is where most of the frame goes, so leaving it unnamed would put the
  frame's whole cost in an "Other" row.

**F2 — Present is split from Render (render north star, S7/D2).** `Render` draws; present swaps. The
**host owns WHEN** (`RunApplication` calls `Engine().PresentBackbuffer()` after `TickFrame()`); the **device
owns HOW**. Only the backbuffer is ever presented; offscreen render targets never are — the facade method is
named `PresentBackbuffer()` (not `Present`) so the call site states that invariant (M1, 2026-07-26). The
`RenderSystem`/`RendererManager`/`IRHIDevice` layer keeps the plain name `Present()` (one unambiguous
swapchain there). This gap is what lets the editor draw world→FBO, UI→backbuffer, then present once. Present
is instance-based (no statics — **I1**).
- **Render-pass shape (landed M1, 2026-07-26):** `BeginFrame / BeginPass(target,view) / EndPass / EndFrame /
  PresentBackbuffer`. `BeginFrame`/`EndFrame` bracket the device frame; the pass bracket
  (`BeginRenderPass`/`EndRenderPass`) lives in `BeginPass`/`EndPass`, so a frame renders into any
  `IRenderTarget` (backbuffer or offscreen FBO, size read from the target). *Amended ⑥ S5,
  2026-09-04: `m_PrimaryTarget` is GONE — a frame is N passes, and which targets they land in comes
  from the per-frame view list (**MV1**).* "scene" is retired vocabulary — a render *pass into a
  target with a view*. New render
  path is **OpenGL-only** today; the VK backend is **parked in `Legacy/RHI/Vulkan`** (2026-07-22) pending a
  new-path `VulkanRHIDevice`.

**F2a — Every GPU resource is created BY THE DEVICE** (landed 2026-07-28). `IRHIDevice::CreateXxx` —
buffers, textures, shaders, pipelines, bind groups, **and framebuffers**. The device already knows its
own backend, so nothing else has to dispatch on one; a free `X::Create` puts a `MakeUnique<OpenGL…>`
inside a backend-*neutral* TU, which is the shape a second backend has to unpick. The one surviving
free factory is **`IGraphicsContext::Create`**, and only because the context must exist *before* the
device that `Init`s against it — there is no device to ask yet. That exception does not generalize:
if a device could have created it, the device creates it.
- The editor reaches this through **`IEngine::CreateFramebuffer`**, one intent-named method — *not*
  a `GetRenderDevice()` accessor. Exposing the device to reach one factory would also hand out
  `BeginFrame`/`Present`/`CreatePipeline`; the device stays engine-internal (this is M1 fork 1's
  objection honored, not reversed — the punt it justified is what ended).
- Resources are **caller-owned** (I5) and must be released while the device and its GPU context are
  still alive — for a panel-owned FBO that means `Shutdown`, never a destructor racing LC teardown.

**F3 — A subsystem needing a sibling mid-boot resolves from the manager.** The guarantee that makes this
work is the manager's **create pass**: `StartupAll` constructs every subsystem before running any
`Startup`, so during its own `Startup` a subsystem may reach a sibling that exists but has not started.
**Never** via a lazy accessor that can re-enter the owner's boot — that is the per-frame re-init loop of
**L6**.

*Corrected M4 S3, 2026-07-29: the "`m_Subsystems.GetSubsystem<T>()`" phrasing described what **`Engine`**
does — a subsystem has no manager pointer.* A subsystem reaches a sibling through
`OpaaxApplication::GetAppService<IEngine>()` and the engine's accessors (`GetResources()`,
`GetEngineEventBus()`, `GetDebugDraw()`), which are **safe mid-boot precisely because they
resolve-from-manager first** and only fall back to a lazy `Startup()` when nothing is there at all.
`RendererManager::Startup` and `WorldManager::Startup` both do this. Resolve **once, in `Startup`**, and
cache non-owning pointers (**I5**) — not per use, and not per world.

**F3a — One scope down: a WORLD's subsystems get the same create-then-start guarantee.**
`WorldManager::CreateSubsystemsFor` creates every qualifying candidate and *then* calls one `StartupAll`,
so a world subsystem may reach a sibling world subsystem during its own `Startup`. Do not start
candidates as you create them.

**F4 — DebugDraw is immediate-mode BY CONTRACT** (landed M2c, 2026-07-27). The queue
(`Renderer/DebugDraw.h`, owned **by value** by `RendererManager` — the thing that drains it, I5) is drained
and cleared **every frame, unconditionally** — including on frames that fail to render, which is why
`Render()` clears *outside* `RenderFrame()`'s early-outs. A producer that wants a line visible re-submits it
every frame; nothing is retained. Engine-owned, not editor-owned (D10: it serves dev builds of `Game.exe`,
which never links `OpaaxEditorLib`); reached via `IEngine::GetDebugDraw()`.

**F4d — It draws LINES and BOXES, and the box cost ONE vertex attribute** (2026-08-31, user's call:
*"we make it with 4 quads, we should create one using bounds and one shape empty with outline"*).
*This amends the clause that used to read "zero new RHI/shader/vertex-layout surface; keep it that
way" — a deliberate change, not drift.*
- **`DrawBox` was four `DrawLine`s**, so a selection of N entities cost 4N quads and the icon overlay
  4 more per entity. It is now one `DebugBox` rendering as one HOLLOW quad.
- **A quad carries the half-extent of its own hole** (`QuadVertex::InnerHalf`, local 0..1 space);
  the fragment shader discards inside it. `{0,0}` is solid, so **every pre-existing draw is
  unchanged** and an outline is a *value* rather than a second pipeline.
- **The part of the old clause that MATTERED survives**: still one pipeline, one batch, no second
  flush. A second pipeline was the obvious alternative and is worse — it would force a flush between
  world geometry and overlays, i.e. raise the very `Draw Calls` counter (**ST7**) this was meant to
  lower. Vertex cost is 40 → 48 bytes.
- **The outline path is UNTEXTURED by construction**, and that is load-bearing: the shader reads
  `v_TexCoord` as the fragment's LOCAL position, which only holds while the UVs span the full 0..1.
  A textured outline sampling an atlas sub-rect would carve the hole somewhere else, so there is
  deliberately no overload taking a texture.
- `MakeOutlineInnerHalf` is free and pure so the border maths is testable with no GL context, and
  **clamped to `[0, 0.5]` at BOTH ends** — below 0 is an inside-out hole that discards the whole
  quad (a too-thick border would *vanish* instead of drawing solid), above 0.5 is what a negative
  thickness produces. The first version clamped only the bottom and a test caught it.
- **`DrawBounds(Bounds2D, …)` is the entry point to reach for.** Every caller already holds one from
  `EntityQuery::TryGetBounds` and was unpacking it into a centre and a size just to hand both back.

This is not an implementation detail — it is the invariant that makes a whole bug class impossible.
Unreal's `FlushPersistentDebugLines(World)` is destructive-to-everyone *because* a shared retained pool
exists to destroy: one system clearing its debug geometry wipes every other system's. With no retained
pool, "flush my draws" is "stop calling `Draw`", which cannot touch a sibling. **Selective flush is free
here precisely because there is nothing to flush.**

Three forward constraints, decided before a second producer existed:
- **F4a — If retention is ever added it is DURATION-LIMITED. Never infinite persistence.** A timed entry
  expires on its own, so state stays bounded and "flush that specific thing" stays rare rather than routine.
  Infinite-persist *plus* a global-only flush **is** Unreal's bug; do not rebuild the first half.
- **F4b — Any retained store is keyed by a channel tag from day one** (`using DebugChannel = OpaaxStringID`
  — reuses an already-`OPAAX_API`, already-interned type; adds no static, I1-clean). Then "flush a channel"
  is an erase on one bucket, never a sweep. Retrofitting selectivity onto a flat retained array is how the
  Unreal shape gets rebuilt by accident.
- **F4c — No per-draw handles.** Every case that reaches for one (a marker on the selected waypoint, the
  last N impacts) is already covered by re-submission or a lifetime, and handles cost real bookkeeping in
  every producer. Revisit only for a concrete *same-frame* retraction need.

A **channel tag for central toggling** ("hide all physics debug" from one editor checkbox) is the one thing
"stop calling" cannot give you, since the toggle must live outside the producer. **BUILT ⑦-A P3
(2026-09-07) — the trigger fired exactly as written**, when collider outlines became the second real
producer. `using DebugChannel = OpaaxStringID`, as specified; the cross-module identity question was
already closed by **I2**'s out-of-line intern pool, so ids agree across the DLL line by construction.
Three things the build settled that the plan did not say:
- **Filtered at SUBMIT, not at the drain.** A silenced channel costs one lookup instead of memory that
  will never be rendered — and it keeps the immediate-mode contract honest, since a dropped submission
  never existed rather than being queued and skipped.
- **An unknown channel is ENABLED.** A disabled-SET, not an enabled-set, so a producer added later is
  visible without anyone registering it first. That is the right default for debug output specifically.
- **It is NOT an editor toggle panel.** F4 predicted a panel; the viewport toolbar already hosts exactly
  this kind of control (Grid), so `Colliders` sits beside it — but the STATE lives on the engine's
  `DebugDraw`, not on `EditorViewport`, because the producer also runs in a dev build of `Game.exe` and
  a toggle inside one host's UI could never reach it.
`Clear()` drops the queues and **keeps** the channel settings: a toggle is a setting, not per-frame state.

**F5 — A PASS is recorded whole, sorted ONCE, then cut into batches** (landed ⑥ S1, 2026-09-02).
`BeginPass` starts a recording, every `Draw*` appends four vertices + a sort key + a pass-wide
texture id, and `EndPass` calls `PlanQuadBatches` (`Renderer/Renderer2DBatchPlan.h`) before anything
reaches the GPU. **The ORDER of those two steps is the invariant**, and it is the whole entry: the
old code sorted *inside* `Flush`, i.e. after the batch had already been cut, so past `MAX_QUADS` or
the sampler limit the painter's algorithm held only *within* a batch and a later flush drew a
`Background` quad over a `UI` one. Draw order is now independent of where a flush lands.
- **A batch is a LIMIT, not a constant.** `RenderLimits{MaxQuads, MaxTextureSlots}` had sat in
  `RenderSystemDesc` unread since M1 under a NOTE saying dynamic buffers were "deferred"; it is now
  what sizes the vertex/index buffers at `Init`, fed from `RendererConfigData` by
  `RendererManager::Startup` — the adapter pattern `ClearColor` already used. **That is also the
  gate**: the split path is unreachable in a 7-quad scene until you can shrink the batch, so a
  config value is what let it run at all ([[L23]] — a feature that has never run in the real app is
  not done). Values outside what the buffers and the shader can honour clamp **loudly**; the floor
  of 2 slots is white plus one texture, because a batch no sprite fits in is not a limit.
  `SHADER_TEXTURE_SLOTS = 16` stays a constant — it is the length of `u_Textures[]` in
  `Sprite.glsl`, a shader fact rather than a policy, and the bind group still fills all 16 (unused
  ones bound to white), so **no shader change was needed**.
- **The texture in the sort key is now a PASS id, not a batch slot** — a slot cannot exist before
  the batch does. Same job (group equal-order quads by texture so a shared atlas draws in one call),
  now stable across the whole sort. `MakeSortKey`'s bit layout is untouched.
- **A whole trap is gone rather than documented.** `DrawSprite` used to carry an "ORDER MATTERS"
  comment: claim a slot before making room and a mid-`SubmitQuad` flush left the index naming a slot
  that no longer held the texture — the wrong image, silently. Nothing flushes during recording now,
  so `EnsureBatchRoom` is deleted and the trap is unrepresentable ([[L18]]'s "make the wrong thing
  impossible, not merely unlikely").
- **`PlanQuadBatches` is free, pure and exported** — the `MakeSortKey`/`MakeOutlineInnerHalf` shape,
  for the same reason: the ordering is the part worth pinning and it needs no GL context.
  `BatchPlanTests.cpp` holds the regression gate directly — four quads submitted `[UI, Bg, UI, Bg]`
  with room for two, asserting both `Bg` land in batch 0 and both `UI` in batch 1, **an ordering the
  old code could not produce**.
- **The instrument is a one-shot Info line** naming the batch count, quad count and both limits, the
  first time a pass needs more than one draw call ([[L12]]/[[L15]] — a smoke run cannot read the
  Stats panel, and "no errors" never discriminated here). The Stats panel's amber `Draw Calls` row
  survives with its meaning corrected: a split is a **cost**, not a drawing error (**ST7**).
- **Cost:** the record is `TDynArray`s that keep their capacity, so a steady frame allocates nothing
  and the per-quad memory is what the old fixed arrays already reserved. A pass may now name more
  textures than one batch can bind, which is exactly the question the plan answers.

---

## ST — Frame stats & profiling (landed ④, 2026-08-31)

**ST1 — NAMED SCOPES, not fixed fields — and I1 is what shapes them.** `FrameProfiler`
(`Core/Profiling/FrameProfiler.h`) holds a frame's `{Name, Milliseconds, Calls, Depth}` list;
`ScopedStat` + `OPAAX_STAT_SCOPE(profiler, "Name")` is the RAII that fills it. This is the shape
Unreal (`SCOPE_CYCLE_COUNTER`), Unity (`ProfilerMarker.Auto()`) and Godot all have, and the user asked
for it by name — *"at the end i want something extensible to have something like all other engine"*,
then again as *"STATS_SCOPE(ID) or somethings you judge better"*.
- **Those engines reach a GLOBAL stat manager, which is exactly the static this codebase forbids.**
  So a scope takes its profiler **by pointer**, and the two tiers that would otherwise have nowhere
  to get one already have a carrier (**ST3**). A `nullptr` profiler is a no-op, so "not profiling"
  costs no branch at any call site.
- **The sample is recorded on `Open`, not on `Close`.** Closing innermost-first would emit children
  before their parent; reserving the slot on the way in leaves `Samples()` in **pre-order**, so
  `Depth` alone renders the tree and nothing sorts. Ten lines of bookkeeping instead of a sort per
  frame, and the panel is a plain loop.
- **A scope re-entered UNDER THE SAME PARENT is one row with a call count, and a smoke run is what
  caught this.** The first measured frame logged **115 scopes**: `Loop` clamps a long first delta to
  `MAX_FRAME_DELTA`, which is 15 fixed steps, and every scope inside a `FixedUpdate` body therefore
  runs 15 times. `Milliseconds` accumulates and `Calls` counts, which is Unreal's stat-row shape and
  reads correctly on the frame that matters (a hitch is exactly when someone opens this panel).
  Merging is scoped to the open parent — by name alone, a subsystem's `Update` cost would fold into
  its `Render` cost — and compares the pointer first, `strcmp` second, so two literals spelling one
  name are still one row. *(The 115 came from the blanket wrapping **ST3** used to do; that is gone,
  but the merge is what makes a fixed-step scope readable and it stays.)*
- **`Name` is a `const char*` contracted to outlive the frame.** Not `OpaaxStringView` — **I13** gives
  the view no `CStr()` on purpose and a UI needs a terminator. Not `OpaaxStringID` — I13 says
  interning is for **keys**, and this is display text on a per-frame path. Every producer passes a
  literal.
- Header-only, **no `OPAAX_API`** — no static, no identity tag, the **I6** shape `ISubsystemManager`
  already uses.
- **THERE IS NO COMPILE-TIME SWITCH, and a `#if` one was built and then DELETED** (2026-08-31). An
  `OPAAX_STATS` flag derived from `OPAAX_DEV_BUILD` shipped first; the user's question —
  *"what is the cost of StatsService::Null on ship game?"* — retired it, because the answer made the
  flag worthless and its cost visible. **A null profiler pointer costs ONE PREDICTED BRANCH per
  scope**: `ScopedStat`'s ctor returns before taking a timestamp, the pointer is raw so there is no
  virtual call, and consumers cache it at `Startup` so there is no locator lookup. Under a
  microsecond a frame at 200 scopes, against ~10 µs enabled.
  - **And the flag actively cost something: it made `Stats.EnableInShipBuild` unreachable.** You
    cannot runtime-enable what was compiled out, so the two switches were contradictory rather than
    complementary. **One switch — the config — and it is what lets a SHIPPED game be profiled
    without a rebuild**, which is the case Unreal keeps a whole `Test` configuration for.

**ST2 — STATS ARE AN APP SERVICE, and the HOST owns the frame boundary.** `IStatsService`
(`Application/Services/`) owns the `FrameStats`; `OpaaxApplication::RunApplication` calls
`Stats().BeginFrame()` at the top of each iteration, beside `GetInput().EndFrame()`.
- **I4's test says app service, not engine subsystem**: it knows nothing about textures or worlds
  and it does not tick — it is a *passive facility you submit scopes to*, the Logger's shape. What
  looks like a tick is `BeginFrame`, which is a SUBMISSION: the host tells it a frame ended.
- **IN2 already settled which frame that is.** `Engine::Loop` is the engine's tick, not the frame —
  which is why input's `EndFrame` lives in the host loop. Publishing stats inside `Loop` was the
  same misplacement [[L28]] describes, and it had the same consequence: the frame the panel read
  was missing its `Present`, because Present runs *after* `Loop` returns.
- **The reader draws in the MIDDLE of the wall-clock frame.** The order is
  `BeginFrame(N)` → poll → `Loop(N)` → editor UI pass(N) → `Present(N)` → `BeginFrame(N+1)`, and the
  Stats panel draws inside that UI pass. `FrameProfiler` therefore **double-buffers** (`Publish()`
  swaps, so capacity stays and a warm frame allocates nothing) and a reader always sees the last
  **complete** frame, one frame old.
- **The service measures its own wall time between `BeginFrame` calls**, so nothing hands it a
  delta. It is the true host frame, and the first call reports 0 rather than the time since process
  start — one absurd sample at the front of every graph is worse than a missing one.
- **`FixedSteps` was DELETED as a field.** The `FixedUpdate` scope moved INSIDE the catch-up loop,
  so the profiler's own `Calls` merge (**ST1**) reports the step count and `Milliseconds` the total.
  One mechanism saying it once, instead of two saying it twice.
- **Never swap or move the `FrameStats` object** — consumers cache `&m_Stats.Profiler`. Fill it in
  place.
- *Corrects this entry's previous form, which had `Engine` own the snapshot and publish it at
  `Loop`'s top. The user's objection was three words — "still, engine have FrameStats".*

**ST3 — MEASURING IS OPT-IN, AT THE SITE THE AUTHOR CHOOSES. Core knows nothing about stats.**
`ISubsystem` does not mention the profiler, `ISubsystemManager` does not hold one, and no tick loop
wraps anything on an author's behalf. A scope exists because someone wrote `OPAAX_STAT_SCOPE` in the
body they cared about — Unreal's model exactly.
- **This entry replaces the opposite rule, and the correction came from the user** (2026-08-31, after
  the first build shipped): *"i do not realized that it was so much 'integrated' in core"* and
  *"there is too much noise in stats like for example input manager is write down on render slice
  even no input is in render"*. Both halves were one mistake. `ISubsystemManager` had gained a
  `FrameProfiler*` and wrapped every subsystem in `UpdateAll`/`FixedUpdateAll`/`RenderAll`, which
  put stats into the SUBSYSTEM CONTRACT (a pure-virtual `GetStatName()` every subsystem had to
  answer, in **Core**) and emitted a row for every subsystem × every phase — including
  `InputManager` under `Render`, whose `Render` is an empty override. **Automatic coverage is not a
  feature when most of what it covers does nothing:** a reader has to learn which rows to ignore,
  which is the opposite of what a profiler is for.
- **The fix was DELETION, not a filter.** Suppressing near-zero rows would have kept the Core
  coupling and hidden a real 0.00 (a subsystem that stopped working looks identical to one that was
  never meant to run). Removing the wrapping removes both problems and shrinks the contract.
- **The two carriers already existed, so opting in needs no new plumbing.**
  `IStatsService::GetProfiler()` is resolved once in `Startup` and cached like any other sibling
  (**F3**) — `Engine`, `RendererManager` and `WorldManager` all do it. A world subsystem takes
  `WorldContext::Profiler`, which is precisely what **WS3** says that struct is for: the
  registration site takes no arguments, so a dependency has nowhere else to arrive.
- **`WorldContext::Profiler` is the ONE member of that struct that may be null**, and deliberately:
  every other reference's absence is a boot failure, while this one's is a supported configuration.
  `WorldManager::CreateSubsystemsFor` therefore does NOT null-check it beside the others.
- **A GAME module's subsystem is one line** — `Sandbox`'s `QuadOscillatorSubsystem::Update` carried
  `OPAAX_STAT_SCOPE(&m_Context->Profiler, "QuadOscillator")` and appeared in the tree under `World`,
  with the engine still never naming the type. That was the extensibility claim, dogfooded rather
  than asserted ([[L23]]).
  **STALE AS OF 2026-09-07** — the user commented that registration out of `Sandbox.cpp`. The class
  is still in the tree and still compiles, but nothing registers it, so **the game-module
  world-subsystem route now has NO caller in either host** — `EditWorldSystems()` lost its last one
  when `QuadBoundsSubsystem` went, and this was the runtime half. The engine-owned route
  (`SpriteAnimation`, `Physics`) is exercised; **MR4**'s "a game module and an editor module land in
  one candidate list" is currently asserted, not dogfooded. One `Register` call restores it.
- **The tree is now small on purpose**: `Update` → `World` → the game's own scopes, `FixedUpdate`,
  `Render` → `Renderer`, `Present`. Every row is work someone chose to name.

**ST7 — A COUNTER IS THE TWIN OF A SCOPE: named, submitted by anyone, published together**
(landed ④ S2). `FrameProfiler::AddCount("Draw Calls", n)` beside `OPAAX_STAT_SCOPE`, keyed by name,
crossing the frame boundary in the same `Publish()`.
- **The shape was FORCED by layering, and it came out better than the typed struct planned.** ④'s
  plan had `FrameStats` hold a `Renderer2DStats`. It cannot: `FrameStats` lives in `Core/Profiling/`
  and **Core must not know what a draw call is** (**I4**). The alternatives were to move `FrameStats`
  up a layer or to put renderer nouns in Core; naming the counters instead does neither, and it
  makes a GAME's own numbers ("Bullets alive") free — the same extensibility the scopes have.
- **`RendererManager` translates**, because it is already *"the only render-side code allowed to
  reach host globals"*. `Renderer2D` keeps a typed `Renderer2DStats` internally (it is renderer
  code, that is fine); the adapter turns it into three named counters. The Stats panel therefore
  needs no renderer type to display them.
- **`AddCount` ACCUMULATES.** A producer may submit per-object or once with a total and both read
  correctly; a name-keyed merge with the same pointer-then-`strcmp` compare the scopes use.
- **Counters are reset by the FRAME, never by a pass.** `RenderSystem::BeginFrame` calls
  `Renderer2D::ResetStats()` — multi-view runs several `BeginPass`/`EndPass` brackets inside one
  frame and their draw calls all belong to one total. `StartPass` deliberately does NOT reset:
  a batch split is exactly the event being counted.
- **The submit sits OUTSIDE `RenderFrame`'s early-outs**, like the `DebugDraw` clear beside it
  (**F4**), so a frame that drew nothing reports zeros instead of leaving stale numbers on screen.
- **`DrawCalls` IS the flush count** — `Flush` issues exactly one `DrawIndexed` — so shipping both
  would be one number twice. Above 1 the frame split, and `Quads` / `Texture Slots` name which of
  the two configured limits did it.
- **Its amber highlight was DELETED with the bug it was built for** (2026-09-02). This bullet used
  to read *"above 1 it is ⑥'s bug made visible: `Renderer2D` sorts the CURRENT batch only"*, and the
  panel coloured that row to say so — correct, and the whole reason the counter shipped ahead of ⑥.
  **F5** made a split a cost, so the colour started crying wolf at a batch limit the author had
  configured deliberately (found the first time the user ran the ⑥ gate: *"3 was a bit red
  already"*). Every reference engine colours a counter against a **budget**; there is none here, so
  the honest form is a plain number. **An instrument built to expose one defect is finished when
  that defect is — retiring it is part of the fix, not a separate tidy-up.**

**ST8 — THE DEVICE TIMES ITSELF, AND THE RESULT IS ALWAYS LATE** (landed ④ S3).
`IRHIDevice::GetLastGpuFrameTimeMs()` is **one** virtual, and the timing happens inside the
`BeginFrame`/`EndFrame` bracket the device already had — so there are **zero new call sites**. ④'s
sequence entry named `IRHIDevice`/`ICommandBuffer` as where queries would go; a per-PASS timer on
`ICommandBuffer` has no caller ([[L23]]) and the frame bracket already existed, so `ICommandBuffer`
was left alone.
- **`OpenGLRHIDevice` keeps a 3-deep ring of `GL_TIME_ELAPSED` queries and NEVER waits.** It polls
  `GL_QUERY_RESULT_AVAILABLE` oldest-first and stops at the first that is not ready (queries complete
  in submission order), so the answer is **1–2 frames old by construction**. Reading a query in the
  frame that issued it would block until the GPU caught up — *the very stall this measurement exists
  to help find*. Three deep because the GPU trails by about a frame: one is always too early, two
  leaves no slack for a hitch. A slot still pending when its turn comes round loses that sample,
  which is cheaper than the stall.
- **Negative means NO READING, and zero would have been a lie** a panel cannot tell from a free
  frame. It is what a device with no timer support answers forever — `Init` checks the generated
  names and disables timing rather than erroring every frame.
- **`FrameStats::GpuMs` is a FIELD, deliberately NOT a scope in the tree.** GPU work runs *alongside*
  the CPU, not inside it, so a top-level row would be counted against the frame total and drive the
  "Other" remainder negative. The panel shows it beside the FPS line for the same reason. It is a
  *duration*, like `FrameMs` — Core learns nothing about GPUs, only that a frame has a second clock.
- **It reaches the service by SUBMISSION** (`SubmitGpuMs`), like a scope or a counter, because the
  renderer is the only thing that can ask the device and the service is the only thing that
  publishes. Held outside the snapshot and folded in at the boundary, so a published frame never
  mixes one frame's GPU reading with another's scopes.
- **Its own one-shot log line**, separate from the "frame stats live" one: the first GPU result
  lands a frame or two later, and *"the queries exist"* is not *"a result came back"*. Without it a
  harvest that never succeeds reads identically to one that works ([[L15]]).

**ST6 — NOT PROVIDING THE SERVICE *IS* THE OFF SWITCH** (**I3**). `BootStatsService` either provides
`StatsService` or returns `IStatsService::Null()`, whose `GetProfiler()` is `nullptr`. There is no
`bEnabled` member anywhere and no disabled state to keep correct — the locator's null object, which
this codebase already requires every service to have, IS the feature.
- **Config-driven, hence provided AFTER the config system** ([[L1]]'s locked boot order, the same
  reason the job system's worker count is). `Stats.EnableInShipBuild` defaults **false**.
- **A dev build always profiles**; the config answers only the question the build cannot. The
  provider keys on `defined(OPAAX_WORKSPACE_DIR)` — **I12**'s dev signal, the one `IPaths` uses —
  never on the editor flag, because a debug GAME build is a dev build with no editor.
- The field is named for what it does (`EnableInShipBuild`) rather than a bare `Enabled`, so it
  cannot be read as "turn stats off in the editor". Renaming it later is a **file-format** change,
  not a rename (the nlohmann macro uses field names as json keys).

**ST4 — The engine MEASURES; the reader keeps HISTORY.** `FrameStats` is one frame and nothing more —
**F4**'s immediate-mode doctrine, one scope out. A snapshot is what every consumer can agree on; a
history *length* is a display choice (a graph wants 120 samples, a log line wants none). So
`StatsPanel` owns its own `TStatsHistory<120>` and the engine holds no instance of one.
- `TStatsHistory` still lives in `Core/Profiling/` beside the profiler, because that is where someone
  would look for it — **placement is findability, not ownership.**
- Its `Offset()` is the **oldest** sample, not the write cursor, which is exactly what
  `ImGui::PlotLines`' `values_offset` wants. Confusing the two scrolls the graph backwards and is
  invisible by eye, so `StatsTests.cpp` pins the wrap-around case explicitly.

**ST5 — A CORRECT per-frame snapshot drawn raw is unreadable, and that is a DISPLAY defect, not a
measurement one.** The first build was accurate and the user's verdict was *"visually its very
glitchy"*. Three independent sources, all fixed on the editor side with the engine untouched:
- **Every number changed 60 times a second.** ~24 rows of `%.2f` shimmering. The panel now refreshes
  its text on a **0.25 s throttle** — `EditorService::RefreshDirtyCache`'s interval, and Unity's
  Statistics window does the same — and the headline is the history AVERAGE rather than the last
  frame. **The graph still samples every frame**, because a spike landing between two refreshes must
  not be lost; it is the one thing that should move at frame rate.
- **Rows APPEARED AND DISAPPEARED.** A frame whose accumulator took no fixed step has no
  `FixedUpdate` children, so every row below jumped up and back. `StatsDisplay`
  (`Editor/Panels/StatsDisplay.h`) holds the layout: a known scope keeps its place and reads 0.00 for
  the frames it did not run, and only a frame that is **not an ordered subsequence** of what is shown
  — a world change, a module registering a subsystem — rebuilds. Matched as a subsequence rather
  than by name because `Renderer` appears under both `Update` and `Render` at the same depth, so a
  name lookup would post the render cost onto the update row. Cleared on `OnActiveWorldChanged`, or a
  dead world's subsystems would sit there at 0.00 forever.
- **The graph ceiling rescaled continuously** as the window's maximum drifted. It now steps in whole
  60 Hz frames, with a floor of 33.3 ms so an idle 16 ms frame does not fill the plot.
- **`StatsDisplay` is header-only and ImGui-free so `OpaaxTests` can reach it** ([[L55]]'s include
  path). The fold is an algorithm with an ordering trap; putting it in the `.cpp` beside the widgets
  would have made it exactly the kind of by-eye-only code ③ opened that path to stop.

---

## CAM — Camera (landed ①, 2026-08-26)

**CAM1 — A world holds ONE resolved view; whoever produces it writes it there.** `CameraView`
(`Renderer/CameraView.h` — `{Vector2F Position; float OrthoSize;}`) is the authored half of
**F2**'s `RenderView`, and it lives on the `World` (`GetCameraView`/`SetCameraView`).
`RendererManager::RenderFrame` reads the active world's and composes the matrices against the render
target's pixels, which is exactly what `RenderView.h` has always asked for: *the renderer has no camera
class; the host composes the matrices.*
- **The slot exists because the engine must not be able to name its second producer.** The Play
  producer is an engine subsystem; the Edit producer is **editor-owned** (`Editor/Camera/EditorCamera`),
  and the engine stays editor-ignorant (**D4**). A `GetSubsystem<T>()` is keyed by type, so it could
  never ask for *"whoever frames this world"*. One POD member answers it with no vtable and no lookup.
- **It is PER-WORLD because PIE keeps two alive** (**WM6**) and each is framed differently. That is
  also what makes the editor's pan and zoom survive a Play→Stop cycle without anyone restoring
  anything: the Edit world is never touched, so its view is still there.
- **The fallback is the DEFAULT-CONSTRUCTED value, not a branch.** A world nobody produced a view for
  renders with `CameraView{}` — centred, 600 units tall, byte-identical to the ortho the engine
  hard-coded before this existed. **BO4c**'s rule one level down: never a black frame, and never a
  special case to keep in step. Pinned as an equality against `glm::ortho(-480,480,-300,300,-1,1)` in
  `CameraViewTests`, so it cannot drift silently.

**CAM2 — `OrthoSize` is the VERTICAL half-extent in world units; width follows the target's aspect.**
Resizing therefore **scales** the view instead of revealing more world — the old `1 unit = 1 pixel`
convention meant a 4K player saw four times the playfield, which is a real bug for a shmup and merely
convenient for an editor panel. One convention for both, because an editor that framed differently
from the game would be lying about what the game will look like.
- `EditorCamera` **seeds its own size from the first real viewport height** (`height * 0.5`), one shot,
  ignoring the 1x1 the panel reports before its first measured resize. The pre-camera view was one unit
  per pixel, so half the panel's height *is* the equivalent `OrthoSize` — which means the editor opens
  on exactly its historical framing at **any** panel size, and the convention change is only visible
  once you resize.
- `MakeViewProjection` and `ScreenToWorld` are the questions a view answers, defined **out of line
  and exported**: inline would drag `glm/gtc/matrix_transform.hpp` into every TU that includes
  `World.h`, and the editor calls `ScreenToWorld` from the exe (**I6**). `ScreenToWorld` is the ONE
  screen→world rule — zoom-at-cursor needs it now, picking and gizmo placement need the same answer.
  - *Corrected 2026-08-28: there are now **three**, not two.* ③ split `MakeView` and `MakeProjection`
    out because ImGuizmo takes them separately, and `MakeViewProjection` is **defined as their
    product** so the halves and the whole cannot drift — see **GIZ2**, including why its zero-size
    guard has to stay duplicated rather than defer to the projection half.

**CAM3 — The Play producer is an ENGINE subsystem, and the resolve is a PURE FUNCTION.**
`CameraManager` (`Engine/Subsystems/Camera/`) reads the active world in `Update`, refuses anything but
`Play`, and writes the slot. It was planned as a *world* subsystem, and that was wrong: the
world-subsystem shape was justified by *"it ticks behaviors"*, and with follow deferred there is no
per-world behavior and **no per-world state** — a camera's position lives on its entity in the world's
registry. The engine tier costs one line in the existing `RegisterNativeSubsystems()`; the world tier
would have cost a new `RegisterNativeWorldSubsystems()`, the first engine-native world subsystem, a
`WorldContext` it barely uses, and an instance per world including every test world. *(User's call,
and [[L47]]'s shape: a blocker I wrote down was a claim about PLACEMENT.)*
- **`CameraManager::Resolve(World&)` is `static` and pure, and that is the design being honest.**
  "The resolve needs the world and nothing else" is the entire argument for the tier, so it is stated
  as a function rather than asserted in a comment — and it makes the positive branch testable against a
  bare `World` with no engine to boot, which a smoke log cannot cover.
- **The view is written UNCONDITIONALLY**, so deleting the last camera mid-play snaps back to the
  default frame instead of freezing on the dead one's final position.
- **The Edit/Play fork is stated exactly TWICE, once per producer** — `CameraManager::Update` refuses
  non-Play, `EditorCamera::Apply` refuses non-Edit — and nowhere else. `Sandbox.exe` **is** the Play
  path; there is no third branch. Proven by absence in the logs: the editor's `CameraManager` says
  `started` and then nothing at all.
- **Several cameras: the FIRST wins and it warns, naming the count.** A `Priority` field nothing reads
  is a spec, and **X5** deletes those. Priority/blending is the growth point, and it is where this
  design gets expensive. Reporting is keyed on `World::GetId()` + the count, so it fires on
  *transition* — once per world, and again for each PIE clone, which is exactly when the answer can
  differ.

**CAM4 — The editor camera is driven from ImGui, and that is FORCED, not preferred.** With an Edit
world on screen the input route is `ClosedEditMode`, so `RouteInput` consumes every Input-category
event and `InputManager` is never fed — an editor camera reading it would report every button up
forever (**IN8**). The gesture is measured in `ViewportPanel::DrawContents`, where the panel's window
is current, which is the same source **IN8** already sends `Ctrl+S` to.
- **The gate is that window's own hover, NEVER `io.WantCaptureMouse`** — the viewport *is* an ImGui
  window, so that flag is true the whole time the pointer is over it ([[L29]], paid for once in M-Input).
- **Measured then applied, one frame apart**: `DrawContents` banks the drag and the wheel,
  `OnPreRender` spends them *after* the deferred resize (so the pixel sizes are current) and *before*
  `Engine().Loop()` (so the result reaches that frame). A panel's draw pass reads the world; anything
  that writes it runs outside the pass (**MP7**).
- **Zoom is applied before pan** — the wheel is cursor-anchored, so it must not read a position the pan
  has already moved out from under the pointer.
- **Middle-drag pans, wheel zooms**; left and right stay free for ②'s click-select and context menus.
  A drag that *starts* on the viewport continues while the button is held even off-panel, because
  cutting it at the panel edge is worst exactly when you are panning to the edge of a level.
  - *Amended ③, 2026-08-28: it no longer LEAVES the panel — the cursor **wraps** to the opposite edge
    (**GIZ6**).* The sentence above is why: wrapping serves that stated reason strictly better than
    wandering off does. A pan needs no correction for the wrap because it reads `MouseDelta`, which
    `TeleportMousePos` zeroes on the warp frame — one invisible frame of no motion.
- Two one-shot Info lines ([[L48]]): the seed, and the first move. Without them "pan does nothing"
  cannot be told apart from "the gesture never arrived", and only one of those is fixable.

**CAM5 — `CameraComponent::Position` is DEBT ON PURPOSE.** It carries the standing comment
`SpriteComponent` and `DummyComponent` already carry: it moves the day a transform exists. That makes
③'s fold **three** components, not two — recorded in `.claude/plans/engine-sequence.md` §③.

**CAM6 — What ① deliberately did NOT build** ([[L23]] — never an API with no caller): follow, shake,
priority/blending, `ViewportRect` and multi-view (**LANDED ⑥ S5, 2026-09-04 — see §MV.** It was
nearly free as promised, and **CAM1**'s slot survived untouched because the consumer that justified
it reads a camera entity instead of writing the world's view), perspective, confiner/bounds, a scene
view that detaches from the game camera during PIE, and
screen→world **picking** (`ScreenToWorld` landed here because zoom-at-cursor needs it; turning it into
click-select is ②).

**CAM7 — `Legacy/Renderer/Camera/` and `Legacy/Editor/Camera/` were DELETED here (19 files), and this
is the salvage record.** ① supersedes both, and leaving them meant a second **definition** of
`EditorCamera` and of a `CameraSubsystem`, with no lineage to the new ones — the ambiguity **X4**
exists to prevent. *Precisely what the delete bought: there is now exactly one definition of each
name. It did NOT silence every mention* — see the dangling-include bullet below — *but a dead call
site naming a type that does not exist is unambiguous in a way a second live definition never is.*
What came forward: `OrthographicCamera::RecalculateViewProjection` → `MakeViewProjection` (minus the dirty-flag
caching, which a per-frame recompute of two floats does not need); `OrthographicCamera::ScreenToWorld` →
the free function; `EditorCamera::Pan`/`Zoom` → the same, in `OrthoSize` vocabulary. What did **not**
come is the `ICamera`/`ICameraController` hierarchy around them — it fights **I8** and **D7**.
- **Named for later blocks, and living only in git from here** (`git show <this commit>^:<path>`):
  `Legacy/Renderer/Camera/FollowCameraController.cpp` — deadzone, snap-when-smoothing-is-zero, and
  frame-rate-independent exponential smoothing (`1 - exp(-dt/tau)`), the right shape for ⑦'s follow.
  `ShakeParams.h` + `ShakeCameraController.cpp` — amplitude, frequency, duration, a decay envelope, and
  a decoupled Y frequency ratio + phase so a shake is not a straight line.
  `ScreenSpaceCamera.h` — a pixel-space **Y-up** HUD view, immune to pan/zoom, for ⑥'s text and HUD.
- **Six other `Legacy/` files still `#include` those headers** (`Core/CoreEngineApp.cpp`,
  `Editor/EditorSubsystem.{h,cpp}`, `Renderer/Pass/{OverlayRenderPass.h,WorldRenderPass.h,.cpp}`) and
  now name files that do not exist. Nothing breaks — `Legacy/` is **not globbed** (**X1**, compiled =
  zero) and every one of those six is itself dead, referencing a `CoreEngineApp`/`WorldOld` world that
  is also going. Stated so the next reader does not mistake it for rot that arrived by accident.

---

## SEL — Selection, picking and the authoring verbs (landed ②, 2026-08-27)

**SEL1 — ONE entity-AABB rule: `EntityQuery` (`World/Entity/EntityQuery.h`).** Picking, the selection
outline, focus-selected, the marquee and later the gizmo all ask "where is this entity", and every one
of them asks HERE — so when a component's extent moves, one function body changes instead of five call
sites. `Bounds2D` (`Core/Maths/`, header-only, no `OPAAX_API` per **I6**) is the value it speaks in.
- **Two tiers.** An extent-bearing component (sprite, quad) gives a real box, **rotated by the
  transform** — an unrotated AABB under-covers a turned sprite, which would go unclickable across most
  of its length. An entity with none gets a box of `InAnchorHalfExtent` at its transform.
- **The anchor is OPT-IN: `0.f` means "no fallback".** A game asking what an entity *draws* must get
  `false`, not a placeholder; only the editor wants the icon-sized answer.
- `PickAt` is topmost by the renderer's own `(ERenderLayer, OrderInLayer)`, and an anchor-only entity
  sorts **below everything drawn** so an icon cannot steal a click from the sprite it sits under.
  `QueryOverlapping` is the same walk with `Intersects` — one file, so the two cannot drift.
- **Exported at authoring time** (**I6**'s checklist, a 5th strike paid ahead like `ScreenToWorld`).
  `OpaaxTests` is an exe linking the DLL, so the exe-side link is proven on arrival rather than latent.

**SEL2 — Picking asks the WORLD how it was framed, so there is no Edit/Play fork.** The conversion
reads `World::GetCameraView()` (through **CAM2**'s `ScreenToWorld`), never `EditorCamera` — so a click
resolves correctly inside a PIE session with no branch anywhere.

**SEL3 — A click is spent BEFORE the resize and the camera gesture.** `ViewportPanel::OnPreRender`
runs `ApplyPendingPick` first, deliberately: the click was made against the frame already RENDERED, so
it must be hit-tested against that frame's viewport size and camera. Applying it after either would
test against a frame the author never saw.

**SEL4 — The icon size is ONE value, used by the draw and by the hit test.**
`(OrthoSize * 2 / viewportHeightPx) * ICON_HALF_PIXELS`, computed per frame and handed to both
`EntityQuery::PickAt` and the `DebugDraw` box. What you see is what you click by construction, not by
two constants being kept in step. Edit worlds only — an overlay is authoring furniture.

**SEL5 — `EditorSelection` is a SET with a PRIMARY, and `Get()` still answers the primary.** That is
what lets the Inspector keep drawing exactly one entity while everything else grows a multi-selection
around it: **multi-select is not multi-edit**, and keeping the old accessor honest is what holds that
line. The panel SAYS so ("3 selected - editing the last picked") because an unexplained "I selected
three and one appeared" reads as a bug rather than a boundary. Real multi-edit is its own slice: a
`TPropertyDrawer` sees one `T&`, not N, and it wants ⑤'s choke point first so one edit is one undo.
- One `World*` for the whole set, not per entry — every member is in the same world by construction.
  `EditorService` retargets **every** entry by Guid on a world change, dropping what has no counterpart.

**SEL6 — `EntityOps` is THE ONE NAMED MUTATION CHOKE POINT** (`Editor/Operation/`, beside `MapOps` and
for its stated reason: a verb duplicated per call site is a verb that drifts). Create · Rename ·
DestroySelected · FocusSelected. The Edit menu, the Hierarchy's two context menus and the keys all
reach these, never `World` directly. **This is ⑤'s precondition** — undo becomes "wrap these" instead
of a twenty-call-site hunt, and `Editor.md` §7's promise about identifiable mutation points becomes
true rather than aspirational. The Inspector's field edits are the one mutation that cannot come
through it (a drawer writes straight through a `TComponent&`), which is exactly why
`World::GetRevision` exists.
- **It held for FOUR of the six, and ⑤ closed the fifth** (2026-09-01). The count was never four:
  the Inspector's Add/Remove Component popups called `IComponentEntry::Add/Remove` **directly**, with
  their own `MarkChanged` and their own log — a leak this entry did not know it had until undo needed
  the list to be complete. Both are `EntityOps` verbs now (`AddComponent`/`RemoveComponent`, taking the
  type's authoring name, since that is what a command can carry). **No panel mutates entt any more.**
  The sixth, the drawer's field write, is still uncatchable by a verb — and **UN6** is how it is
  recorded anyway. *The general shape: a choke-point claim is only as true as the last audit of it,
  and the audit that finds the leak is the one that has to enumerate every mutation for another reason.*
- **This entry became LOAD-BEARING on 2026-09-02**, when **UN1** was reversed and each verb started
  recording its own undo step. It used to be a convenience — the dispatch bracketed everything, so a
  verb reached outside `EntityOps` lost only its log line. Now it loses its undo, silently. Six verbs,
  one file, and the rule is the only thing enforcing it.
- **`Create` takes its `MapId` as a REQUIRED argument**, which is how **WM2** is closed by
  construction: an entity made without one lands in `(runtime - not saved)` where no Save can reach
  it. That was a live bug in the deleted `SandboxPanel`. The Hierarchy's header menu knows the map
  because it was clicked; the menu command falls back to the focused map because an entry names none.
- **`Create` uniquifies its name, `Rename` does not.** "3 selected - editing Entity" cannot say which,
  so the default must be distinguishable on sight; a name the author typed must not be silently altered.

**SEL7 — An editor shortcut belongs to the SELECTION, not to a panel.** `F` and `Delete` live in
`EditorService::HandleAuthoringShortcuts` beside Ctrl+S, routed globally, so they work from the
Hierarchy as well as the viewport. *Corrects the first attempt, which measured them on the viewport
and therefore did nothing from the panel an author is most likely to be deleting in.* What makes a
bare key safe is not the route but the **`WantCaptureKeyboard` guard**: typing "Fred" into the name
field cannot frame and delete the selection.

**SEL8 — The ImGui-hosted viewport keeps biting in ONE way: a borrowed predicate answers ImGui's
question, not yours** ([[L29]]'s family, now five occurrences). All three interactive bugs in ② were
this. `IsMouseDragging` = "is a drag in progress", **false on the release frame** where the answer was
wanted (latch it while true, never interrogate after). A floating window moves on a background drag,
and `ImGui::Image` is not an item. `IsWindowHovered` includes the **title bar**, so both gestures gate
on `IsItemHovered()` taken right after the image, and `io.ConfigWindowsMoveFromTitleBarOnly` states the
convention once. **When a panel's body becomes interactive, audit what the host already does with
drags there** — and note that none of the three was reachable by a smoke run.

---

## GIZ — The transform gizmo (landed ③, 2026-08-28)

**GIZ1 — The gizmo is IMGUIZMO, and the deciding question was not "screen or world"** (user's call).
Two designs were built. The first was hand-rolled `DebugDraw` handles sized from one
`WorldPerPixel()` (`d076b48`); the user then asked whether ImGuizmo would do, and after pricing both
honestly chose it (`3dc2d9b`), so the hand-rolled `GizmoHandles` was deleted.
- **What the survey actually settled.** Unreal (PDI geometry, screen-constant widget), Unity
  (`Handles` + `HandleUtility.GetHandleSize`), Godot-3D (`gizmo_scale` from distance), Godot-2D (an
  editor overlay control) and ImGuizmo (an ImGui draw list) **all size a gizmo from the SCREEN**.
  They differ only on where the geometry is authored, and that difference tracks 3D-vs-2D —
  occlusion — not taste. So "screen or world" was never the real axis.
- **What ImGuizmo bought:** rotate, scale, **snapping** and bounds in one step instead of three.
  **What it cost:** a vendored submodule, a view/projection split (**GIZ2**), and a matrix bridge for
  a component with two and a half fields. It is a 3D gizmo masked to `TRANSLATE_X|TRANSLATE_Y`,
  `ROTATE_Z`, `SCALE_X|SCALE_Y`.
- **The precedent cuts both ways and that is why it needed a decision, not a rule.** This tree vendors
  13 libraries, 7 as submodules, and box2d rather than hand-rolled physics — so "we avoid
  dependencies" is false here. But the depth *inside* ImGuizmo is almost entirely the 3D part; in 2D
  ortho, translate is point-in-box, rotate is an `atan2` delta and scale is a ratio. Both readings
  were defensible; the user owns the call.

**GIZ2 — `MakeView` / `MakeProjection` exist because ImGuizmo takes them SEPARATELY**, and
`MakeViewProjection` is now defined as their product so the halves and the whole cannot drift
(**CAM2**'s "two questions" is three). **Its zero-size guard stays duplicated on purpose:** identity ×
view is the *view*, so deferring to `MakeProjection` alone would change what a degenerate target
answers for any camera not at the origin. Pinned in `CameraViewTests`.

**GIZ3 — ONE VERB AT THE CHOKE POINT, AND IT TAKES A DELTA:
`EntityOps::TransformSelected(ctx, TransformDelta)`.** *(③b widened the argument from a bare
`Matrix44F` — see **GIZ9**: a matrix alone cannot say which frame its linear part is in.)* A matrix is what a gizmo *produces*: the delta maps
each entity's old placement to its new one, so translate, rotate-about-the-pivot and
scale-about-the-pivot all arrive as the same value. A multi-selection keeps its layout with no special
case, and a single entity — whose pivot is its own origin — falls out of the identical path.
- **Incremental, never absolute**, because that is the form ⑤ coalesces: a drag is many of these and
  deltas compose by multiplication.
- **Rotation and scale are read off the delta's own basis** — the angle and length of columns 0 and 1
  — so the choke point stays free of ImGuizmo's Euler decompose and speaks the engine's vocabulary,
  not a vendor's. A translation-conjugated delta has the same linear part, so this is exact.
- **This is what ③ owed ⑤**, and it is now true rather than aspirational.

**GIZ4 — IMGUIZMO'S `deltaMatrix` MEANS A DIFFERENT THING PER MODE. DO NOT USE IT.** The single worst
defect of this block, found by the user's eye and invisible at the origin:
- `HandleTranslation` (`ImGuizmo.cpp:2394`) → a per-frame increment.
- `HandleRotation` (`:2674`) → `modelInverse * rotation * model`: incremental **and** already
  conjugated about the pivot.
- `HandleScale` (`:2544`) → a **pure origin-centred** `Scale(...)` whose factor is measured **since
  the drag began**.
Used uniformly, scale multiplied an entity's POSITION about the world origin — exactly nil at (0,0),
so it looked right there and only there — and compounded the cumulative factor every frame on top.
- **The fix is to stop asking: the delta is `M * inverse(M last frame)` off the matrix WE own.**
  Per-frame by construction, and because that matrix **sits on the pivot** the conjugation is free.
  One expression for all three modes, no per-mode knowledge anywhere. `deltaMatrix` is passed `nullptr`.
- **The general shape: a vendor's "delta" is a name, not a contract.** Read what it computes per
  branch before treating it as uniform.

**GIZ5 — The matrix is STATE, re-seated only while idle.** ImGuizmo captures its start pose when a
drag begins and then drives the matrix it was handed, so `EditorGizmo::ReseatAt` runs on every frame
`IsUsing()` is false and never during a drag — re-seating mid-drag would fight that captured state.
`ReseatAt` moves **both** `m_Matrix` and `m_PrevMatrix`, or the first frame of the next drag
differences against a stale pose and jumps. *(③b: it takes a ROTATION too, and remembers it — see
**GIZ9**. Scale is always seeded to unit: the matrix measures a DRAG, not the entity, and seeding it
otherwise would make the first frame report a stretch nobody applied.)*
- **`Manipulate`'s RETURN VALUE gates banking, not `IsUsing()`.** It answers "did the matrix actually
  change"; `IsUsing()` stays true for the whole gesture and would bank an identity delta every frame,
  dirtying the map on a click that never moved.
- Mode is three tags, not one with a payload — **a key binding carries a tag and no payload**, the
  same constraint that removed `QuitParams`. W/E/R (Unreal, Unity and Godot all share it), editor-wide
  behind `WantCaptureKeyboard` (**SEL7**), but **Edit-only unlike F and Delete**, because W/E/R are
  also the game's movement keys. The Edit-menu entries tick from the LIVE mode, so keys and menu
  cannot disagree.

**GIZ6 — THE INFINITE DRAG: wrap the cursor, and correct for it where it is read ABSOLUTELY.**
A drag that dies at the panel edge is worst exactly where it is needed most. `ImguiCursor::WrapInRect`
teleports the cursor to the opposite edge via `ImGui::TeleportMousePos`, which moves `MousePos` **and**
`MousePosPrev` — so `MouseDelta` reads zero on the warp frame — and raises `WantSetMousePos` for the
backend (`imgui_impl_glfw.cpp:943`, honoured whenever the window is focused).
- **THE TWO CONSUMERS DIFFER, and that is the whole content of this rule.** A **delta** reader (the
  camera pan) needs nothing more: it accumulates no motion for one frame, invisibly. An **absolute**
  reader (ImGuizmo) would see the cursor leap across the viewport and **fling the selection** — so the
  wrap returns a correction, the panel accumulates it for the drag, and `io.MousePos` is offset by it
  **for the length of the `Manipulate` call only**, restored immediately because every other reader in
  the frame wants the real cursor.
- **This amends CAM4**, whose own justification asks for it. The **marquee is deliberately excluded**:
  a rectangle drawn between two screen points is nonsense wrapped.
- `ImguiCursor` is a fourth file in `Editor/ImguiLibrary/` because that library splits on
  *submits-an-item* / *paints-only* / *pure-geometry*, and moving the cursor is none of them. It is the
  tree's only `imgui_internal.h` consumer.

**GIZ7 — ~~Named, NOT built: a viewport tool strip~~ BUILT IN ③b** (2026-08-28→30). This entry filed
the strip as a growth point and the user's answer was *"I mean i want a really task for viewport
toolbar"* — so it became its own block. See **GIZ8**–**GIZ10**. *Kept as a record of the call, not as
a live claim.* Still unbuilt: a rotate/scale gizmo for a multi-selection with **mixed** rotations
(**GIZ9** states what it approximates), and `DebugDraw::DrawCircle` — the rotate ring is ImGuizmo's,
so that header's *"waits for a caller that needs it"* still does.

**GIZ8 — THE VIEWPORT TOOLBAR IS A REGISTRY, and an item is a CLOSURE where a menu node is a TAG.**
`ViewportTools()` is the eighth route on `EditorExtensionRegistrar`, natives → modules → `Seal()`
(**MR2**), so a game module adds a tool with the same call a panel takes. The user's requirement set
this: *"should be easy to add thing in it too!"*.
- **The closure is not a relapse into what [[L37]]/[[L38]] deleted.** A menu node has exactly ONE
  behaviour — invoke a command — so a tag says everything about it. A toolbar item is a WIDGET: a
  toggle, a drag-float, a combo. There is no uniform behaviour to name, and tags would force a new
  item TYPE per widget kind, which is the machinery "easy to add things" exists to avoid. An item
  that *does* invoke still uses a tag: the mode buttons dispatch `EDITOR_COMMAND_GIZMO_*`, making the
  toolbar a THIRD front-end rather than a fourth source of truth.
- Each item is wrapped in `PushID(id)` — **I15**'s entry-is-an-ID-scope rule, so two independently
  authored items sharing a label cannot fight over hover state. A label that carries state pins its
  own id with `###`, or clicking it would make it a different widget every frame.
- **A closure is not an ANONYMOUS closure** (2026-08-30). The five natives were written as lambdas
  inline in `EditorService::RegisterNativeViewportTools`, which put ~150 lines of widget code — the
  only `ImGui::` calls in the file — inside the composition root. They are now named functions in
  `Editor/Toolbar/EditorNativeViewportTools.{h,cpp}`, registered by pointer: the **`EditorNativeCommands`
  shape**, one route over. The registration site keeps what is genuinely its decision — the ORDER and
  the separator grouping (Grid sits with Snap deliberately) — and `EditorService.cpp` is now free of
  widget calls entirely. `DrawFunc` is unchanged; a lambda still registers, so a game module pays
  nothing for this.
- **It is an OVERLAY, and the ORDER is the whole risk.** The strip sits ON the image, and every
  viewport gesture gates on the image's hover — so it is drawn BEFORE the measures and its rect
  SUBTRACTED from that hover, or a click on "Snap" also starts a marquee. The gizmo needs the same
  guard through `ImGuizmo::Enable(false)` (which still draws, only refuses to manipulate) — but
  **never mid-drag**, because that call cancels the interaction it is editing.

**GIZ9 — PIVOT AND SPACE, AND THE FRAME A DELTA IS EXPRESSED IN.**
- **Space** is `World` or `Local`; Local is `ReseatAt` adopting the PRIMARY's rotation. Without that
  the two are identical, because ③ always built the matrix unrotated.
- **SCALE FORCES LOCAL** (`GetEffectiveSpace`), and the alternative is unrepresentable rather than
  merely awkward: scaling along world axes an entity turned by R is a **shear**, and
  `{Position, Rotation, Scale}` has nowhere to put one. Unity forces it for the same reason; the
  toolbar disables the button and says why instead of offering a control that cannot work.
- **Pivot is THREE modes, and the third is a different KIND of answer.** Center and Origin are both
  ONE shared point, so N entities orbit it and keep formation. `Individual` (Blender's Individual
  Origins, and the user's own drawing) means each entity turns about ITSELF. It is implemented as an
  **absence** — skip the position multiply — because an entity that is its own pivot cannot be moved
  by turning about itself. Refused for translate, where "about its own origin" has no meaning.
- **A DELTA IS ONLY MEANINGFUL WITH THE FRAME IT WAS BUILT IN, and that frame is the GIZMO'S.** A
  scale arrives as `R·S·R⁻¹`; conjugating by `R` recovers a clean diagonal `S` for **every** entity.
  Conjugating by each entity's *own* rotation — the ③b bug the user found — cancels only for the
  entity that happens to match the gizmo, and hands every other one a non-diagonal matrix whose
  `atan2` is a rotation nobody asked for. Because the answer is identical for all of them, the
  conjugation lives OUTSIDE the per-entity loop. `EditorGizmo` REMEMBERS the pose (`GetFrameRad`): a
  drag never reseats, and `R·S·R⁻¹` cannot be reduced by anyone who does not know `R`.
- **So the choke point takes a `TransformDelta`, not a matrix** (**GIZ3** amended). `Matrix`,
  `FrameRad` and `Origin` are one answer to "what did the gizmo just do" — which is also exactly what
  ⑤ records to replay a drag — and a struct stops the parameter list growing again.
- **What it approximates, stated:** an entity whose rotation differs from the gizmo's gets `S` applied
  along its OWN axes. The exact result is a shear, so this is the honest 2D answer, not a defect.

**GIZ10 — THE SNAP GRID IS THE SNAP STEP MADE VISIBLE, and snapping follows what is DRAWN.**
Spacing IS the translate snap step, Edit worlds only, on the **Background** band.
- **`DebugDraw` gained a per-segment `ERenderLayer` for this**, defaulting to `Debug` so every prior
  caller is untouched. A grid on the Debug band would draw over every sprite, which is not a grid but
  a cage — the first thing that queue could not express.
- **Bounded TWICE:** the visible world rect from the same `ScreenToWorld` picking uses, and a
  **decade step-up** once a cell would be finer than a few pixels, solved with `log10` rather than
  looped so a pathological step cannot spin. The line cap behind both is a guard, not the mechanism.
- **Snapping follows the drawn spacing while the grid is visible** (user's call): zoomed out the grid
  coarsens to 100s while the authored step is still 10, so a drag was landing *between* two visible
  lines. With the grid hidden the authored number is honoured literally, because then there is
  nothing to match. **What you snap to is what you can see.**
- **Named, not built:** cross-session persistence of any of this. `EditorCamera`'s pan and zoom do not
  survive a restart either. *(The "first non-DLL config" question this used to raise is CLOSED —
  `Config_EditorImgui` answered it 2026-09-01: `DECLARE_T_CONFIG`, no `OPAAX_API`. See **I6**'s
  mirror-image strike. Only the persistence itself is still unbuilt.)*

---

## UN — Undo / redo (landed ⑤, 2026-09-01)

> **REWRITTEN 2026-09-02, and UN1 is REVERSED.** The first shipped version made the executed
> `IEditorCommand` the undo entry and had the dispatch bracket every verb with a before/after
> capture; a standing baseline plus a per-frame poll covered the drawer writes no verb announces.
> It was rejected on first hands-on use — a slow drag recorded one step per micro-pause, the poll
> flooded the log, and `EditorUndo` had grown to 17 public members. The user's replacement is below
> and it is smaller in every dimension. See [[L73]] for the post-mortem; the old shape is recorded
> only where this section says "it used to".

**UN1 — THE STACK HOLDS UNDOABLE OBJECTS AND NOTHING ELSE** (user's call, 2026-09-02: *"The undo
system do not care about a property changing or whatever. The property itself calls undo to record its
own stuff."*). `EditorUndo` (`Editor/Undo/`) is **`Record` · `Undo` · `Redo`**, plus the four reads the
Edit menu needs and `Clear` — 8 public members, 3 members, and **it includes no engine header at all**.
It knows nothing about `World`, `MapData`, `EntityOps`, `EditorSelection`, revisions or ImGui.
- **The verb that made the edit is the one that builds the step**, because it is the only thing that
  knows what changed. `EntityOps`' five verbs each end in one `Record` call; the two multi-frame
  gestures are built by the panels that own their edges (**UN5**).
- **`EditorCommandRegistry::Execute` brackets nothing** — find, typecheck, run, return, and the
  instance dies with its call. That reverses this rule's first version, in which the registry opened
  and closed a record around every dispatch and kept the command alive as the entry.
- **What the reversal cost is real and is stated, not hidden: a new mutation verb that forgets to
  `Record` has no undo, silently.** The bracket caught that automatically. The mitigation is
  **SEL6** — `EntityOps` is one named choke point with six verbs — and this sentence. It is
  discipline, not structure, and nothing makes it impossible.
- **What it bought:** the poll, the baseline, the gesture API, `EntityEdit`, both command concepts,
  `IEditorCommand`'s five undo virtuals, the 20-line dispatch bracket, all seven `UndoLabel()`
  declarations and the empty `EditPropertiesCommand` all went away in one change.
- **Policy is NOT the stack's.** `UndoCommand`/`RedoCommand` gate on `MapOps::CanEdit`, and
  `EditorService::HandleWorldDestroyed` calls `Clear()` when an **Edit**-mode world dies — asked of
  `World::GetMode()` rather than of a recorded world id, which is what lets `EditorUndo` not name
  `World`. Same behaviour as the old key-on-`GetId()` rule: a PIE cycle destroys the Play clone, so
  Play/Stop keeps the history exactly as Unreal does.

**UN2 — A STEP IS A SMALL TYPED OBJECT THAT STATES ITS OWN INVERSE.** Seven of them, one per verb,
each carrying exactly what its inverse needs and nothing else — **four serialize nothing at all**:

| Step | Payload | Serializes |
|------|---------|-----------|
| `EntityCreate` / `EntityDelete` | the entities as `MapData` | yes (`CaptureEntities` / `Restore`) |
| `EntityRename` | `Guid` + two `OpaaxString` | no |
| `EntityTransform` | `{Guid, TransformComponent Before, After}[]` + its own name | no |
| `ComponentAdd` | `Guid` + type name | no |
| `ComponentRemove` | + the component's `json` | one component |
| `EntityComponentsEdit` | `Guid` + the changed `ComponentData` both sides | the changed ones only |

- **`ComponentAdd` deliberately carries no payload**: `AddComponent` default-constructs, so redo has
  nothing to restore. `ComponentRemove` must carry one, or undo brings the type back at its defaults —
  which reads as data loss rather than as an undo.
- **`EntityTransform` is ONE type for three modes and any count.** The payload is identical whichever
  handle was grabbed, so the mode is only the *name*, and one entity is a list of one. Four types with
  the same body would be four places to fix a bug. Its label is `ToString(EGizmoMode)`, so the menu
  reads **"Undo Translate"** — the gizmo's own word, the one the toolbar already shows.
- **Create and delete are the same two bodies run in opposite directions** (`RestoreEntities` /
  `DestroyEntities`), and restore re-selects while destroy clears — which is what makes the
  selection need no payload of its own, unlike the record this replaced.

**UN3 — ONE CONCEPT, three members, the exact parallel of `EditorCommand`.**
`EditorUndoable<T, ContextType>` (`Editor/Undo/EditorUndoableConcept.h`) requires `Undo(Ctx)`,
`Redo(Ctx)` and a `const` `Label()`. `IEditorUndoable` erases it with the same `Concept`/`Model<T>`
shape `IEditorCommand` uses, and constrains the same two places: the erasure's ctor and the public
entry point (`Record<T>`, where `Register<T>` sits for commands).
- **`Label()` is an INSTANCE method, not `static`.** That is the whole reason a step can name itself
  from its own data ("Translate" / "Rotate" / "Scale"), which is what let the gesture API — whose only
  remaining job was carrying that label — be deleted rather than renamed.
- **No base class and no registry**, the `CComponent`/`CResource` shape (**I8**). A game module's own
  step is one struct with three members; nothing has to be registered, and a type that cannot answer
  all three fails at the `Record` call naming itself.
- `EditorContext` is only FORWARD-DECLARED in `IEditorUndoable.h` — `Model<T>`'s bodies instantiate at
  the `Record<T>` call site, where it is complete. That is what keeps the stack's header clean.

**UN4 — REDO IS NEVER A SECOND `Execute`.** Re-running the verb is the obvious move and it is wrong:
`EntityOps::Create` mints a **fresh Guid** and a freshly uniquified name, so a second run produces a
*different* entity and breaks every reference the Guid exists to protect; `SaveMapAsCommand` would
re-open a file dialog. **A step re-asserts what it recorded**, which is also why a step is state
rather than a hand-written inverse wherever the inverse is not exact: `TransformSelected` only *looks*
invertible — it writes `Rotation +=`, `Scale *=`, and skips entities whose handle went stale.
- **The two state-shaped steps reuse the engine's snapshot core, which already was this**:
  `MapSerializer::CaptureEntities` (a third NAMED capture, **MP10**'s idiom) and `MapFactory::Restore`
  (recreate-by-Guid / overwrite / **remove the registered components the data does not name**; an
  essential type refuses and stays, **I17**). `Restore` bumps the revision, because an in-place
  component write is invisible to the world.
- **Identity is the Guid, everywhere** — a step outlives the edit, and an entt handle does not survive
  an undo that recreated the entity. The selection stores handles, so it is re-resolved after a
  restore.
- Pinned headless by `Engine/Tests/Core/World/MapRestoreTests.cpp` — the gate is
  **capture → edit → restore → re-capture, byte-equal** through `MapJson::Serialize`. It did not move
  when UN1 was reversed, which is the proof the engine half never depended on the editor's shape.

**UN5 — A MULTI-FRAME EDIT IS ONE STEP BECAUSE THE PANEL HOLDS THE STEP ACROSS FRAMES.** `Begin` on
the rising edge, `End` on the falling edge, one `Record` in between. **No baseline, no poll, no
gesture API, and nothing in `EditorUndo` knows a drag happened.** The two edges already existed in the
code for other reasons; this rule only says what they are now wired to.
- **The viewport** owns `ImGuizmo::IsUsing()` and holds an `EntityTransform`. It closes in
  `OnPreRender`, after `ApplyGizmoDrag`, never at the edge seen in the ImGui pass: a delta measured in
  frame N is applied in frame N+1 (**SEL3**), so closing early would read a world that predates the
  final motion. `m_bGizmoMeasured` closes it when the panel stops drawing at all.
- **The Inspector** owns `ImGui::IsAnyItemActive()` — the line it already kept for `MarkChanged` — and
  holds an `EntityComponentsEdit`. **Global is the point, not a compromise:** a resource dragged from
  the Browser holds `ActiveId` over there, so the bracket opens before the drop and closes on it.
  (Verified by hand 2026-09-02; the plan had wrongly listed that path as uncovered.)
- **A step must be CLOSED on the falling edge whether or not it recorded.** One left holding entries
  keeps re-reading them, and the next unrelated edit to the same entity surfaces as a phantom step
  under the old label. Both panels reset unconditionally.
- **THE RISING EDGE IS READ AFTER THE DRAWERS RAN, and that is correct rather than lucky.** ImGui
  zeroes the drag accumulator on the frame an item is activated (`ActiveIdIsJustActivated`) and
  trickles the click and the first move into different frames, so a `Drag*` has not written yet;
  `InputText` has only taken focus; a `Checkbox` commits on release. A **click-set** widget is the
  exception — the colour picker popup's SV square jumps on its activation frame, so that first jump
  sits outside the step. Named, not built: caching at the top of `DrawContents` fixes it in one line.
- **SETTLING IS NOT A BOUNDARY, and the first version's central mistake was assuming it could be.**
  It committed a step whenever the value stopped changing for one frame, so a slow drag became one
  step per micro-pause. No engine does this: Unreal brackets at the widget
  (`OnBegin`/`OnEndSliderMovement` → `FScopedTransaction`), Unity groups on the mouse-down event,
  Godot and Lumix merge in the stack by name within a time window. **All four push "what is one step"
  outward to whoever made the edit** — which is what UN1 now does.

**UN6 — THE PROPERTY STEP RECORDS VALUES ONLY, and it needs to know the ENTITY, never the field.**
A `TPropertyDrawer` writes straight through a `T&` (**I15**), so a field edit is the one mutation no
verb announces — but the Inspector draws exactly **one** entity, the primary selection, so whatever
was edited belongs to a known Guid. `EntityComponentsEdit` narrows at `End()` to the components whose
*payload* differs, and that narrowing is load-bearing rather than tidy:
- **A type on one side and not the other was added or removed** — `ComponentAdd`'s and
  `ComponentRemove`'s steps — and the entity's NAME is `EntityRename`'s. So committing a name in the
  same frame the gesture closes records **nothing** here, instead of a second step that undoes the
  same rename. That hazard was a live gate in the first version ("if one rename takes two Ctrl+Z,
  that is this"); here it cannot arise. Same for Add and Remove Component.
- **Per-COMPONENT attribution is available and was not needed.** `TDrawerRegistry::Register`'s entry
  captures the component's authoring name and receives the `Entity` at exactly the point it pushes the
  ID scope (**I15**) — so "whose value is this" is answerable. Undo restores state, not a field, so
  the entity is enough. The growth point, if a step ever needs to *name* the field: it is there.
- **HISTORY IS ONE WORLD'S.** Every mutation gates on `MapOps::CanEdit`, so only edit-world state is
  ever recorded, and `Undo`/`Redo` gate on it too. `MapOps::RemoveFromLevel` clears the stack, and it
  is the **only** structural verb that does: those entities are gone *and* unmounted, so undoing a
  step naming one would recreate it into a world no Save can write it from (**WM2**). Add-map and
  set-persistent touch no recorded entity.
- **`Record` is a no-op while a step is replaying** (`m_bApplying`, private), or an undo pushes
  itself as the next step. Nothing outside has to remember to ask.
- **The Edit menu names the step** — `EditorTitleBarCommandNode::SetLabel(FMenuLabel)`, a facet beside
  `SetEnabled`/`SetChecked`, so the entry reads "Undo Translate (Ctrl+Z)". Identity stays the node's
  id, so the lookups and the invocation log are untouched; only a LEAF's displayed text became state.
- **Ctrl+Z / Ctrl+Y**, beside Ctrl+S in `HandleAuthoringShortcuts` and for its reason (**IN**'s route
  is closed in Edit, so ImGui's view of the keyboard is the authoritative one). Ctrl+Shift+Z is not
  expressible — `IEditorGui::Shortcut` takes one modifier — and Ctrl+Y is what Windows and Unreal use.

---

## SS — Sprite sheets (landed ⑥ S2, 2026-09-02)

**SS1 — THE FRAMES ARE THE TRUTH; the grid only generated them.** `SpriteSheetData`
(`Engine/Subsystems/Resources/Types/`) holds a texture path, a `TDynArray<SpriteFrame>`, a
`DefaultFrame` and the `SpriteSheetGrid` that last sliced it. `SliceGrid` is an explicit, undoable
act that REPLACES the list; nothing recomputes a rect afterwards. That is what makes "edit bounds"
a thing that exists at all, and what lets an irregular packed atlas be described — a stored grid
that derived frames on read could describe only uniform ones, and would silently un-move every
hand-dragged rect.
- **The stack is `MapData`/`MapFile`/`MapResource`'s, one layer over** — data · file · resource,
  each knowing only its neighbours. There is no `SpriteSheetJson` beside the file layer because a
  sheet is a plain aggregate, so the nlohmann macro **is** its serializer; a map needed its own
  layer only because a component payload is opaque json it must not interpret.
- **`SpriteFrame::Name` is an `OpaaxStringID`** (user's call) — **I13**'s split exactly: a frame
  name IDENTIFIES, so what the field wants is four bytes and an integer compare, not a heap string
  per frame. Animation will look frames up by it. **KEPT, ⑥ S3 (**AN5**): a clip step names a frame
  by this id and binds it to an index once. What the promise did not price is that `SliceGrid`
  leaves frames UNNAMED, so the feature arrived owing an authoring button —
  `SheetOps::AutoNameFrames`, without which a freshly sliced sheet has nothing a clip can name.** It forced `Core/String/OpaaxStringIDJson.h`, whose
  one rule is the one `MapJson` had hand-written since M5: **the TEXT crosses, never the id**,
  because a pool index is built in whatever order a process happened to intern things. An invalid
  id writes EMPTY, not `"None"` — which is what `CStr()` answers and would read back as a name.
- **Pixels are floats** (Unity's `Rect`, Godot's `Rect2`): the UV division is float anyway, and
  `Vector2F` draws in the Inspector with no new drawer. **Not `Bounds2D`** — that is a *world-space*
  centre + half-extent built for picking, and sharing the type would invite passing one where the
  other is meant.

**SS2 — `MakeFrameUV` is one named function BECAUSE of the V flip.** `TextureResource` decodes
bottom-up since GL samples that way (**I16**), while a frame's `Offset.y` counts from the TOP —
which is how an artist and every atlas tool count. Getting it backwards draws a plausible-looking
WRONG frame rather than failing, so it is a free pure function with cases asserting a known cell of
a 64×64 sheet. Degenerate input answers the whole texture, never a division by zero.

**SS3 — A sprite names a sheet OR a texture, and `Sheet` wins.** Both survive because a plain image
— a backdrop, a UI panel — must not need a `.opaaxsheet` beside it to be usable. `SpriteComponent::
Frame` is `Int32` with **−1 meaning the sheet's own `DefaultFrame`**: one field says both "which
frame" and "I have no opinion", and `SpriteSheetData::FrameAt` resolves the sentinel so the renderer
and the editor cannot disagree about what it shows.
- **`FrameAt` answers NULL for an out-of-range index rather than clamping.** A caller that silently
  drew a different frame would be the wrong-answer failure; `RendererManager` warns ONCE per sheet
  and falls back to the whole texture, which is **BO4c**'s skip-and-say-so one level down.
- **`Size` still decides world size.** The frame decides *what* is drawn, `Size` × `Scale` decides
  *how big* — **I17**'s "an extent is what a thing IS" split, unchanged.
- **`SpriteSheetResource` does NOT `Acquire` its texture**, and the reason is placement, not
  laziness: `Acquire` needs an ABSOLUTE path and asset→absolute lives in `IPaths`, an app service
  the Resources layer does not reach. `RendererManager` — the one adapter allowed to — resolves both
  through the texture cache it already owns. *This also corrects `MapResource.hpp`'s standing note,
  which blamed "no component names a texture yet": one has since ④, and the real blocker is the
  path resolver. The day `LoadContext` carries one, maps and sheets adopt `Acquire` together.*

**SS4 — THE EDITOR EDITS ITS OWN COPY, AND SAVE IS WHAT PUBLISHES IT.**
`EditorSpriteSheetDocument` owns a `SpriteSheetData`, unlike the map and level documents which are
cursors into state the engine holds. It has to: the copy in the `ResourceManager` is what the
RENDERER draws, so editing that one would change the running game mid-edit. **The publish half is
not optional and was missed once** — a sprite already holding the sheet kept drawing the first
parse, so re-slicing changed the file and nothing on screen ([[L75]]). `SheetOps::Save` therefore
writes the file *and* calls `ResourceManager::Reload`.
- **The dirty marker is DERIVED**, never a flag: `IsDirty` re-serializes and compares against a
  baseline taken at Open/Save. A bool would have to be set by every mutation, and the one that
  forgets is a `*` that lies.
- **Every sheet undo step carries its sheet's PATH**, which no entity step needs. One sheet is open
  at a time and the stack outlives that, so without it an undo after opening a second sheet would
  write the first one's frames into it. A step whose sheet is not open is a **no-op with a warning**
  — a silently skipped undo is indistinguishable from one that had nothing to do. *Trigger for
  revisiting: a multi-document sheet editor.*
- **Slice REFUSES to produce nothing.** Replacing an authored list with an empty one because a cell
  size was mistyped is data loss with an undo step on it. It clamps a now-dangling `DefaultFrame` in
  the SAME step, so one Ctrl+Z puts both back.

**SS5 — `ResourceManager::Reload<T>` keeps the slot, the refcount AND the generation.** That is its
whole design: every `ResourceRef` already held stays valid and simply resolves to the new payload.
Bumping the generation would stale exactly the refs it exists to update. Three refusals around it,
each deliberate:
- **Not resident answers `false` and never loads** — the honest answer to "nobody was looking".
  Turning a save into a load would pull files into memory for nobody.
- **A failed re-read KEEPS the resident payload.** Blanking a live resource because a file went
  missing is worse than something stale.
- **The load runs UNLOCKED** (as `LoadInternal` runs `FillSlot`, since a composite's `Load` recurses
  through the manager) and the swap is re-checked under the lock; a slot released in between simply
  drops the freshly parsed payload.
- It is the **first piece of the hot-reload item** on `Docs/TODO.txt`, scoped to one call site. The
  general form — a file watcher — is ⑧ and needs nothing here to change.

**SS6 — The editor's rect arithmetic is ONE tested body, two units.** `Editor/UI/EditorRectGeometry.h`
(was `WindowFrameGeometry.h`): the eight-region hit test with corner priority and the resize with
left/top origin compensation are identical for a client-drawn WINDOW frame (`Int32` screen pixels)
and a sheet FRAME (float texture pixels), so they are templates. **The window's own vocabulary
survives as aliases** — `WindowFrameRect`, `EWindowFrameEdge`, `HitTestFrame`, `ResizeFrame` — so
the title bar's code and its eight cases are untouched, which is what made generalising a
hand-verified feature safe rather than brave.
- **`ClampRectInside` is the sheet's rule and does not belong to the window**: a window may
  legitimately hang off a monitor; a frame may never name pixels the texture lacks. It SLIDES a
  dragged frame back rather than shrinking it — the author moved it, they did not resize it.
- **`TPropertyDrawer<OpaaxStringID>` submits on ENTER**, and that is the design rather than a
  preference: an id is interned, the pool is never reclaimed, and per-keystroke interning would
  leave `"H"`, `"He"`, `"Her"` and `"Hero"` in it forever — the case `OpaaxStringID::Find`'s own
  note names. An emptied field is the INVALID id, not an interned empty string.
- **`IEditorGui::IsPanelWindowFocused` is on the CHROME seam, not the widget one.** **MR2h** rules
  out a decorate-the-previous-item query in the *value* vocabulary; this is host chrome, whose
  windows are already an ordered `Begin`/`End` pair, so "the window just begun" is well-defined.
  `EditorPanels` records it per frame and Ctrl+S routes to Save Sheet or Save Map from it — one
  chord, one command each, no second implementation.

---

## AN — Animation (landed ⑥ S3, 2026-09-03)

**AN1 — ANIMATION ADDS NO RENDER PATH.** `SpriteAnimationSubsystem` writes into the
`SpriteComponent` that `RendererManager::ResolveSpriteDraw` already reads (**SS3**), so `Renderer2D`,
`RendererManager` and every shader are untouched by this whole block. It is **D7** exactly:
authoring data plus a world subsystem, never a polymorphic component. *The measure of the design is
that the diff contains no renderer file at all.*

**AN2 — A CLIP IS ITS OWN ASSET; THE LIBRARY IS AN ALIAS TABLE.** Two formats, and the split is the
user's call (2026-09-03) with a stated reason: a notify track, curves and events all hang off a
*clip*, so making the clip the asset means adding them later is one field in one file, with no
library, component or subsystem change. It also makes a clip reusable across characters.
- `.opaaxclip` — `AnimationClipData`: a sheet, ordered `AnimationStep`s, `Fps`, `EAnimPlayMode`.
- `.opaaxanim` — `AnimationLibraryData`: `{Name -> clip path}` entries plus a `DefaultClip`. Its
  whole job is to let gameplay say `OPAAX_ID("Run")` instead of naming a file, so switching state is
  an integer compare rather than a string copy.
- **A component names a library OR one clip directly, and Library wins.** The THIRD instance of an
  idiom already in the tree (`SpriteComponent`'s `Sheet`|`Texture`, a clip's sheet|texture-list), so
  a spinning coin costs one `.opaaxclip` and a hero costs a library plus five clips. A one-off prop
  must not need two assets to exist.
- **`Hold` counts TICKS, not seconds** (Unreal's `PaperFlipbook` keyframe, Aseprite's frame): pixel
  timing is quantized, so integers remove drift from the total. A `Hold` of 0 reads as 1 — dropping
  a mistyped step would shift every index after it, silently.

**AN3 — `SampleClip` IS STATELESS, and that is the playback model, not an optimisation.** The step
is a pure function of the elapsed time — never of the previous frame — so playback cannot drift and
a frame hitch SKIPS rather than queueing up the frames it missed. It is free and pure for
`MakeFrameUV`/`SliceGrid`/`PlanQuadBatches`' reason: it needs no world, no GL and no clock to test,
and 21 cases pin Loop's wrap, Once's clamp, PingPong's `2n-2` period (the ends are not held twice)
and every degenerate that could divide by zero or index past the steps.
- **`bFinished` means "nothing new will be shown from here"** — the end of a `Once` clip, and any
  clip that cannot advance at all. A Loop or PingPong clip that CAN advance never finishes, because
  a caller that stopped accumulating on one would freeze it.

**AN4 — THE SUBSYSTEM IS PLAY-ONLY, and that is what makes writing to authored components safe.**
`ShouldCreate` returns Play worlds only (**WS2**), so in an Edit world the type is never
*constructed* — proven by `World 'Main' (Edit) — 0 of 2 subsystem candidate(s) created`. A PIE clone
is a separate world (**WM6**), so what it writes is thrown away with the clone and the authored map
is untouched. Authoring preview is the clip panel's job, on its own copy (**SS4**).
- **While an animator drives a sprite it OWNS that sprite's `Sheet`, `Texture` and `Frame`.** The
  sprite's authored values are what shows when no animator is present — which, since this is
  Play-only, is exactly what the editor viewport keeps showing. `bPlaying = false` freezes on the
  current step rather than reverting.
- A `TResourcePath` is assigned only when it CHANGED: it is a string copy, and this runs per
  animated entity per frame.

**AN5 — A FRAME NAME BECOMES AN INDEX ONCE, AT BIND — never per tick and never per entity.**
A clip step names a sheet frame by `OpaaxStringID`, which is what **SS1** promised when
`SpriteFrame::Name` was made an id. Binding builds one `{clip path -> per-step index}` table; a name
the sheet does not have resolves to -1, warns once, and that step is skipped rather than drawing
some other frame (**BO4c** one level down).
- **Name-based costs an authoring step that index-based would not, and the fix is one button.**
  `SliceGrid` deliberately generates UNNAMED frames, so a freshly sliced sheet had nothing a clip
  could reference — 64 renames before the first clip on a 64-frame sheet. `SheetOps::AutoNameFrames`
  fills only the blanks (an authored name is never overwritten, so it is safe to press twice) and
  guarantees UNIQUENESS, because the binder resolves by FIRST match and a duplicate would silently
  animate the wrong picture. The same rule is why `LibraryOps::CommitEntryEdit` REVERTS a duplicate
  clip name.

**AN6 — A COMPONENT MAY HOLD TRANSIENT STATE, and the json macro is what makes it transient.**
`SpriteAnimatorComponent::PlayTime` and `BoundClipPath` are in neither
`NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` nor `OPAAX_PROPERTIES`, so they cannot reach a
`.opaaxmap` and the Inspector does not offer them. A PIE clone round-trips through the map snapshot
(**MP5**/**WM6**), so a cloned world starts every animation at zero for free, with nothing to reset.
Unreal's `PaperFlipbookComponent::AccumulatedTime` is the same shape.
- The bound identity is the resolved clip's PATH, not its name: it is the one thing both component
  routes share, so a clip SWITCH restarts at zero whichever way the clip was named.

**AN7 — `WorldContext` GAINED `IPaths` UNDER WS3's OWN CLAUSE.** WS3 said adding a member later "is
one line and breaks no existing subsystem, so this starts at what has callers"; this is the first
world subsystem to load an ASSET, and `ResourceManager::Load` takes an absolute path while a
`TResourcePath` is deliberately relative (**MP8**). One member, one argument at
`WorldManager.cpp`'s `SetContext` — where `m_Paths` was already resolved — and the guard beside it
gained `m_Paths` so the new dereference is checked like its three siblings.
- It is also **the engine's first world subsystem**, so `Engine` gained a fourth
  `RegisterNativeWorldSubsystems()` beside components, resource formats and engine subsystems. Until
  ⑥ S3 every candidate came from a game or editor module, so the engine owned a registry it never
  wrote to — **I15**'s `RegisterNativeDrawers` gap, one route over.

**AN8 — THE EDITOR HAS TWO DOCUMENTS, AND BOTH OWN THEIR DATA.** `EditorAnimationClipDocument` and
`EditorAnimationLibraryDocument` are **SS4** verbatim: the copy in the `ResourceManager` is what a
PLAYING entity animates from, so editing that one would change a running game mid-edit. `Save`
writes the file **and** `Reload`s it, which is the half [[L75]] records as having been missed once.
Every undo step carries its document's PATH; a step whose document is not open is a no-op with a
warning.
- **The clip preview runs on the PANEL's own clock.** An Edit world has no animation subsystem by
  construction, and what is previewed is the document's copy, which no world has ever seen. Its
  frame crop LERPS inside the UVs `GetTextureImage` reported rather than recomputing them, so the
  panel cannot hold a second opinion about which way up a texture is (**I16**).
- **Policy lives in the ops, not in the drawers and not in the panel.** An `AnimationLibraryEntry`
  is `CReflected`, so `DrawProperties` gives the name field and the typed drop target for free — but
  a property drawer sees one value and knows nothing about the rest of the library, which is why the
  gesture closes through `LibraryOps::CommitEntryEdit`.
- **`DefaultClip` RIDES IN THE SAME UNDO STEP AS THE ENTRY LIST**, because the edits that dangle it
  are the edits to the list: a rename orphans it, a remove deletes it. A dangling default does not
  fail — `Find` falls through to the FIRST entry — so the library silently plays a different clip
  than it names, and a different one again once the list is reordered. A rename CARRIES it; every
  other list edit re-validates and CLEARS it, cleared rather than repointed because which clip
  inherits the role is the author's call and the empty default already *means* the first entry.
  **This is `SheetOps::Slice`'s `DefaultFrame` clamp, one asset over — and it shipped missing,
  caught only because the user's own renaming produced the dangling case within the hour.** The
  general shape: *whenever a type holds a NAME that points into a list it also owns, every list
  verb is a verb on that name too.*
- **Creating a clip or a sheet from nothing is deliberately NOT here.** No `New Sheet` verb exists
  either, and `Docs/TODO.txt` reserves asset creation for a factory pattern ("Create asset type
  (Unreal pattern? asset action + Factory?)"). Building a one-off menu entry now would pre-empt that
  design. **Trigger: that factory, or the first time copying a file to start one costs real time.**

**AN9 — Named, not built:** notify tracks (`{Uint32 Tick; OpaaxTag Tag;}` — **I14**'s tag is already
the right type and the subsystem already reaches the bus through `WorldContext::Events`, so it is
one field plus a publish), a state machine or blending, a per-entity start offset for crowd
de-sync, multi-document editing. Each has no reader today (**X5**), and none of them changes a shape
above — which is the whole point of **AN2**.

---

## TX — Text rendering (⑥ S4, landed 2026-09-03, USER-VERIFIED 2026-09-04)

**TX1 — A FACE IS A FILE; A FAMILY IS AN ALIAS TABLE OVER FACES.** `FontFaceResource` is one `.ttf`:
one subset, one weight, one slant, one baked atlas. `FontFamilyResource` (`.opaaxfont`) maps
`FontStyleKey{Subset, Weight, Width, Slant}` → a face path, and holds nothing else.
**AnimationLibraryData's split, for AN3's reason** — the face is the unit that grows, the table is a
pure alias — and it is why 162 Roboto cuts cost one asset instead of 162 decisions.
- **A component may name either** (`TextComponent.Font` vs `.Face`), and **Font WINS**. That is
  `SpriteComponent`'s Sheet-over-Texture precedence and the same justification: a single label, a
  debug readout, a game whose whole UI is one weight, must not need a family asset beside it.
- **The family has NO default entry**, unlike its animation sibling. A clip alias is a string that
  can be absent; a style key cannot be — the four axes always have a value — so there is no "no
  opinion" case to answer.

**TX2 — THE BAKE ASKS THE FILE WHAT IT HAS. There is no range parameter to get wrong.** `FontBake`
scans `0x20..0x33FF` with `stbtt_FindGlyphIndex` and packs whatever answers. A fontsource subset
bakes its subset; any other `.ttf` bakes everything it carries. ~13k cmap probes, a few ms.
- **0x33FF is the CJK line, drawn deliberately** — the last code point before CJK Extension A.
  Chinese and Japanese need a DYNAMIC atlas, not a bigger window: raising the bound would not make
  them work, it would overflow the 2048 cap. When that day comes the change is inside `FontBake` and
  `FontFaceResource`; **`FontFaceData`'s codepoint-keyed lookup does not move**, which is the
  property that made "static now, dynamic later" a safe answer rather than a deferral.
- The atlas starts at 512 and **doubles**, failing loud past 2048. Latin Bold at 224 glyphs really
  does need 1024 — the grow path is exercised at every boot, not theoretical.
- Kerning is an N² walk over the covered set, **skipped with a warning above 512 glyphs**: `symbols`
  and `math` carry a thousand glyphs nobody kerns. stb 1.26 reads GPOS, so real faces do kern
  (2495 pairs on latin-700).

**TX3 — GLYPHS ARE KEYED BY CODEPOINT, AND THAT IS THE WHOLE DIFFERENCE FROM THE RETIRED M5.** The
old shape was a 95-slot array indexed by `char - 0x20`; a Greek face could not exist in it. UTF-8
decoding joins the path boundary in `Core/String/OpaaxUtf8.h` (**I7**) — same invariant, read from
the text end. `Utf8::Decode` **always advances on a non-empty string**: a decoder that can stand
still turns one bad byte into a hang, which is the only failure mode here that is not survivable.

**TX4 — THE DATA IS PORTABLE, THE RESOURCE IS NOT.** `FontFaceData` (glyphs, metrics, kerning) lives
under `Renderer/Text/`; `FontFaceResource` composes it under `Resources/Types/` the way
`TextureResource` composes an `ITexture2D`. **Resources → Renderer is the allowed direction**, never
the reverse, which is what lets the whole layout walker be unit-tested with no device. `Text2D`
consumes a `FontFaceView{Data, Atlas}` — the `RenderView` idiom — built by `RendererManager`, the one
adapter allowed to reach the ResourceManager.

**TX5 — MEASURE AND DRAW ARE ONE WALK.** `WalkText` takes a nullable `Renderer2D*`. Two
implementations is how the drawn string and the measured one start disagreeing —
`Renderer2D::SubmitQuad`'s argument, one layer up. One `DrawSprite` per visible glyph, all on one
atlas, so a whole string costs one texture slot and sorts with everything else; a blank glyph submits
nothing.

**TX6 — A MISSING GLYPH DRAWS A TOFU BOX, AND THE SUBSET NEVER FALLS BACK.** Width, slant and weight
all fall back on CSS's own priority order (width first, then slant, then weight) because asking for
Medium in a family that stops at Regular is a *style request*, not a typo. **Asking for Greek and
getting Latin does not degrade — it returns a screenful of boxes**, so the honest miss is the better
answer and the caller can say which script it lacks. Anything inexact warns **once per (family,
style)**, never per frame.
- `FontFaceData::Tofu()` is the ONE definition of "empty but still able to lay out", shared by a face
  that failed to load and a family with nothing in the script asked for. A value-initialised
  `FontFaceData` is NOT the same thing: its zero `PixelHeight` divides by zero the moment a draw
  scales it, and its zero `LineAdvance` stacks every line on one spot.

**TX7 — A TEXT'S BOUNDS ARE ESTIMATED, GENEROUS, AND HANG DOWN-RIGHT OF THE TRANSFORM.**
`EntityQuery::TryGetBounds` is headless by design — hit-testing must not depend on what happens to be
uploaded — so it calls `Text2D::EstimateExtent` rather than `Measure`. **Over-estimating is the
contract**: a box that comes up short makes the tail of a string unclickable, which reads as a broken
entity. Text is the one renderable **not centred on its position**: the transform is where the first
line starts. `DrawRank` reads the text's layer too, or a UI-band label could not win a click from a
Default-band sprite behind it.

**TX8 — THE EDITOR'S OWN TYPEFACE GOES THROUGH THE GUI SEAM** (user requirement, 2026-09-03: *"Go
through GUI API. if we switch to QT it has to easy."*). `IEditorGui::SetUIFont(EditorUIFont)` states a
REQUIREMENT — this file, this size, these scripts must read — never ImGui's spelling; Qt hands the
primary to `QApplication::setFont`. `EditorService` resolves the config's mount paths to absolute
ones and calls it, so a second backend inherits the choice unchanged.
- **The fallbacks are a LIST because the source files are subsetted.** One path cannot express a UI
  that reads Latin, Greek and Cyrillic. ImGui merges them into one `ImFont`; 1.92 loads glyphs on
  demand, so no range table is needed. `ImFontFlags_NoLoadError` turns a mistyped path from an
  `IM_ASSERT` before the first frame into a warning and the default font.
- Applied at Init, so both drawn config fields carry `NeedRestart` and the drawer prints "(restart)".
  `UIFontFallbacks` is json-only — no drawer draws a list, the split `SpriteSheetData` already makes.

**TX9 — THE PREVIEW ROUTE IS A CHROME FACET, NOT A CHAIN OF IFS.**
`ResourceTypeBuilder::SetPreview<T>(drawer)` stores a `FResourcePreviewOpen`; the panel holds an
`IResourcePreviewClaim` per open entry and **names no resource type at all**. Registry → live object,
**MR2g**'s shape for every other route.
- **The claim travels WITH the drawing, and it must:** `ResourcePool::Release` defers an unload to the
  next `CollectGarbage` pump, so a drawer that re-`Load`ed each frame would re-bake a font from disk
  the first time the pump landed between two frames.
- The drawers live in `Editor/Resources/ResourcePreviewDrawers.{h,cpp}`, not as lambdas in
  `EditorService` — that composition root has zero `ImGui::` and keeps it (**GIZ8**).
- **A font atlas draws with STRAIGHT `(0,0)-(1,1)` UVs where a texture needs them swapped.**
  stb_image flips on load, so a `TextureResource`'s row 0 is the image's *bottom*; stb_truetype writes
  top-down, so an atlas's row 0 is its *top*. Two buffers, opposite orientations, one y-down widget —
  the F-Text-1 bug of the retired M5, now stated where both call sites can read it.

**TX11 — THE FAMILY EDITOR IS A MATRIX, AND THE FALLBACK IS PRINTED** (landed 2026-09-04).
`FontFamilyPanel` copies `AnimationLibraryPanel` in every respect but one: Roboto is 162 entries, and
scrolling 162 rows to answer *"do I have Greek Bold Italic?"* is not an answer. So the table is the
question a family is actually asked — **subset across, weight down**, for one (width, slant) plane at
a time. 81 cells, one screen, reads as coverage.
- **A cell is a verb**: `*` selects that face, `?` is one that names no file yet, `+` adds an entry
  already carrying its style. Which is why the panel has no Add button — an add always has a style.
- **A Resolve section asks the family what a `TextComponent` would ask** and prints the answer:
  exact, or which of width / slant / weight it fell back on, or "no Greek face — text asking for it
  draws a row of boxes". **TX6**'s ladder is otherwise unreadable from a matrix of the cuts that DO
  exist; you would meet it in the viewport instead.
- `FamilyOps::CommitEntryEdit` **refuses a duplicate style** the way `LibraryOps` refuses a duplicate
  name, for the identical reason: `Find` resolves by first match, so a second entry for one style
  makes the first unreachable. **Reverted, not refused** — the drawer has already written it.
- **No `MoveEntry`.** Order is not meaningful: `Find` scores every candidate and stops on an exact
  match, so first-wins only decides a tie, and a tie is the duplicate the rule above forbids.

**TX12 — THE ACTIVE SOURCE IS DERIVED, NEVER STORED** (landed 2026-09-04). Three components name a
resource two ways with one winning — `SpriteComponent` (Texture/Sheet), `SpriteAnimatorComponent`
(ClipAsset/Library), `TextComponent` (Face/Font) — and the generic drawer showed both fields with no
hint which the renderer would use. Each now opens with a **Source** line, and says so when both are
set: *"Texture is ignored while Sheet is set."*
- **A stored "which type" enum is the better model and is deliberately NOT built.** It is what
  Unity's Draw Mode is — but a component gaining one needs a DEFAULT, and no static default is
  right: a map already carrying a Sheet, with the field defaulting to Texture, **silently stops
  drawing at load**. That is a migration, not a field. Reading the same rule the renderer reads
  means the picker cannot disagree with the frame.
- **The custom drawer adds one line above the same `DrawProperties` fold**, never a hand-listed
  field set. Enumerating fields in a custom drawer is how a new property silently stops appearing.
- **Do not pre-wire `SetIcon` to art that does not exist yet.** Tried on 2026-09-04 so a later
  drop-in would be free; the browser's fallback works exactly as documented (glyph + one warning) but
  the missing file costs **four `[error]` lines at every boot**, which is a worse trade than the one
  line it saves.

**TX14 — ONE WALK, MANY SINKS: `Text2D::Layout` is the primitive** (landed 2026-09-04, user:
*"I could be cool to have an example text like window .ttf in the opaaxfont panel"*). The family
panel renders a sample string with the selected face's own glyphs — Windows' font viewer, where a
family is chosen. It could not go through `Renderer2D`, and re-implementing the layout in the editor
is exactly what **TX5** forbids: a preview that lies about the thing it previews is worse than none.
- So `Layout(text, origin, face, params, sink)` is the primitive and **`DrawString` is a sink over
  it**. `Measure` is the empty sink. Adding a third consumer costs a lambda.
- The seam is a **`TextQuad`** in world units — centre, size, atlas rect, and a `bTofu` flag. It
  carries the WORLD's V convention, so a y-down sink negates Y and swaps the V components back:
  **TX9**'s asymmetry arriving per glyph instead of per atlas.
- **The ImGui half cannot be tested** (it needs ImGui), so the QUADS are: one per visible glyph in
  reading order, none for a space, the origin being top-left with Y going up, a newline dropping
  exactly one line step, tofu carrying no atlas rect, and the empty sink matching `Measure`.
- The panel holds **one claim**, re-taken when the selection points elsewhere and released in
  `Shutdown` (**LC3**). An un-uploaded atlas says so rather than drawing nothing — the bake runs on
  `Load`, the upload at the next pump (**TX4**).

**TX13 — A STRING CAN SAY IT IS A BLOCK OF TEXT** (landed 2026-09-04, user: *"we cannot really edit
text properly. I cannot jump line etc..."*). `EPropertyFlags::Multiline` + `IEditorWidgets::
InputTextMultiline`, so `TextComponent::Text` is authored as prose and Enter inserts a break.
- **The flag is on the META, not the type**, and that survives the header's own "not a widget hint"
  rule: a window title and a sign's inscription are both `OpaaxString`, and only one can hold a
  `'\n'`. That is exactly "what a field's TYPE cannot say about it" — the same shape as `SetRange`.
- **Its own seam entry, not a flag on `InputText`**: every toolkit splits the two widgets (ImGui
  `InputText`/`InputTextMultiline`, Qt `QLineEdit`/`QPlainTextEdit`), so a caller is choosing a
  shape rather than an option.
- A block gets a 4096 buffer where a line keeps 512; the refuse-rather-than-truncate rule is
  unchanged at both sizes.
- **Ctrl+Z inside the field is ImGui's**, not the editor's: `InputText` claims the chord with its own
  item id, which outranks `ImGuiInputFlags_RouteGlobal` while it is active. Verified in
  `imgui_widgets.cpp` rather than assumed, and nothing was added to arbitrate it.

**TX10 — Named, not built:** screen-space text (a stats overlay pinned to a corner) is a second ortho
pass, and faking it by moving a world position against the camera is the thing multi-view exists to
stop. *Amended 2026-09-04: multi-view LANDED (**MV**), so this is no longer blocked — and the user
then deferred the overlay itself (*"We have stats panels, for now is very ok"*). What it now waits
on is the HUD, whose design they are keeping.* Alignment, word-wrap, rotation,
outline/shadow, SDF and per-glyph cross-subset fallback each have no caller (**X5**). **Width has no
files**: the static Roboto export carries no width axis, so only `Normal` resolves until a
`Roboto_Condensed` family drops in — the axis is in the key so that costs no file-format change.

---

## MV — Multi-view (landed ⑥ S5, 2026-09-04, USER-VERIFIED)

**MV1 — A frame is a LIST of views, and each is claimed ONE FRAME AT A TIME.**
`RendererManager::SubmitRenderView(IRenderTarget&, const CameraView&, bool bDrawOverlays)` replaced
`SetPrimaryRenderTarget` through the whole route (`IEngine` / `NullEngine` / `Engine` /
`RendererManager`). Producers submit in `OnPreRender`; `RenderFrame` loops the list; `Render` clears
it beside the debug queue. **This is F4's immediate-mode contract applied to views**, and it buys the
same thing: a producer that wants a view re-submits it, and one that stops — hidden, destroyed, mid
teardown — stops being drawn with nothing to unregister. `ViewportPanel::Shutdown` lost its
"clear the target first, in this order, or a live frame reads a dangling pointer" ordering rule
entirely; that window is now unrepresentable rather than documented.
- **The RUNTIME path is a SUBMISSION, not a branch.** An empty list makes `RenderFrame` submit the
  backbuffer framed by the active world — what it always drew. One line, no second code path to keep
  in step, and `Sandbox.exe` never learns any of this exists. *(**BO4c**'s never-a-black-frame rule
  one level down, and the shape **CAM1**'s default-constructed fallback already uses.)*
- **`RenderPass` composes the matrices against THAT target's pixels**, so **CAM1**'s split holds per
  view: the submitter says where it is looked at from, the adapter is what knows pixels. Two views of
  different sizes frame the same world correctly with neither producer knowing about pixels.
- **A zero-size target skips its PASS, not the frame** — the early-out moved from per-frame to
  per-target. A frame with no drawable pass still opens no device frame.
- **The debug queue is READ per pass and CLEARED once per frame.** Two views that both want overlays
  each draw them.
- **`bDrawOverlays` had two callers on day one** — true for the Viewport, false for the Camera
  Preview, which must look like the GAME. That is the rule `EnqueueEntityIcons` already states for
  Play worlds, not a new one, and it is why this is a field rather than a spec (**X5**).
- **A per-pass `ELoadOp` was planned and REFUSED at build time.** `Load` is only needed when two
  passes share ONE target; every view here owns its own, so all `Clear`. Adding the parameter would
  have been an enumerator with no caller — the trap this whole block was shaped to avoid.
  **`ELoadOp::Load` still has zero callers**, and `ICommandBuffer.h` names the one it waits for: the
  HUD, the first thing that draws twice into one target.

**MV2 — The block was defined by its CONSUMER, because the plumbing alone cannot be verified.**
With one pass still composed, the change renders byte-identically: no smoke run, no log line and no
test can tell it from no change, and `Renderer2D`'s pass loop needs a GL context so the headless
suite cannot reach it either ([[L23]] / **X5**). Every cheap consumer considered had a cheaper answer
that is **not** multi-view — a camera-framing rectangle is `DebugDraw`; asset previews are ImGui
images the clip editor already draws; the selection outline's zoom-thinning is `WorldPerPixel()`,
which the grid already uses. See [[L80]].

**MV3 — `CameraPreviewPanel` is the second view, and READ-ONLY is what makes it cheap.** It resolves
the previewed entity's `TransformComponent` + `CameraComponent` — the same pair
`CameraManager::Resolve` reads, so the preview and the game cannot disagree about what a camera means
— and **never writes `World::SetCameraView`**. **CAM1**'s one-view-per-world slot is untouched and
there is no second producer to arbitrate. *A second **editing** viewport is the change that would
move the view off the World; it is deliberately not this.*
- It owns its FBO + `OffscreenRenderTarget` and copies `ViewportPanel`'s deferred-resize shape.
- **A hidden panel submits nothing**, so it costs no pass while closed (`EditorPanels::IsVisible`).

**MV4 — The preview is STICKY: a camera claims it, nothing else disturbs it.** *Corrected in use by
the user (`dcd34b3`) — the first version followed the raw selection, which blanked the panel on every
click on ordinary geometry, and you select geometry to position it AGAINST the framing.*
`TrackSelection` banks the primary selection **only when it carries a camera**; the view is
re-resolved from that id **every frame**, so a move or an `OrthoSize` edit lands the same frame while
a deleted entity falls back rather than showing a stale picture. Tracking runs while hidden, so
opening the panel shows the camera already selected.
- **An `EntityID` in the PANEL** — not an `Entity`, not a `CameraPreview` object on `EditorContext`
  (which was proposed and deleted). Only entities of the ACTIVE world are ever banked, so the resolve
  rebuilds the handle against that world and no `World*` is stored to dangle.
- **Forgotten on `OnActiveWorldChanged`, and that is CORRECTNESS, not hygiene**: an id means nothing
  in another world and **entt reuses handles**, so a kept one could silently resolve to a different
  entity that happens to have a camera — a plausible, wrong picture.
- **A PIE cycle nevertheless KEEPS the preview, and that is the selection's doing, not this panel's.**
  *Corrected 2026-09-04 after the user ran it (*"preview still with latest when start and stop"*) —
  I had predicted "No camera" by reading my own handler and not its caller.*
  `EditorService::HandleActiveWorldChanged` **retargets the selection by `Guid` first** (**WM3** — a
  clone preserves them; clearing instead *"would throw away the exact guarantee the snapshot core
  exists for"*), so a previewed camera that is also selected is re-tracked on the next frame.
  **The gap that remains, named and left alone at the user's call** (*"keep at it is"*): a preview
  left STICKY on a camera that is not the current selection does fall back to "No camera" across
  Play. The fix, if it ever bites, is SMALLER than the code it replaces — bank the camera's `Guid`
  instead of its `EntityID` and resolve it through `World::FindByGuid`, which is the mechanism the
  selection already uses and which deletes this override entirely.
- **"No camera" is a first-class state**, not an error: what a panel opened from the Window menu
  shows before any camera is picked, and what remains after the previewed one is deleted, loses its
  component, or leaves the active world.

**MV5 — A DRAWER MAY ASK FOR THE EDITOR, and only to call a verb.** `TDrawerRegistry` gained an
optional `Draw(IEditorWidgets&, TDrawable&, TSubject&, EditorContext&)`, detected by the
`CContextDrawer` concept + `if constexpr` — the same duck typing that already tells the custom form
from the generic one, so every existing drawer compiled untouched. The closure carried the subject
all along; only the context is new, and it comes down from the Inspector, which holds it.
- **`CameraComponentDrawer` is the first and only taker**: its own `CollapsingHeader`, then
  `DrawProperties` (so a field added to `CameraComponent` needs no change here), then a button that
  dispatches **`EDITOR_COMMAND_TOGGLE_PANEL`** with the panel's id. **No new command, no new tag** —
  the button and the Window menu entry are one verb, and a key binding would be a third front-end.
- **This does NOT contradict I15.** A field is still written straight through the `T&`; what needs
  the context is a *verb*, which is the registry's business. **The cost is stated in the header
  rather than hidden:** a drawer holding the context can reach the whole editor, and what keeps it to
  a verb is convention, not the type system. Accepted deliberately (user: *"Maybe its good to have
  editor context for drawer too. some custom drawer may need it more often"*), with `Docs/TODO.txt`'s
  "Convert Texture to sheet" as the next expected taker.
- **A `ComponentActions()` registry was proposed and REJECTED** as machinery (*"cant we make it
  simplier"*). The drawer plus the context does the same job with no new route and no new counter.

**MV6 — The counts, and the one that did NOT move.** `panels=13→14`, `titleBar=34→35` (the Window
menu gains an entry per panel), and **`drawers=8` UNCHANGED** — a registration changed FORM, not
count. The discriminating signal for that is in the boot log: the generic form logs itself at
registration, so `Generic drawer: CameraComponent` **disappearing** is the proof the custom form took
over, with Camera now absent beside the three already-custom drawers while Transform and Dummy
remain. *(**L79**'s rule turned around: predict which counts move, and know why one does not.)*

**Growth points, named and not built:** the HUD (theirs to design — one more submitted view, into the
same target, with `ELoadOp::Load` and a pixel projection) · split-screen and minimap (no caller) · a
second **editing** viewport (needs **CAM1**'s slot to move off the World) · the asset preview WORLD
(Unreal's preview scene — its trigger is ⑦ prefabs, the first preview an image cannot fake) · pinning
or stacking several previews (the user declined the stack: *"i do not really like the stack"*).

---

## PH — Physics (⑦-A, P0 landed 2026-09-07)

Salvaged from **M9** (closed with user sign-off 2026-06-12), whose seam survived the architecture it was
written for. The seam and the Box2D backend came back as a restyle; everything above them is rewritten
onto the live tiers. Collision **profiles did not come back** — see **PH4**.

**PH1 — `IPhysicsWorld` is the seam, and it is shaped like the RHI's.** A concrete world is built only
through `PhysicsAPI::Create`, the backend is a config enum resolved once, and **no `b2*` symbol appears
above `Physics/Box2D/`** — a grep gate proves it and is run at every slice close. The interface speaks
engine concepts (world units, Y-up, opaque `BodyHandle`/`ShapeHandle`); each backend absorbs its own
quirks (poll-based events, length units, native filter bits) behind the methods. This is the property
the user asked for by name — *"interfaces to make the physic itself swappable easily"* — so a direct
Box2D call from gameplay is a contract violation, not a shortcut.

**PH2 — box2d is linked PRIVATE, and that is an I1/I2 rule, not a build preference.** It is a static lib
with GLOBAL state (`b2SetLengthUnitsPerMeter`), so `PUBLIC` gave `Sandbox.exe` and `SandboxEditor.exe`
each their own copy of it — **exactly [[L11]]'s glfw bug**, latent only because nothing outside the
engine had ever included box2d. Fixed when the seam landed: box2d moved beside glfw in the PRIVATE
block, its include dir was already private, and physics is reached through the `OPAAX_API`
`IPhysicsWorld` / `PhysicsAPI::Create`. The one instance now lives in the DLL by construction rather
than by nobody having called it yet.

**PH3 — the seam is TESTABLE, which makes it the rare engine path that needs no eyes.** Physics wants no
GL context, so `Engine/Tests/Core/Physics/PhysicsSeamTests.cpp` runs the real Box2D backend through the
neutral interface (fall, land-and-rest at a computed height, ray hit + miss, channel-filtered miss,
overlap collection, `MoveCapsule` grounded + free). Because `OpaaxTests` links the **import lib** like a
game exe, those cases simultaneously prove the seam is exported and that no consumer needs the vendor
(**PH2**). Contrast the renderer, where every equivalent claim is a smoke run and a pair of eyes.

**PH4 — a collider carries its CHANNEL and its MASK; there is no CollisionProfile asset.** M9 made the
profile a first-class `IAsset` with a three-state response matrix; that base is retired and the user's
call was channels-only (2026-09-07). `ShapeDesc` already takes `CategoryBits`/`MaskBits`, so the
channel's `CategoryBit` goes straight onto the shape and a profile resource later is **pure addition
with zero migration** — the reason it is safe to defer. `CollisionChannelList.h` stays the X-macro
single source of truth, and its ordinal IS the filter bit index: **append only, never reorder**, or
every saved map renumbers.

**PH6 — PHYSICS IS A PLAY-WORLD SUBSYSTEM, and that one line is the whole PIE story.** M9 ran physics
as an *engine* subsystem with a process-lifetime world and hand-rolled per-play body churn
(`OnPlayBegin` / `OnPlayEnd` / `ClearBodies`, plus a standing worry about Box2D state leaking across
Start/Stop). One tier down, all of it evaporates: `ShouldCreate` returns Play-only exactly like
`SpriteAnimationSubsystem`, so a PIE clone builds its own physics world and takes it with it (**WM6**),
`WorldManager` already forwards `FixedUpdate` to the **active** world only (**WS5**), and **WS8**
already resolves pause/step once per frame — so the subsystem holds no gate of its own. **The editor
proves this by ABSENCE:** an Edit world logs zero `[Physics]` lines, because a rejected candidate is
never constructed.

**PH7 — BODIES ARE RECONCILED EVERY FIXED STEP, NEVER BUILT ONCE — and that is what closes WS7.**
`ReconcileDeadBodies` then `ReconcileLiveBodies` run before `Step`. Entities spawned by the host in
`PostEngineStartup`, by a clone's `Instantiate`, or by gameplay mid-play therefore all arrive by the
same route, and **the post-instantiate hook WS7 named as physics' likely need stays unbuilt** — the
reconcile is a better answer than a hook, because it also covers the cases a hook would miss. Two
consequences worth keeping: a body whose entity died is reaped FIRST, so it cannot emit contacts for a
step it should not be in; and a body whose `BuiltType` no longer matches what its components imply is
**rebuilt**, which is what makes component add-order irrelevant (a `Rigidbody` added after a
`Collider` would otherwise stay static forever — silently, which is why it has a test).

**PH8 — ONE BODY PER COLLIDER; the rigidbody only says what KIND.** A collider with no rigidbody is
**static**, because level geometry is the overwhelmingly common case and should not need a second
component to declare what it already is. A rigidbody with **no** collider is skipped entirely: a body
with no shape is something nothing can touch, moved invisibly by gravity. The collider authors FULL
size and the seam takes half extents — halved once, in `MakeShapeDesc`, so no call site has to
remember which convention it is holding. `TransformComponent::Rotation` is DEGREES and the seam is
RADIANS; both crossings go through `Maths::DegreesToRadians`/`RadiansToDegrees`, the same pair the
renderer uses, and a round-trip test pins it because getting it wrong is silent rather than loud.

**PH9 — `WorldContext` carries `Config`, and the null-guard grew with it.** A world subsystem has no
other route to a config: `IConfigSystem` is an app service and reaching the locator is what **D3**
forbids. It arrived by **WS3**'s stated growth clause, the second member to do so after `IPaths`, and
as the WHOLE `EngineConfigData` rather than a physics-shaped slice — it is one config object, and the
next reader wants a different part of it. **Both existing `WorldContext` test fixtures failed to
compile on the new field**, which is the aggregate doing the job a grep would have missed ([[L10]]).

**PH10 — Physics events are PODs on the bus, and they are PUBLISHED, not enqueued.** Five payloads in
`Physics/PhysicsEvents.h`, shaped exactly like `WorldEvents.h`: adding one is adding a struct, with no
`EEventType` entry (that Tier-1 enum stays closed to OS/window/input) and no central registration. The
dispatch choice is a deliberate departure from the bus's stated default, and the reason is **where
`Flush` sits — at the TOP of `Engine::Loop`, before `Update` and before the fixed-step catch-up loop**.
An enqueued contact would therefore arrive a whole frame later, after the reconcile had already run,
and a frame that ran three fixed steps would deliver three steps' worth of edges in one batch with no
relationship to the simulation that produced them. Immediate dispatch puts the handler directly after
the `Step` that produced the touch, with both entities still live. (Same conclusion as **WorldEvents**
and [[L7]]'s teardown corollary, reached from a different constraint: theirs is pointer lifetime,
this one is step coherence.)

**PH11 — `OverlapStayed` is SYNTHESIZED; the backend only reports edges.** Box2D gives begin- and
end-touch and nothing in between, so the subsystem keeps a live-overlap set keyed by the **normalized**
pair — `(A,B)` and `(B,A)` collapse to one entry, and the value keeps the ordered *(sensor, visitor)*
pair so a `Stayed` re-fires with the same meaning its `Began` had. **The order within a step is
load-bearing: Began → Ended → tick the survivors.** Ending before ticking is what stops a pair that
ended *this very step* from receiving one more `Stayed` after its `Ended`.

**PH12 — A handler MAY destroy an entity, and nothing phantom may follow.** That is the pickup case and
it is supported on both sides: each survivor is **re-validated against the world** before its `Stayed`
rather than trusted, and `RemoveBodyForEntity` scrubs the pairs its body was in (a body with no shape
reports nothing, including its own `Ended`, so a pair that can never end would otherwise tick forever).
Killing the visitor and killing the sensor each yield exactly one `Began` and nothing after — the
assertion M9's close-out records having found by hand, now a test.

**PH13 — COLLIDER OUTLINES ARE A SEPARATE SUBSYSTEM WITH NO `ShouldCreate`, and that is the design.**
`ColliderDebugSubsystem` is the first native world subsystem without a creation filter, deliberately: a
collider must be visible while you are **authoring** it, and that is precisely when `PhysicsSubsystem`
does not exist (**PH6**). So it reads the **components** rather than the simulation and runs in Edit and
Play alike — which costs nothing in accuracy, because in Play the transform is synced back from the body
each step, so the authored pose *is* the simulated one. **Visibility is a CHANNEL, not a mode**
(`DebugChannels::Physics`), which is the only version that also silences it in a dev build of `Game.exe`
— something "just don't register it in the editor" could never have done. The gate is its absence:
the editor logs `Drawing 8 collider outline(s)` in an **Edit** world with **zero** `[Physics]` lines.

**PH14 — Circles and capsules are POLYGONS OF LINES, not queue entries.** `DebugDraw` gained
`DrawCircle`/`DrawCapsule` on ⑦-A P3's caller, and neither adds a drain path, an RHI primitive or a
renderer concept — they build a closed point loop and emit the segments the line queue already carries
(`ToQuad`'s trick, one level up). The builders (`BuildCircleOutline`, `BuildCapsuleOutline`) are **free
and pure**, so the part that can be wrong is arithmetic a test holds without a GL context. Two facts
worth keeping: a capsule whose cap centres coincide **is** a circle and comes out as one; and the
polygon is **inscribed**, so its extent is `radius * cos(halfStep)` rather than `radius` — a test
asserting the exact radius fails, and the code is right (that is how this one was written first).

**PH15 — `DebugBox` carries a rotation because a rotated collider proved the drain site was lying.**
`Renderer2D::DrawQuadOutline` has always taken a `RotationRad`; the debug queue never carried one and
`RendererManager` passed a hardcoded `0.f`. Invisible for three milestones — every prior box was
axis-aligned — until a collider on the physics test map's `-25°` ramp annotated itself with a level
outline. One field, one argument at the drain, and every future box caller inherits it.

**PH16 — A QUERY ANSWERS IN ENTITIES, and an unresolvable hit is a MISS.** The seam speaks raw body
user-data (**PH1**); `PhysicsSubsystem::RayCast` / `OverlapAABB` are the same answers with the entity
decoded, and that decode is the layer's whole reason to exist. Two decisions ride on it: a hit whose
body carries no resolvable entity is reported as **no hit** rather than as a hit on `ENTITY_NONE` —
every caller would otherwise have to check and most would forget — and a query made when there is **no
physics world** answers empty rather than failing, because asking before Play (a tool, a script, an
Edit world) is legitimate rather than an error. User-data is the entity's bits **+1**, so entity 0 is
distinguishable from the seam's reserved 0.

**PH17 — THE KILL VOLUME IS OFF BY DEFAULT, RUNS LAST, AND LATCHES.** Off, because a bounded world is
the wrong default for an endless scroller and a body quietly vanishing is worse than one falling
forever. **Last in the step**, after `DispatchPhysicsEvents`, so every contact and overlap that step
produced is already delivered before anything is reaped — a body that touches something on the way out
still reports it. **Latched** via a set of out-of-bounds entity bits, which is the property that makes
it usable at all: "it fires" would be satisfied by a version firing sixty times a second. Coming back
inside un-latches, so leaving twice reports twice — the honest answer. **Static colliders are skipped
entirely**: one authored outside the bounds is level geometry, and reaping it would delete something
someone placed. The event is published **before** the reap, so a handler still sees a live entity.

**PH18 — A MODE IS A CLIP AND A MOVER IS THE BAG THAT NAMES THEM** (⑦-A P5a, their design). M9 put
per-mode tuning on the component behind `IMoverModeParams::DrawEditor()` — an ImGui virtual inside the
engine — which cannot come back: the engine always builds `OPAAX_WITH_EDITOR=0` and **MR2d** forbids it.
As RESOURCES the knobs get the editor for free and gain what the clip gained in ⑥ S3: a tuning is
authored once and **shared**, and it is the unit that grows. `.opaaxmovemode` is the clip (one tuning
plus the id of the `IMoverMode` that reads it); `.opaaxmover` is the library (an alias table, so
gameplay says `OPAAX_ID("Fly")` instead of a path). `MoverData::Find` is `AnimationLibraryData::Find`
verbatim in shape, **including the rule**: a NAMED mode that is absent answers nullptr rather than
falling back, and only "I have no opinion" gets a default.
*The measurement that made the old shape indefensible: `GroundMoveParams` was 8 floats and
`FlyMoveParams` was 1 — the same speed-at-full-input Ground already had. Per-mode TYPING bought zero
distinct fields.* **ONE resource type, not one per mode**, for the clip's reason — clips differ in
DATA. Which knobs a mode ignores is a PRESENTATION question, answered by `MoveModePanel` opening
`DrawProperties`' fold so a field can be skipped; the engine never has to care, and a mode a game
registers shows everything, which is the honest answer for one the editor has never met.

**PH19 — `IMoverMode` IS Tick PLUS TWO HOOKS, and the resource is why.** No `CreateDefaultParams`, no
`TypeTag`, no assert-and-downcast — every M9 mode opened with all three. `MoverTickContext::Params` is
a plain `const MoveModeData&`. Modes are **STATELESS** (all per-entity state is on the component), so
`MoverModeRegistry` — the FOURTH engine registry, in `EngineRegistries` for that aggregate's stated
reason — owns ONE instance of each. **A mode's name IS an on-disk key**, so unlike a world subsystem
the name is required at registration and a duplicate is REFUSED rather than replacing: a game module
must not silently shadow a built-in that assets already name.

**PH20 — THE MOVER TICKS AFTER PHYSICS, and registration order is the only thing that says so.** A
mover sweeps against the world's shapes, so it must see the poses THIS step produced rather than last
step's; the subsystem manager ticks in registration order, so `Engine::RegisterNativeWorldSubsystems`
puts `MoverSubsystem` after `PhysicsSubsystem` and that line is load-bearing. Two policy rules the
modes own rather than the component: a jump is spent **only when grounded** (otherwise holding the key
is flight), and the edge is consumed **by the mode**, so one request is one jump however many fixed
steps the frame ran. `FlyMoveMode::OnModeEnter` drops carried momentum — without it, switching mid-fall
keeps sinking, which reads as a bug rather than as physics.

**PH21 — THE ALPHA WAS PLUMBED AND DISCARDED FOR THREE MILESTONES; it is consumed now** (⑦-A P6,
M9's P7). `RendererManager::Render` took `double /*Alpha*/` and threw it away, so a fixed-step pose
drew stepped rather than smooth. `TransformInterpolationComponent` holds where an entity was at the
END of the previous fixed step and is **deliberately NOT registered** with the ComponentRegistry: a
saved previous pose would be a lie the first time the map loaded, and since **WM6** copies exactly
what the registry knows, staying out of it leaves every map, snapshot and PIE clone byte-unchanged.
The fixed-step WRITERS record it (physics and the mover, each just before overwriting), the renderer
blends toward the current pose, and **gameplay, queries and picking keep the RAW value** — that split
is what makes this display-only rather than a second source of truth. `ResolveDisplayPose` is free and
pure, so the two cases a screenshot cannot judge are tests: the **shortest-arc wrap** (350° → 10° is
+20°, not −340°, or a body visibly spins backwards) and the **first step**, which has no previous pose
and must not blend from a default-constructed one.

**PH21a — "OFF" IS NOT ALPHA 0, and getting that wrong drew everything one step behind.** Alpha 0 is
the PREVIOUS pose, so expressing the disabled state as `m_FrameAlpha = 0` made every fixed-step entity
render a step late while looking entirely plausible. **The instrument is what caught it**: the blend
counter reported *"Interpolating 5 drawn pose(s) — alpha 0.00"*, and a blend at alpha zero is a
contradiction. Off now skips the blend entirely — also one fewer lookup per entity. The counter stays,
because "the toggle is on" and "something was actually blended" are different claims and only the
second one says the feature works ([[L15]]).

**PH5 — `EngineConfigData::Physics` came back on the clause that deleted it.** The group was removed
2026-08-21 for having no reader, under "each comes back with the system that reads it". It is a real
`EPhysicsBackend` enum (a typo is not expressible, and the editor gets a dropdown), with gravity,
length-units and sub-steps. `EPhysicsBackend` lives in its own small `Physics/PhysicsBackend.h`
mirroring `RHI/RHIBackend.h`, so the config type does not drag the whole seam in behind it — and for
the same reason RHI's did, **`BackendFromString` does not exist**: the enum json bridge parses it.
*The rule is about the SEAM (`IPhysicsWorld.h`, and `PhysicsAPI.h` which includes it), not about
`PhysicsTypes.h`* — that one is PODs over Core with no backend and no interface, so P4's
`WorldBounds` group includes it for `EWorldBoundsResponse` at no cost.

---

## SE — Extension seams (composition-root-only)

Only a **composition root** (an `OpaaxApplication` subclass — `Sandbox`, `EditorApplication`) overrides
these. A *service* or a *panel* never touches the locator (D3). Base implementations are silent no-ops, so
not overriding them leaves runtime byte-identical.

| Seam | Fires | Purpose |
|------|-------|---------|
| `CreatePaths()` | in `BootPaths` (Bootstrap) | choose the `IPaths` impl (runtime `Paths` vs `EditorPaths`) |
| `PreRegisterConfig()` | in Bootstrap | register config types before load |
| `OnProvideServices(locator)` | end of Bootstrap | add host-owned app services (editor adds `IEditorService`) |
| `PreEngineStartup()` | start of `EngineStartup` | before subsystems start |
| `RegisterModules(registrar)` | in `EngineStartup`, **after `Engine().Startup()`** — subsystems up, registries live, **no world yet** (BO4) | route the game module — drives `IRuntimeModule::OnRegister` (**MR**) |
| `OnModulesRegistered()` | in `EngineStartup`, **after** `OnRegisterModules`, **before** `Engine().Startup()` (still no world) | editor registers its D10 extensions and **seals before the first world** (§2). `EditorApplication` overrides → `EditorService::RegisterExtensions`, which registers the editor's own **native** panels first, then drives each `IEditorModule::OnRegister(EditorExtensionRegistrar&)`, then `BindPanelToggles()` (one Window-menu entry per registered panel, native or game — **MR2c**), then seals — **MR2's order one level down** (natives → game module → toggles → seal), so a native panel travels the same route as a game panel with no privileged path. Generic engine-side name (no editor types) — the engine stays editor-ignorant (**D4**). |
| `GetStartupWorldSpec() const` | in `EngineStartup`, **after `OnModulesRegistered`** — the last step of boot (BO4) | **a pure query, not an action**: answer *which level* and *which* `EWorldMode` the app starts in — **not** what the world is called (BO4b). Base impl is real, not a no-op — `IProjectManager::StartupLevel()` + `Play`; `EditorApplication` overrides → `Edit`. The engine then does the work in `IEngine::FinishStartup`, whose `CreateWorld` is the **first** one and so seals the registries. Replaced `CreateStartupWorld()`, which reached through the engine to drive `WorldManager` itself |
| `PostEngineStartup()` | end of `EngineStartup` | after subsystems start **and the startup world exists** (editor inits `EditorService`; Sandbox populates the world) |
| `TickFrame()` | per loop iter | base = `Engine().Loop()`; editor wraps it UI-begin → Loop → UI-end |
| `OnEvent(event)` | window callback, per event | base = app sink (close/resize→bus); `EditorApplication` overrides → `EditorService::RouteInput` first (S11), so the editor sees events before the bus |

---

## MR — Module registration (D9)

A game module is an **`IRuntimeModule`** (`Application/IRuntimeModule.h`); its `OnRegister` registers
**into** a `ModuleRegistrar`, invoked by the host's `RegisterModules` seam before any world exists:
```cpp
InRegistrar.Components().Register<TransformComponent>();      // → ComponentRegistry v2 (M3)
InRegistrar.WorldSubsystems().Register<WaveSpawnSubsystem>(); // → WorldSubsystemRegistry (LIVE, M4 — see WS)
```
**MR0 — Registries live on `Engine`, in one `EngineRegistries` aggregate** (`Engine/Registries/`, user call
2026-07-28). Editor.md §2 has said "Engine builds the registry" since v3; M3 briefly hung `ComponentRegistry`
off `WorldManager` and that was wrong for three reasons: a registry is **type metadata**, not one
subsystem's state (the Inspector reads it too, and it is not "the world manager"); **there is more than
one** — `WorldSubsystemRegistry` lands in M4, `ResourceFormatRegistry` on 2026-08-20 (**MR1a**) — and
hanging each off whichever subsystem happens to read it
turns that subsystem into a bag; and **registration is a boot-order concern** (MR2), which is the Engine's
business. Consequences: the engine registers its **own** native types in the `Engine` **constructor** —
`RegisterNativeComponents()` + `RegisterNativeResourceFormats()` — before any subsystem exists;
`WorldManager` **borrows** a non-owning `EngineRegistries*` (injected through
its subsystem factory, so it never reaches for the Engine) and seals it on the way to the first world; and
`BindEngineRegistries(EngineRegistries&)` is one call that does not grow an argument per registry.
*Corrected 2026-08-20: this said `Engine::RegisterNativeTypes()`, a function that has never existed. It
also implied `BindEngineRegistries` was in use — it had zero callers until MR1a, which is precisely how a
new route shipped unbound.*
- **Each registry lives with its DOMAIN; the aggregate only aggregates.** `ComponentRegistry` is in
  `World/Components/`, `WorldSubsystemRegistry` in `World/Systems/`, `ResourceFormatRegistry` in
  `Engine/Subsystems/Resources/`. `Engine/Registries/` holds `EngineRegistries.h` and nothing else.

**MR1** — The call-site API is **final now**; only the route *bodies* change (M0 counts; M3/M4 forward to
real registries). Do not change how modules call in. **`Components()` went real in M3** and the call site
did survive verbatim, because the authoring name is *optional*: omitted, `ComponentRoute` derives the C++
type's leaf name (`Opaax::DummyComponent` → `"DummyComponent"`). That derived name is the key written into
map files, so renaming the C++ type orphans components already saved — pass an explicit name to pin it.
Not silent when it happens: `MapFactory` warns per unknown component as it skips them.
**`ModuleRegistrar` moved to the ENGINE layer in M3** (`Engine/Modules/`): it exists to front the engine
registries, and by **I4** a registrar that knows about component types knows about worlds. It could live in
Application only while it knew nothing but a count. `OpaaxApplication` holds it behind a forward declaration
so **no Application header includes from `World/` or `Engine/`** — a discipline the tree has never broken.
**MR1a — …except where the M0 skeleton guessed a payload that did not exist yet** (M2d, 2026-07-27).
`AssetTypes().Register<TAsset, TActions>()` presumed an asset *type* to bind; the engine has no such type
(the `CResource` system is load-by-path, and `Legacy/Assets` is unlinked), so the route graduated to
`ResourceTypes().Register(ResourceTypeDesc{...})` — keyed by file extension, no template. **The test:** a
skeleton call site is binding when its payload already exists in some form (`Drawers<TComponent,TDrawer>`
— both real); it is a *guess* when it names a type nothing defines. Amend the contract rather than bend
the design to a placeholder's shape, and amend `Docs/Architectures/Editor.md` in the same change.
- **The premise expired on 2026-08-20, and the test above survived it verbatim.** `LevelResource` and
  `MapResource` (**WM4**, 2026-08-03) are real `CResource` types, so "the engine has no such type" stopped
  being true — and the route became `ResourceTypes().Register<T>()` after all. What forced it was not
  their existence but **arity**: a resource type has MANY extensions (`.png`/`.jpg`/`.tga` are one
  loader), so an extension-keyed table needs N entries per type, each repeating one icon and one closure.
  The right owner of a many-to-one table is the engine.
- **Extension → type is `ResourceFormatRegistry`, the third `EngineRegistries` member** (**MR0**), NOT a
  `ResourceManager` feature: the manager mints its dense ids *lazily on the first `Load<T>`*, so it cannot
  enumerate its own types at all, and its header freezes its surface against exactly this ("editor type
  info … is a separate system CONSUMING this API"). Placement follows the extensibility axis — a game
  registers through `ModuleRegistrar::Resources()`, which the existing `BindEngineRegistries` already
  reaches, whereas binding to a *subsystem* would hand the registrar an owner from a different tier.
  A type declares its formats in-class with `OPAAX_RESOURCE_FORMAT` (**I15**'s idiom); the facet is
  OPTIONAL because `BinaryResource` is format-agnostic and would have to invent an extension.
- **A duplicate extension is REFUSED with an Error naming both types, and it claims none of its others.**
  Two loaders for one extension make "what opens this file?" depend on registration order. Overriding an
  engine-claimed extension is a deliberate future `Override<T>()`, never a silent last-wins.
- **The registration seam had a latent hole this exposed, and it is the general lesson.**
  `OpaaxApplication::PopulateEngineRegistries` bound each route BY HAND while
  `ModuleRegistrar::BindEngineRegistries` — the one call MR0 built so the binding "does not grow an
  argument per registry" — sat there with **zero callers**. So the new route shipped unbound and dropped
  every game registration with one Error. **A convenience seam with no caller is not yet a seam**; the fix
  was to call it, which is also what makes the *next* registry cost nothing here.
**MR2** — Order is engine natives → game module → editor module → **seal** (before the first world). The
editor module slots in before the seal.
**MR2a — every D10 route is REAL as of M5, and `EditorRoute` is DELETED.** The M0 counts-only skeleton
existed to make boot *ordering* observable before any machinery did; `Panels()` (M2a), `Drawers()` (M2b),
`ResourceTypes()` (M2d), `EditWorldSystems()` (M4 S5) and finally `Menus()` (M5 S4) each graduated off it,
and the type left with its last user. The editor's whole menu bar is registry-driven, its own `File/*`
entries registered through the very route a game's `Tools/*` uses (D10), which also dissolves the merge
problem: there is only one `File` menu. **The M0 placeholder could not survive this** — a command needs
`EditorContext&` to do anything (D3) and `[] {}` does not convert — which is exactly why registry,
consumer and dogfood were one atomic step ([[L16]] predicted this in M2 and it held).
**Note on the M2a-style diff gate** ("a game extension costs zero `OpaaxEditorLib` changes"): it does not
apply to the slice where the route itself goes real, and cannot. It applies again from the next entry.

**MR2b — the menu bar is a TREE the caller builds, and a node is a COMMAND TAG** (landed 2026-08-19,
replacing the ORIGINAL `MenuRegistry`). `Menus()` returns a **`MenuRegistry`** (`Editor/Menus/`): root
categories in `Category()`-call order, each holding one ordered list of `TUniquePtr<IEditorMenuNode>` —
`EditorMenuCategory`, `EditorMenuCommandNode`, `EditorMenuSeparatorNode` — so categories, entries and
separators interleave and nesting goes as deep as it is written.
- **THE NAME CAME BACK ON 2026-09-01; THE DESIGN DID NOT** — read this before trusting git history.
  The type was called `MenuRegistry` until M5 S4, was renamed `EditorMenu` here, and is
  `MenuRegistry` again now. **The 2026-08-19 rename was the only one that changed anything**: flat
  `{Path, FMenuCommand}` closures → a tree of command TAGS, which is what [[L37]]/[[L38]] retired the
  closures for and what `ViewportToolbarRegistry.h` still cites. The 2026-09-01 rename changed
  **nothing but the name**, restoring it because `EditorMenu` was the one member of
  `EditorExtensionRegistrar` not called a registry — see the bullet below. A `MenuRegistry` in an old
  commit is the closure version; a `MenuRegistry` today is the tree.
- **Every route in the registrar is now a `*Registry` or a `*Route`, with the same three parts:
  register, consume, `Count()`.** That uniformity is the whole point of the second rename — the user's
  own framing: *"Editor Menu is the only one to not be a registry. All the rest is. We have to make
  one design."* **`ViewportToolbarRegistry` is the model and settles the shape question**: it stores,
  it draws *itself*, and it lives in the registrar — so "a thing in the registrar that draws" was
  never the defect it looked like, and `MenuRegistry::Draw` needs no apology. Registries live with
  what they register (`Editor/Menus/`, `Editor/Commands/`), not in one folder.
*This supersedes MR2a's "flat array of `{Path, FMenuCommand}` … nesting computed at draw time, because a
tree built at registration would be a second structure to keep consistent with the paths it came from."*
That argument was sound **only while a category had no identity of its own**. A path string can carry a
bar ORDER, an enabled predicate or a checked state for *nothing*, so the moment a category became a thing
that holds state, deriving it from a prefix stopped being free — and the "second structure" it warned
about never existed, because the tree **is** the storage now rather than a cache beside one.
**This is the one D10 route whose CALL SITE changed** (`Menus().Register("Tools/X", lambda)` →
`Menus().Category("Tools").SubCategory("Debug").AddCommand("X", tag)`). **MR1**'s freeze covers the
game-side `ModuleRegistrar`; it was never a promise about `Menus()`, and the break was deliberate.
- **A node is a TAG, never a closure.** `AddCommand(label, tag)` is the whole entry API; clicking is
  `Commands().Execute(tag, context)`, the identical call a key binding makes. That is what makes "the
  menu and the shortcut trigger one verb" true by construction rather than by discipline.
  - *This bullet used to add "and that one funnel is why UNDO lives in the dispatch" — **UN1** was
    reversed on 2026-09-02 and `Execute` now brackets nothing. The funnel claim above stands on its
    own; it never needed undo to be true.*
- **A FOURTH facet, `SetLabel(FMenuLabel)`** (2026-09-01): the drawn text computed per draw, for the
  entry whose name is state — "Undo Translate (Ctrl+Z)". Identity stays the node's id, so the
  get-or-create lookups and the invocation log are untouched; only a LEAF's display became dynamic,
  and nothing looks a leaf up.
- **Params are allowed, and the test is the CLASS OF PAYLOAD** (amended 2026-08-19, **MR2c**). This
  bullet used to reject a params-carrying entry outright, reasoning from `QuitCommand`'s `Window*`:
  *a payload only the composition root can supply is a payload a key binding cannot carry, so such a
  command would be menu-only by accident.* That argument is sound and it is **about the payload, not
  about params** — it was over-generalized into a blanket ban. `PanelIdParams{OpaaxStringID}` is the
  opposite kind of thing: plain interned data, expressible verbatim in a key→tag table, so an entry
  carrying it stays bindable. **A composition-root-only payload is still forbidden** and
  `EditorContext::MainWindow` remains the answer (`QuitParams` stays deleted).
  The payload rides as a **third optional facet** — `SetParams(T)` beside `SetEnabled`/`SetChecked` —
  not an `AddCommand` overload, so `AddCommand` keeps one signature and what an entry *is* still
  follows from which facets were set. It is held in an `EditorCommandParamsBox<T>`
  (`Editor/Commands/EditorCommandParams.h`, **not** `Menus/`): `EditorCommandRegistry::Execute` is a
  template whose `typeid` gate is what turns a wrong payload into a logged refusal rather than a
  `reinterpret_cast`, so a holder must keep the static type — and the key→tag table will need the same
  box.
- **Identity is an `OpaaxStringID` and it doubles as the label**, so `Category()`/`SubCategory()` are
  get-or-create on an integer compare and a menu cannot be looked up by one name and drawn under
  another. Two modules naming `"Tools"` mean the one `Tools`.
- **Children are `TUniquePtr` and that is load-bearing, not style.** `Category()`/`SubCategory()` hand
  back a reference the caller keeps and adds to, so by-value children would dangle it on the next
  `push_back` — the [[L25]] shape, a lifetime bug that compiles.
- **Facets are OPTIONAL PREDICATES** (`FMenuPredicate`, asked every frame), so what a node *is* follows
  from which were set — `WS2`'s `ShouldCreate` shape, not a kind enum that must agree with the fields
  beside it. `SetEnabled` is **MP7**'s "disabled, never refused" finally reaching the bar; the command's
  own `MapOps::CanEdit` guard stays, because the predicate is the readable half of the rule and not a
  replacement for it.
- **`Draw` is `const`** for the reason `EditorCommandRegistry::Execute` is: the tree is built before the
  registrar seals, and drawing must not be able to add to it.
- **Every PIE verb is a command now** (`Play`/`TogglePause`/`Step`/`Stop`), and its **three** front-ends
  — the toolbar's buttons, the reserved F5–F8, the `Play` menu — all dispatch the same tag instead of
  calling `PlayInEditor` three times. Nothing about a shortcut belongs on a menu node, so there is no
  `SetShortcut`: a **key→tag table is its own system**, and this is its precondition (a verb that is not
  a command cannot be bound).
**MR2c — A panel is a DESCRIPTION plus contents; the host owns the window** (landed 2026-08-19).
`Panels()` takes `Register<TPanel>(PanelDesc{...})` — id, which root menu category holds its toggle
(`Window` default, `Tools` for a tool-shaped panel), and `EPanelVisibility` at startup. `EditorPanels`
(`Editor/Panels/EditorPanels.{h,cpp}`) owns every instance **and** its visibility, and is the only place
in the editor that OPENS A PANEL WINDOW — *amended 2026-08-30: it used to say "the only place that calls
`ImGui::Begin`", which **MR2d** made false. It decides the window; `ImGuiEditorGui` emits it.*
- **The id is stated ONCE.** It was interned twice — in the `Register("Hierarchy", …)` call and again as
  a `m_PanelID` member — with nothing making the two agree ([[L37]]'s shape). `GetPanelID()` had 7
  overrides and **0 consumers**, so the panel-side copy bought nothing; it is deleted. The desc's `Id`
  is the registry key, the ImGui window label and the dock key in `imgui.ini` at once.
- **`IEditorPanel::Draw()` became `DrawContents()`** — widgets only. The `Begin`/`End` pair, the label,
  the first-use size and the close button are the host's, which deletes 7 copies of that boilerplate and
  the two mid-function `End(); return;` early-outs that were one added `return` from leaking a window.
  What genuinely varies is a `PanelWindowStyle` (first-use size + the Viewport's zero padding).
- **The ViewportPanel is an ordinary panel**, registered first (natives before modules, **MR2**), so its
  `SetPrimaryRenderTarget` handshake still cannot be reordered by a game module — and reverse-order
  teardown (**LC3**) now frees the FBO **last**, while the device and GL context are alive (**F2a**).
  What kept it out was `EditorService` reaching in for `IsHovered()`/`IsFocused()`; the panel now
  **pushes** those into `InputRoute`, which `EditorContext` already calls the one answer to "is the
  engine being fed". Sourcing, not mirroring — and it fixes a bug visibility would otherwise have
  introduced: a hidden panel never draws, so a cached hover would stay `true` forever and hold the route
  open with no viewport on screen ([[L28]]). `OnPreRender` runs visible-or-not and clears it.
- **The toggle is ONE command for every panel** (`Editor.Command.TogglePanel`), with the panel as the
  *payload* — see **MR2b**. Not a tag per panel: `OpaaxTag` refuses any byte `<= ' '` (**I14**) and
  `"Play Controls"` is a panel id.
- **`EditorPanels::SetVisible` is the single MUTATION POINT, which is stronger than a shared bool.**
  `Begin` is given a **local**, and the result is routed back through `SetVisible`; the transition is
  logged there and nowhere else. The first cut passed `&bVisible` straight to ImGui, reasoning that the
  close button and the menu tick then write the same variable and cannot disagree — true, and too small
  a claim: it is about the *value*, while a log line (or a later undo record, or a dirty flag) lives on
  the *path*. The X reached the state without passing through `TogglePanelCommand` and announced
  nothing, found by the user in one click ([[L39]]).
- **`BindPanelToggles` runs after the game module and before `Seal()`**, so a game panel's toggle costs
  zero `OpaaxEditorLib` changes — the M2a diff gate, still holding (S4 touched one file).
- **Not built, deliberately:** visibility does not persist across sessions (ImGui's `.ini` keeps dock
  position and collapse; open-state is the application's). Trigger: the first hidden panel that has to
  be reopened every launch. Also no `Closable=false` — closing the Viewport leaves the render target
  bound and the world drawing into an FBO nobody samples, which is wasteful and never wrong, and the
  menu is the way back.

**MR2d — ONE UI pass, ONE backend seam: `IEditorGui`** (landed 2026-08-30). The editor drew its UI as
three siblings — `EditorService::DrawGUI` called the dockspace, then `Menus().Draw()`, then
`Panels->Draw()` — and each reached ImGui itself. Now `IEditorGui::Draw(EditorContext&)` emits the
whole pass in one place (dockspace → menu bar → panels), and `DrawGUI` is two lines.
`Editor/UI/IEditorGui.h` is the seam, `Editor/Imgui/ImGuiEditorGui.{h,cpp}` the one implementation —
the `IEditorUIBackend`/`OpenGLEditorUIBackend` idiom, and `EditorService` picks the concrete type the
same way `Init` already picks the GL backend.
- **The line is CHROME vs CONTENTS, and it is the one every reference editor draws.** Unity splits
  `EditorWindow` (the host owns the window) from `OnGUI()` (the leaf owns its widgets); Godot's
  `EditorPlugin` adds a dock and paints nothing in it. So the seam carries exactly the structural
  widgets a HOST emits — `BeginMenu`, `MenuItem`, `MenuSeparator`,
  `BeginPanelWindow`/`EndPanelWindow` — and `Editor/Menus/` (4 files) plus `EditorPanels.cpp` name no
  backend at all.
  *Amended 2026-09-01: this list began with `BeginMainMenuBar`/`EndMainMenuBar`, which are now
  **deleted** rather than renamed — see **MR2e**. Opening the bar turned out to be the
  implementation's business, not the seam's.* A panel's contents, a `TPropertyDrawer` and a `ViewportToolbarRegistry` item still
  call ImGui directly, which was always the stated exception (**MR2c**) — but each of those now lives
  in a file whose job IS drawing. `EditorService.cpp` was the exception to that and is no longer: its
  five inline toolbar lambdas moved out (**GIZ8**), leaving the composition root with zero `ImGui::`.
- **The other ~360 call sites were deliberately NOT abstracted, and the reason is not scope.** A
  widget API designed against exactly one backend encodes that backend's shape — immediate mode, an
  ID stack, `IsItemHovered` late-bound to submission order (**L56**) — so it would buy the *word*
  portability and none of the property. What the seam does buy is real and small: the pass has one
  owner, and the ~20 chrome calls are behind one interface.
  - **ONE EXCEPTION, added 2026-09-01 with its reason: property DRAWERS** (**MR2h**). They are not
    bespoke panel UI — `TPropertyDrawer<T>` is the point a **game** extends to support a new field
    type (**I15**), so leaving it backend-bound binds every game's custom editor forever. The
    objection above is answered rather than waived: the vocabulary is *derived from the call sites
    that exist*, and the late-bound queries are **folded into the calls**, so nothing in it depends
    on submission order. Panel contents and viewport-tool closures are still covered by this bullet.
- **`EditorContext` carries `IEditorGui& Gui`**, which is why no `Draw` signature changed: a menu node
  already took the context, and a panel already gets one by ctor (**D3**). `UIBackend` STAYS beside it
  rather than collapsing into `Gui.Backend()` — it is the narrower dependency, and a panel that only
  turns a texture into an image has no business with the menu bar.
- **`BeginPanelWindow` swallows the push/pop pair.** The zero-padding style var is pushed before
  `Begin` and popped immediately after it (`Begin` has already consumed the window's padding), so
  `EndPanelWindow()` takes nothing and a caller cannot mis-pair it — the same reason **MR2c** moved
  `Begin`/`End` off the panels in the first place, one level further down.
- **The shortcuts moved AHEAD of the pass.** `HandleAuthoringShortcuts` ran between the menu bar and
  the panels; a chord there can execute a command that destroys the world mid-submission. ImGui's
  `RouteGlobal` defers its routing decision, so firing them before anything is submitted is
  behaviour-neutral and strictly safer.
- **`EditorService` owns it as a `TUniquePtr<IEditorGui>` built in the CONSTRUCTOR**, not in
  `InitGUI` — the context holds a reference to it, and a member that is null between construction and
  `Initialize` would put a null check in front of every `IsReady()`.
- **The gui does not look up what it draws** (2026-09-01). `Draw` reached into `EditorContext` for
  `Extensions.Menus()` and `Panels` every frame. It was given them by two setters the same day, and
  **superseded within hours by outright ownership — see MR2e**, once the registrar's own vocabulary
  was made uniform. `Draw` KEEPS its `EditorContext&` either way: the context is the per-frame
  *subject* (a menu node executes a command through it), not the content.

**MR2e — The editor draws its own TITLE BAR, and DECORATION is an axis of its own** (landed
2026-09-01). The OS caption is gone (`Window::SetDecorated(false)`); the editor paints menus on the
left, a drag region, and Minimize / Maximize / Close on the right. `ImGuiTitleBar`
(`Editor/Imgui/`) composes it and `ImGuiEditorGui` owns one — nothing outside the ImGui
implementation names the type, which is **MR2d**'s chrome-vs-contents line applied to the caption.
- **The MENU REGISTRY IS THE EXTENSION POINT, so there is no title-bar registry.** The bar draws
  the whole `Menus()` tree, so a game module's category appears in the caption through the route it
  already registers into (D10). The alternative — a `TitleBarRegistry` beside `ViewportTools()` —
  would have been a second way to say the same thing, for nothing.
- **`BeginMainMenuBar`/`EndMainMenuBar` LEFT the seam rather than being renamed, and the reason is
  structural.** The approved plan said rename; building it showed that if `MenuRegistry::Draw` both
  *opens and closes* the bar, the window buttons can never share the row. So `MenuRegistry::Draw`
  emits **categories** and the host opens the strip that holds them. The seam got smaller, and the
  menu tree genuinely stopped knowing where its bar is.
- **The gui OWNS what it draws** (2026-09-01, superseding **MR2d**'s "receives it" bullet).
  `IEditorGui` holds `MenuRegistry` and `EditorPanels` **by value, on the base** — so `SetMenu`,
  `SetPanels`, `BindGuiContent` and their null checks are all deleted. On the base rather than the
  implementation because a second backend wants the same tree and the same panels, not its own
  copies: what is backend-specific is the ~20 virtual chrome calls, not the content they draw.
  `Menus()` is therefore a **bound route** (the **MR4** `WorldSubsystemRoute` shape), bound in
  `EditorService`'s **constructor** — so "unbound" is not a reachable state rather than one every
  caller has to check, which is exactly the failure **MR2a** records.
- **`IEditorGui::Teardown()` is NON-VIRTUAL, and that is the change worth the most.** It runs the
  panels down, then the backend. `ViewportPanel::Shutdown` frees an FBO and clears the engine's
  primary render target, both of which need a live GL context (**F2a**) — an order that used to be
  two calls sitting correctly in `EditorService::OnShutdown`, i.e. a rule a future edit could break
  in silence. One owner makes it structural (**LC3**).
- **`EditorPanels` stays a separate live object and CANNOT be folded into `PanelRegistry`.** Two
  existing invariants forbid it, neither of them taste: registration runs at `OnModulesRegistered`
  with **no `EditorContext` in existence** (a panel needs one by ctor, **D3** — `PanelRegistry`'s own
  header says *"Registration STORES ONLY"*), and `EditorContext::Extensions` is **const by
  construction** while panel visibility mutates on every Window-menu tick. So panels are the one
  registration that becomes objects with lifetime, because a panel is an instance and the other
  seven routes are descriptions. That is the single exception, and it is stated rather than drifted.
- **Decoration is ORTHOGONAL to `EWindowMode`, which had welded them.** `Borderless` meant
  undecorated *and* primary-monitor fullscreen, and `SetWindowed()` hard-set `GLFW_DECORATED=TRUE` —
  so "Windowed AND undecorated", which is exactly what an editor with its own caption is, was not
  expressible. `WindowData` carries the host's `bDecorated` preference and `SetWindowed` applies it;
  `Borderless` still overrides while active *without overwriting it*, so leaving borderless restores
  what was asked for. `IsDecorated()` asks GLFW rather than the preference, so the two cannot drift.
  Same two-axes shape as **I12**, one layer down.
- **It is a RUNTIME verb, not a `WindowProps` field, and that is forced.**
  `WindowManager::CreateMainWindow` builds props from `EngineConfigData` alone — there is no host
  seam to override at creation — so `EditorService::Initialize` undecorates after the fact. The cost
  is a possible one-frame OS-caption flash at boot; the fix if it shows is an
  `OpaaxApplication::OnConfigureWindow(WindowProps&)` seam, named and not built.
- **`Window` grew what a client-drawn caption needs**: `GetPosition`/`SetPosition`, `SetSize`,
  `Minimize`/`Maximize`/`Restore`/`IsMaximized`. **`WindowData::PosX/PosY` became `Int32`** — they
  were `Uint32` while `SaveWindowedState` writes `glfwGetWindowPos` straight into them, so a monitor
  left of the primary, or a maximized window's `-8,-8`, wrapped silently. Latent until something
  dragged the window; the title bar is that something.
- **The buttons are COMMANDS** (**MR2b**), so the caption and a later key binding reach one verb.
  Close reuses `EDITOR_COMMAND_QUIT` — the X and File/Exit are the same verb, not two, the same
  argument `Window::RequestClose` already makes one level down.
- **`Draw` open-codes `DockSpaceOverViewport`, and TWO details are load-bearing:** the host window's
  label `WindowOverViewport_%08X` and `GetID("DockSpace")` *inside it* are what produce the
  dockspace id the user's `imgui.ini` already names (`0x08BD597D`). Change either and every saved
  dock position is orphaned — a silent loss of the user's own state ([[L20]]). Verified by diffing
  the ini's `[Docking]` section across a run.
- **The captions are PAINTED, not typed.** ImGui's default font covers Basic Latin + Latin-1, so
  `—` (U+2014), `□` (U+25A1) and `✕` (U+2715) would every one have rendered as a box. Strokes on the
  draw list need no font — the `ImguiDraw`/`ImguiWidgets` split already names this shape.
- **`Editor/UI/WindowFrameGeometry.h` is ImGui-free ON PURPOSE.** The 8-region hit test (corners beat
  edges) and the resize clamp are the fiddliest logic here and the part a smoke run physically
  cannot reach — it never drags a window edge. `ImguiLayout.h` is the tree's home for pure geometry
  and calls itself *"the half testable without a context"*, but it includes `imgui.h`, and
  `OpaaxTests` may reach editor headers only when they pull no ImGui. So the geometry got its own
  header and 8 test cases. **The clamp is the one worth naming**: a left/top drag moves the origin
  *and* sizes, so the minimum has to give back what it refused, or the origin keeps walking while
  the size stands still and the window slides out from under the cursor.
- **GLFW-only frame, Win32 later, decided with the user.** Aero Snap, the drop shadow and rounded
  corners are gone the moment decoration is; edge-resize is the one sub-item a `WM_NCCALCSIZE` frame
  gives for free. The editor side is written against the `Window` verbs precisely so that upgrade
  touches `WindowsWindow` only.

**MR2f — A MODAL is a seam too, and its result arrives by CONTINUATION** (landed 2026-09-01).
`IEditorDialogs` (`Editor/UI/`) fronts file pickers and message boxes;
`TinyFdEditorDialogs.cpp` is the **only file in the editor that includes `<tinyfiledialogs.h>`**.
Six `tinyfd_*` calls had sat inside command bodies — the exact shape **MR2d** removed for ImGui,
still standing for the other backend the editor names.
- **Its own seam, not a section of `IEditorGui`**, for the reason `EditorContext` already gives for
  keeping `UIBackend` beside `Gui`: it is the narrower dependency, and a command asking where to
  save a file has no business with the menu bar.
- **The continuation IS the design.** The native backend blocks and fires it inline, so a call site
  may capture `EditorContext&` and still read as a guard — but an ImGui modal is inherently
  multi-frame and a *returned value could never have expressed that*. Writing the call sites this
  way now is what makes an in-editor implementation a class swap instead of a rewrite of all six.
  **What a second implementation inherits:** by the time a deferred continuation runs, the world it
  captured may be gone; it must re-resolve or refuse.
- **That reached `LevelOps::ConfirmDiscardingEdits`, which returned `bool`.** It takes a
  continuation now. Only 2 callers, both guards, so keeping the seam *uniformly* async-capable was
  cheaper than leaving half of it blocking — and a confirm is the **easiest** dialog to draw in
  ImGui, so making exactly that one blocking would have been backwards.
- **Each post-pick body moved to a file-local function** (`CreateMapAt`, `AddMapAt`) rather than
  nesting fifty lines in a lambda: the interesting half stays readable and is callable with no
  dialog in front of it — which is what would make these commands testable at all.
- **`FileDialogRequest::Filters` is PLURAL from the start.** Every caller today passes one pattern;
  a texture field wants `*.png`, `*.jpg`, `*.tga`, and a singular field would have had to grow.
  `EDialogAnswer` is Yes/No — a three-button Save/Don't Save/Cancel costs one enumerator and
  tinyfd's `"yesnocancel"`, named and not built because nothing asks it.

**MR2g — ONE PIPELINE for every extension route: registry → live object → backend** (landed
2026-09-01, and it supersedes the ownership bullets in **MR2e** that preceded it). The driving
requirement is the user's, stated only after two wrong attempts at the symptom: ***"If i want to move
to QT its easy."*** Everything below follows from that rather than from the surface asymmetry.

> **A REGISTRY holds data and names no backend. A LIVE OBJECT walks a registry and draws through
> `IEditorGui`. The BACKEND implements `IEditorGui`.** Moving to another toolkit is one new
> `IEditorGui`, not an edit spread across the editor.

| Stage | Owner | Panels | Title bar |
|---|---|---|---|
| Registry — data | the registrar, **by value** | `PanelRegistry` | `TitleBarRegistry` |
| Live — walks it, draws via the seam | the gui, **by value** | `EditorPanels` | `EditorTitleBar` |

- **`MenuRegistry` → `TitleBarRegistry` (`Editor/TitleBar/`), back in the registrar by value.** The
  bound route added hours earlier is **deleted** — with the storage back where the other seven live,
  there is nothing to bind and **MR2a**'s hazard cannot recur. The *node* types stay in
  `Editor/Menus/`: they are menus, and the bar is what holds them. The registry lost its `Draw` —
  it exposes `Categories()` and the live object walks it, exactly as `PanelRegistry` exposes
  `Entries()`. Accessor `Menus()` → **`TitleBar()`**, five call sites, one of them the game module.
- **`EditorTitleBar` is not a wrapper, which was the objection to giving menus a live object at
  all.** It owns the bar's **FURNITURE** — the drag region and the three caption buttons — which
  *nobody registers*. That is precisely the counterpart of `EditorPanels` owning per-panel
  visibility on top of the registry's descriptions. Without it the stage really would have been
  empty, and the objection would have stood.
- **The POLICY is backend-agnostic; only the INPUT is the backend's.** `TitleBarDragRegion` returns
  `{Delta, bDoubleClicked}` and `TitleBarButton` takes an `EWindowButtonKind`, so *a drag moves the
  window, a double-click toggles maximize, and the middle glyph follows the state* are all decided in
  `EditorTitleBar`. A second toolkit inherits the behaviour and supplies pointer data.
- **`ImGuiTitleBar` is now purely the ImGui side of those two calls, plus the resize border.** The
  border deliberately is NOT part of the bar: it is frame chrome around the whole window, and a
  toolkit that keeps the OS frame has none. `WindowFrameGeometry.h` and its 8 tests are untouched.
- **NO REGISTRY IN THE EDITOR NAMES A BACKEND ANY MORE.** `DrawerRegistry.h` and
  `ViewportToolbarRegistry.h` were the only two headers under `Extensions/` that `#include
  <imgui.h>`, both to do **id scoping and layout** inline — which is *structure*, not presentation,
  so it moved to the seam: `PushIdScope`/`PopIdScope`, `CollapsingHeader`, `SameLine`,
  `ToolbarSeparator`. A drawer entry now takes an `IEditorGui&`. Gate:
  `grep -rn "#include <imgui" Editor/Source/Editor/Extensions/` is **empty**.
- **What is still NOT portable, stated so nobody is surprised later:** panel contents, every
  `TPropertyDrawer`, and the viewport tools' closures all call ImGui directly — ~360 call sites that
  **MR2d** ruled stay direct, and the reason holds. So the *frame* moves toolkits cheaply and the
  *leaves* do not. R5 buys the generic machinery, not the widgets.
- **`EditorPanels` cannot be folded into `PanelRegistry`**, so the two-stage shape is forced rather
  than chosen: registration runs at `OnModulesRegistered` with **no `EditorContext` in existence**
  (`PanelRegistry`'s own header: *"Registration STORES ONLY"*), and `EditorContext::Extensions` is
  **const by construction** while panel visibility mutates on every Window-menu tick.
- **Three attempts, and the first two were mine being wrong about the same thing** — see [[L71]].
  The registrar had held four different shapes at once; what looked like an ownership defect was a
  naming one, and what the user actually wanted was a *pipeline*.

**MR2h — A DRAWER names no backend: `IEditorWidgets`** (landed 2026-09-01). The value-editor
vocabulary — ~24 calls in `Editor/UI/IEditorWidgets.h`, implemented by
`Editor/Imgui/ImGuiEditorWidgets.{h,cpp}` — is what a property drawer speaks instead of ImGui. With
it, **the whole drawer layer, both registries and the game's editor module name no backend at all**
(`grep -rn "ImGui::" Editor/Source/Editor/Properties/ Editor/Source/Editor/Extensions/ Sandbox/Editor/`
is empty).
- **Its own seam, not a section of `IEditorGui`**, for the reason `EditorContext` already gives for
  `IEditorUIBackend` and `IEditorDialogs`: it is the **narrower dependency**. A drawer editing a
  float has no business with the menu bar, the dockspace or a panel window. Reached as
  `IEditorGui::Widgets()` — the `Backend()` shape, because it is the *same* backend one level down,
  where `IEditorDialogs` is the OS and is therefore owned separately.
- **Why this is not MR2d being violated**, and the distinction is the whole justification: MR2d
  ruled against abstracting *bespoke panel UI*, and that still holds for the ~360 call sites. A
  property drawer is the opposite kind of thing — a **closed** vocabulary of value editors, and the
  extension point a **game** uses (**I15**). Its specific objection — *"immediate mode, an ID stack,
  `IsItemHovered` late-bound to submission order"* — is answered: the vocabulary is **derived from
  the call sites that exist** rather than invented, and **the late-bound queries are FOLDED INTO the
  calls** (`Button(label, width, tooltip)`, never `Button` then a hover query), so nothing in the
  seam depends on submission order. The id scope is explicit rather than implied.
  - **That rule then DECIDED a later feature rather than merely surviving it** (property tooltips,
    2026-09-02). The obvious shape is "attach this text to the item just drawn" — precisely the
    banned decorate-the-previous-item verb. So `HelpMarker(text)` draws **its own item**, a `(?)`
    carrying the text, and the seam stays order-free. The constraint paid: a marker is *visible*,
    where an invisible hover target never tells a reader that an explanation exists. **When a
    stated invariant forces a different design, check whether the different design is better
    before treating it as a cost.**
- **The HAND-WRITTEN drawer is the dogfood, and it mattered.** `TagsComponentDrawer` — a game's
  bespoke drawer, not the generic fold — needed almost the same vocabulary as the built-ins
  (header, id scope, small button, text, separator, text field, disabled state, button). That it
  rewrites cleanly is what makes the seam worth its size; had it needed a dozen more calls, the
  closed-vocabulary argument would have been false.
- **One `DragFloat` with a component COUNT** replaces four ImGui entry points (scalar through
  `Vector4F`) — a backend implements the widget once. **The integer drags stay TYPED** (`DragInt16`,
  `DragInt32`, `DragUint32`) and that is not verbosity: a round trip through a wider type lets a drag
  past the real type's bounds **wrap** — past 32767 to a large negative draw order, below zero to ~4
  billion — so the clamp has to happen at the type the value actually is.
- **The five chrome calls MR2g had put on `IEditorGui` moved here.** They were widget vocabulary on a
  host-chrome seam; with a widget seam existing they have a home, and `IEditorGui` is pure host
  chrome again. `DrawerRegistry` and `ViewportToolbarRegistry` speak `IEditorWidgets`.
- **`AcceptResourceDragPayload` deliberately did NOT move.** `ResourceDragDrop.h` is already
  backend-free — only its `.cpp` names ImGui, the `TinyFdEditorDialogs` shape — so a drawer calling
  it names no backend already. Its `SetResourceDragPayload` twin stays with the browser, which is a
  panel and draws directly.
- **Still not portable, and still deliberate:** panel contents, the viewport tools' closures, and
  `Editor/ImguiLibrary/*`. The *frame* and the *fields* port; bespoke panel UI does not.

**MR2i — A NEW TYPE IS NOT DONE UNTIL EVERY ROUTE THAT COULD SHOW IT HAS BEEN TOLD** (landed
2026-09-04, from ⑥ S4 shipping half an authoring surface and the user finding both halves in one
minute: *"where is the component? the panel for opaaxfont? missing so many things."*).

The routes are separate on purpose — that is what keeps the engine editor-ignorant (**D4**) and lets
a game register its own — but "separate" means **nothing tells you one is missing**. A component
registered with the engine and not with the drawer registry is *addable, invisible and uneditable*:
the worst of the three states, and no build or test can see it. So the list is mechanical.

**A COMPONENT:**
| Route | Where | Without it |
|---|---|---|
| `Components().Register<T>()` | `Engine::RegisterNativeComponents` | not addable, not serialized |
| `Drawers().Register<T>()` | `EditorService::RegisterNativeDrawers` | **blank Inspector** — the ⑥ S4 bug |
| `EntityQuery::TryGetBounds` + `DrawRank` | if it RENDERS | unclickable, wrong outline, "nothing to render" icon (**TX7**) |
| a draw pass | `RendererManager` | invisible |

**A RESOURCE TYPE:**
| Route | Where | Without it |
|---|---|---|
| `Resources().Register<T>()` | `Engine::RegisterNativeResourceFormats` | the file has no type |
| chrome + **`SetActivate`** | `EditorService::RegisterNativeResourceTypes` | **double-click does nothing** — the other ⑥ S4 gap |
| `SetPreview<T>` **or** a document + panel + ops + undoables | per type | it can be seen and never opened |
| the deploy list | `Engine/CMakeLists.txt` | resolves in a dev build, missing in a shipped one |

- **The tell is a comment justifying the absence.** ⑥ S4 registered the family with chrome and no
  verb under *"A family has no preview and no document editor: it is an alias TABLE"* — a sentence
  whose whole job was to explain why a route was skipped, which is [[L19]]'s exact signature. Every
  other document type in the tree opens a panel; being an alias table is what the LIBRARY is too,
  and it has one.
- **"Which routes does this type still owe?" is a question to ask before reporting, not after.**

**MR3 — One module shape.** Runtime and editor modules share a marker base **`IModule`**
(`Application/IModule.h`): `IRuntimeModule : IModule` (`OnRegister(ModuleRegistrar&)`) and
`IEditorModule : IModule` (`OnRegister(EditorExtensionRegistrar&)`). `OnRegister` stays on each derived
interface — the two register into *different* registrars, so it can't sit on the base. All three are
header-only pure interfaces, **no `OPAAX_API`** (no exported symbols / shared state / identity tag — the
opposite end of the axis from **I2**). A host invokes a module as a throwaway instance
(`SandboxModule().OnRegister(reg)`), symmetric across runtime and editor.

**MR4 — The EDITOR registers world subsystems through the game-side route, into the SAME registry**
(M4 S5). `EditorExtensionRegistrar::EditWorldSystems()` is an `Opaax::WorldSubsystemRoute` — the very type
`ModuleRegistrar::WorldSubsystems()` uses, not an editor parallel — bound by `EditorService` at
`OnModulesRegistered`, which **MR2**'s order puts after the engine has started and before the seal. One
candidate list ends up holding a game module's Play systems and an editor module's Edit overlays, and
`World` cannot tell them apart: filtering is by `ShouldCreate`, never by who registered. The editor gets
no privileged path, which is the property that keeps an editor overlay writable by a game.

**MR4a — a derived type name must strip the elaborated-type keyword, not just the namespace.** MSVC's
`entt::type_name` yields `"class Opaax::Foo"`, and `DeriveTypeLeafName`'s `rfind("::")` removed `class `
**by accident**. The bug surfaced only when a **global-namespace** type first registered (M4 S5's editor
subsystem → `"class QuadBoundsSubsystem"`), because there was no `::` left to strip. Components make this
serious rather than cosmetic: a derived name is the key written into map files, so the first
global-namespace component would have written an unreadable one. Stripped explicitly now, with both
routes covered by a test.

---

## WS — World subsystems (landed M4 S3 2026-07-29; WS7 S4, WS8 S5)

**WS1 — REGISTRY holds candidates; the MANAGER holds instances.** `WorldSubsystemRegistry` (the second
member of `EngineRegistries` — **MR0**) is engine-owned *type metadata*: a flat list of candidate types in
registration order. `WorldSubsystemMgr` — the plain `ISubsystemManager<IWorldSubsystem>` subclass a `World`
already owned — holds the live instances **per world**. One registry, N worlds. This is why
`ISubsystemManager` needed no change: it already separates factories from instances, which is exactly what
a world needs.

**WS2 — `ShouldCreate` is STATIC and OPTIONAL, and that is what makes filtering meaningful.** A candidate
may declare `static bool ShouldCreate(const World&)`; `TWorldSubsystemEntry` detects it with
`if constexpr (requires ...)` and defaults to *always create*. Static because deciding needs no instance —
so a rejected candidate is **never constructed**, which is what makes "an Edit-only overlay does not
*exist* in a Play world" true rather than merely inactive. Optional because most subsystems want "always",
and forcing every one of them to write `return true` is boilerplate. It runs at **every** world creation,
including every PIE start, so it must stay pure and cheap.

**WS3 — `WorldContext` exists because the registration site has nowhere to capture a dependency.**
Editor.md §3 says a subsystem "receives its world and nothing else" and that the *registration site*
captures any app service into the factory. **That is not implementable**: the site is
`WorldSubsystems().Register<T>()`, which takes no arguments and is frozen by **MR1**. Without a context a
subsystem needing `ResourceManager` would reach the locator, which **D3** forbids. So the dependency
arrives by **constructor**, `EditorContext`'s shape one layer down: `{ World& OwningWorld;
ResourceManager& Resources; EngineEventBus& Events; DebugDraw& Debug; }`. Per-world (that is the point —
`OwningWorld` differs, and PIE means two are live), **owned by the `World`** so a subsystem may store
`WorldContext&` for the world's whole life. `EngineRegistries` is deliberately **not** a member: a
registry is type metadata, not a running subsystem's business, and nothing needs it — add a member when
something does.
- **`IPaths` arrived by exactly that clause (⑥ S3, 2026-09-03)** — the first world subsystem to load
  an ASSET, since `ResourceManager::Load` takes an absolute path while a `TResourcePath` is
  deliberately relative (**MP8**). One member, and `WorldManager` had already resolved `m_Paths`.
  **The clause works as written; what it did not say is that the null-guard beside the construction
  must grow too** — three siblings were checked and the fourth dereference would not have been.

**WS4 — `std::ref` at the injection point is LOAD-BEARING.** `ISubsystemManager::RegisterSubsystem`
captures ctor args **by value** into the factory lambda, and `StartupAll` **clears `m_Factories`** once
consumed. Passing `WorldContext&` straight through therefore copies the context into a lambda that is then
destroyed, leaving every subsystem's stored reference dangling — a silent use-after-free. `CreateInto`
passes `std::ref(InContext)`, so what gets copied is a *pointer* to the World-owned context. Caught only
because the by-value form also fails to compile (an rvalue will not bind to `WorldContext&`); do not
"simplify" it back. The regression gate is the assertion that a started subsystem's context address
**equals `World::GetContext()`** — comparing `OwningWorld` instead would not discriminate, since freed
memory usually still holds the old value (**L15**).

**WS5 — There is NO `Render` hook for world subsystems.** `WorldManager` overrides `Update`/`FixedUpdate`
and forwards to the **active** world only (a PIE clone and the edit world coexist; exactly one simulates).
`RenderAll` is left unwired even though the base offers it: a subsystem draws by submitting to `DebugDraw`
from its `Update` — immediate mode, drained every frame (**F4**). A second draw path into a frame
`RendererManager` already owns is the thing being avoided.

**WS6 — A world's subsystems shut down through `DestroyWorld`, not through `~World`.**
`World::ShutdownSubsystems()` is **idempotent** (**LC3**) because it is reached two ways:
`WorldManager::DestroyWorld` calls it while every engine sibling a context points at is **still alive**
(the LC-correct moment), and `~World` repeats it as the safety net for `WorldManager::Shutdown`, whose
`m_Worlds.clear()` never goes through `DestroyWorld`. Same **LC1** reasoning as the engine-level phases,
one scope down.

**WS7 — A world subsystem NEVER assumes entities exist at `Startup`; it reads world content from its
first `Update`** (landed M4 S4). True on both creation paths, which is the point of stating it: the
*startup* world's entities are spawned by the host in `PostEngineStartup`, i.e. after `FinishStartup`
(**BO4**), and a *cloned* world is `Capture` → `CreateWorld` → `Instantiate`, so `CreateWorld` starts its
subsystems before the entities land. Instantiating before starting would "fix" the clone at the cost of
making it the only populated-at-`Startup` world in the engine — and "can I read the world in `Startup`?"
would answer *"depends how your world was made"*, which is a far worse contract than a uniform **no**.
`QuadOscillatorSubsystem` is the worked example (baselines on first tick) — *still readable as code,
but unregistered since 2026-09-07, so `PhysicsSubsystem`'s reconcile is now the LIVE one.*

**The named candidate for a post-instantiate hook was PHYSICS, and it did not need one** (⑦-A P1,
2026-09-07). This entry used to say that "physics rebuilding bodies from authoring components" would
be what finally justified an explicit `OnWorldBeginPlay`. Building it showed the opposite: a
**per-step reconcile** (**PH7**) is strictly better than a hook, because one pass covers the
host's `PostEngineStartup` spawn, a clone's `Instantiate`, gameplay spawning mid-play AND a component
added to a live entity — where a hook covers only the first two and leaves the rest silently
unhandled. So the hook remains **unbuilt, now with its best candidate spent**: the next proposal for
one must first say why a reconcile is not the answer.

**WS8 — The PIE tick gate lives in `WorldManager`, and the decision is taken ONCE PER FRAME** (landed
M4 S5). `SetPaused` / `RequestStep` are flags; `Update` — the once-per-frame hook — resolves them into
`m_bTickThisFrame`, and `FixedUpdate` only *reads* that. The ordering is the design, not an
implementation detail: `Engine::Loop` runs `Update` once and `FixedUpdate` 0..N times, so a step that
advanced only `Update` would starve the fixed step and desync a physics world from what the viewport
shows. **A step is a FRAME.** Two consequences worth keeping: a pause arriving between the two hooks
takes effect *next* frame (the current one stays coherent), and the gate is **mode-blind** — an Edit
world pauses exactly like a Play one, because the rule is about the tick, not about what a world is for.
The editor sets a flag and never reaches into the loop.

---

## IN — Input (landed M-Input, 2026-07-31)

**IN1 — One chain, and the ROUTE is the only gate.** `Window → Application → route → InputManager`
(Editor.md D5). The application feeds the engine in `HandleAllInputEvent`; a host that wants to withhold
input **consumes the event before that** (`EditorApplication::OnEvent` returns early when `RouteInput`
says true). So "the editor ate it" and "the engine never saw it" are the same statement, and there is no
second gate downstream to keep in sync. The runtime has no route at all: no editor, no gating, zero cost.

**IN2 — The frame boundary is the HOST LOOP's: `EndFrame()` runs in `RunApplication`, immediately before
`PollEvents` — not in a tick hook, and not inside `Engine::Loop`.** A subsystem `Update()` is too late by
construction: the application polls OS events *before* it calls `Loop`, so the hook would run after the
very events it is meant to precede. But `Loop` is wrong too, and that one shipped before it was caught:
**`Loop` is not the end of the host's frame.** The editor draws its entire UI *after* `Loop` returns, so
clearing there wiped the frame's edges and deltas before any panel could read them — held keys still
worked, which is exactly why it looked fine. Every reader must see the same frame: a game system in
`Update`, an editor panel in the UI pass. The host loop's boundary is the only point that is true for
both.

**IN3 — Edges are LATCHED by the feed, never derived from a previous-frame snapshot.** A key pressed and
released inside one frame leaves both snapshots reading "up", so a comparison **drops the input** — and a
frame is easily long enough for a real keypress once the framerate dips. `OnKeyPressed`/`OnKeyReleased`
set latches that `EndFrame` clears. This is smaller as well as more correct: there is no previous-state
array. Found by a test, not by review.

**IN4 — The feed is IMMEDIATE, not queued through the event bus.** It runs inside `PollEvents`, before
the frame ticks, so a reader mid-route — the editor asking whether Shift is held while handling a key —
gets this frame's truth instead of last frame's copy.

**IN5 — Closing the route RESETS the engine's input, and a reset is not an event.** The OS delivers
releases to whoever has focus, and once the route closes that is someone else; anything held would stay
held forever. `ResetState` **clears** the edge latches rather than filling them, so nothing reports a
release for a press the reader may never have seen. Closings: `WindowLostFocus` (the runtime's only one)
and, in the editor, every open→closed transition of `InputRoute` — which covers PIE pause and stop
without either of them calling input code.

**IN6 — Modifiers are keys.** `IsShiftDown()` reads `LeftShift || RightShift`. There is no modifier
state and no modifier field on the event payload — which is why nothing needs the GLFW `mods` parameter
the window callback discards, and why the editor's reserved shortcuts are bare function keys today.

**IN8 — An EDITOR shortcut cannot read `InputManager`, because the route it lives behind is closed**
(landed M5, 2026-08-03). The engine's input is fed only when `InputRoute` is open; with an Edit world on
screen the route is `ClosedEditMode`, so `RouteInput` consumes every Input-category event and
`InputManager` never sees the keys at all — `IsCtrlDown()` answers false *forever*, precisely where a
`Ctrl+S` wants it. So the editor has **two shortcut mechanisms, and the split is principled**:
- **PIE control (F5–F8) lives in the event route** (`HandleReservedKeys`, D5 step 3). It must fire while
  the *game* owns the keyboard, so it has to sit ahead of the feed.
- **Authoring chords (Ctrl+S, Ctrl+O) live in the UI pass**, via `ImGui::Shortcut`. They only mean
  anything while the *editor* owns the keyboard — which is exactly when ImGui's view of it is the
  authoritative one. ImGui is fed regardless of our route (`ImGui_ImplGlfw_InitForOpenGL(window, true)`
  chains the GLFW callbacks), so this is not a workaround, it is the correct source.
This is **IN6**'s consequence, not a contradiction of it: modifiers are keys, and a mechanism that never
receives keys cannot report modifiers. *(Caught while planning, by asking what `IsCtrlDown` would actually
answer, rather than after building on it — the [[L29]] shape.)*

**IN7 — Gamepad codes are REFUSED, not half-supported.** `EKeyCode` reserves the range, but GLFW exposes
pads by *polling* — a second feed that does not exist. Accepting the code would make `IsKeyDown` answer
"false" forever while looking supported. Same for `KeyTyped`: a Unicode codepoint is text entry and has
no "down" to hold.

---

## WM — World model (World > Level > Map)

Settled with the user 2026-07-28, superseding the retired `Scene` vocabulary (**X4**); the World/Level
composition **amended 2026-08-06** (WM1/WM1a — `RootLevel` dropped). Source of concepts:
`Docs/Architectures/EngineArchi.md` — stale in places, see WM5.

**WM1 — Three nouns, one registry.** `World` is the runtime simulation container and **the ECS boundary**:
it owns the single `entt::registry` and the `WorldGuidRegistry`. A `Level` composes Maps and handles
streaming. A `Map` is **pure entity data** — no systems, no runtime ownership — and is therefore **the
serialization unit**. `World { Level { PersistentMap (always mounted) + N maps (streamed) } }` — **ONE
Level per World**, and the always-mounted/streamed split lives on the **Map**, not on a second Level.

**WM1a — The PersistentMap exists for AUTHORING COST, not for state survival** (settled 2026-08-06,
replacing the `RootLevel` / `ActiveLevel` pair). The question that decided it was not *"what entities must
outlive a level change"* — nothing has to — but *"what do I refuse to drag into every map again"*: the
player, the lights, the managers. They are authored ONCE, in one map, and every other map in the level
composes on top of it. Open a decor map, hit Play, and the player is there because the Level mounted its
persistent map first — not because that map did anything special. **That is why `RootLevel` is gone:** a
second `Level` object above the first was solving state survival, and one field in the manifest solves
authoring cost.
- **Declared in the LEVEL manifest, not the project**, so a shmup's persistent map (ship, HUD) and a
  menu's can differ. `LevelData` carries a `persistentMap` key naming one of its own `Maps`; **absent
  defaults to the first entry** (**MP3** — a missing field defaults). No existing `.opaaxlevel` became
  invalid and `LEVEL_FORMAT_VERSION` did **not** move (it bumps only for what a v1 reader would MISREAD).
- It is an ORDINARY map in every other respect — same `.opaaxmap`, same `EntityMeta::OwnerMap` partition
  (**WM2**), same Save path, same dirty check. *"Persistent" is a mount policy the Level applies, never a
  property of the file* — so nothing in `MapFile`/`MapJson`/`MapSerializer` learns a new concept.
- **Consequence for the editor, and it is the load-bearing one:** what a session has open is a **Level**,
  not a Map. That is what makes *"try this decor map"* well defined — it is one of the open level's maps.
  The editor's one-map-at-a-time state (**MP5**) is a placeholder for a per-map record under one open
  level, not the shape it keeps.
- **The manifest names a PATH; memory holds an INDEX** (`LevelData::PersistentMapIndex`), so "it is one of
  this level's maps" is resolved once in `LevelFile::Load` and never re-checked downstream. A name matching
  no entry warns and defaults to the first (**MP3**) — a level that stopped opening over a mistyped
  optional field would cost more than the field is worth.
- **The runtime `Level` object landed 2026-08-08** (**WM8**), so *"the Level mounted its persistent map
  first"* is now literally what happens rather than a description of a rule the loader followed.
  `RootLevel` staying dead is the part of this entry that never moved.

**WM2 — A Map is a PARTITION of the World's registry, not a container.** One World owns one registry, so
"the entities of map X" is a filter, not a separate store. `EntityMeta::OwnerMap` (`MapId` =
`OpaaxStringID`, interned) is what makes the partition addressable. **Default-invalid means
runtime-spawned** — a bullet no map authored — so filtered capture excludes it *by the rule* rather than by
a special case. Unfiltered capture takes the whole world (the PIE-clone case).

**WM3 — Guid is the only persistent reference.** entt handles are per-registry and never assumed stable
across worlds, so capture→instantiate must preserve GUIDs or every inter-entity reference silently retargets.
`World::CreateEntity` mints a fresh Guid and therefore **cannot** serve instantiate;
`CreateEntityWithGuid` is the restoring entry point, and it refuses an already-live Guid because
`WorldGuidRegistry::Register` *replaces* — a duplicate would evict the original and `FindByGuid` would
start answering the impostor.

**WM4 — A Level's maps are NOT `LoadContext::Acquire`d** (landed M5, 2026-08-03). Both are resources
(`MapResource` / `LevelResource`, both **FailFast**; `.opaaxlevel` = a manifest of map refs, `.opaaxmap` =
entity data; lowercase, matching the shipped `.opaaxproj`). `Acquire` is for *hard* dependencies: it loads
them inline and chains their refcounts to the parent, so acquiring a level's maps would load every one of
them at once and make unloading a single map impossible — the exact opposite of streaming. The manifest
stays **data**. A Map's *textures* will be `Acquire`d; a Level's *maps* never are, and **the asymmetry is
the rule** — `LevelResource::Load` carries that note precisely so the next reader does not "fix" it.

**WM4's trigger FIRED on 2026-08-08, and there is still no `LevelManager`.** The trigger it named was *"the
first thing that genuinely needs to load or unload a map while the world is running"* — the editor opening
levels and standalone maps. What that produced is **WM8**: a `Level` object owned by the World. It did not
produce a manager, and now deliberately rather than by deferral — **a World owns its Level, and nothing
manages levels across worlds.** Switching level means a new World (**WM8**), which `WorldManager` already
owns; a `LevelManager` would be a second owner of the same lifetime.

M5's stateless `LevelLoader` is **deleted**, absorbed into `Level::Mount`/`MountAll` — a stateless loader
beside a stateful Level doing the same job is two homes for one rule. What it established survives
unchanged inside them: each map resolved through `IPaths::AssetToAbsolute`, loaded as a `MapResource`
**through the `ResourceManager`** (a real caller on every boot rather than only in its own test, [[L23]],
plus dedup when two worlds open one map), instantiated, ref dropped. A failed map is counted and skipped.

**WM5 — `EngineArchi.md` is behind this contract in three places** (code wins, X4): it puts `MapId ownerMap`
in `EntityMeta` as though it were already there (M3 S2 actually added it); it makes `GuidRegistry` global
(`Guid → World*, entt::entity`) where the code made it **per-World** — entt handles are only valid inside
their own registry, so a Guid resolves through its world and never crosses worlds; and its **§8 Level
Hierarchy is superseded by WM1/WM1a** — it nests `World → PersistentLevel + ActiveLevel` and *then* puts a
`PersistentMap` inside `ActiveLevel`, which is self-contradictory (if a PersistentLevel holds the
always-loaded content, the persistent map has nothing left to be). One Level, persistence on the Map.
Its **§7 `LevelManager`** is not stale — it is simply **unbuilt** (WM4 holds the trigger).

**WM6 — A world CLONE is a snapshot round trip, so it copies exactly what the ComponentRegistry knows**
(landed M4 S4). `WorldManager::CloneWorld(source, mode)` = `Capture` (**unfiltered** — a clone that
dropped runtime-spawned entities would start out already diverged, **WM2**) → `CreateWorld(name, mode)` →
`Instantiate`. Three properties come from that shape rather than from clone-specific code:
the clone keeps the source's **Guids** (**WM3**) but gets its **own** world Guid; its **mode is the
caller's argument, never inherited** — going through the ordinary `CreateWorld` is what makes its
subsystem set follow that mode (**WS2**); and a component type the registry does not know is **not
carried** (it has no stable name to be written under — a registry gap, not a clone bug). The source is
read-only throughout and stays alive, which is the whole restore mechanism: Stop re-activates it and
destroys the clone, undoing nothing. No `MapId` filter here — the filtered form is "save this map" (M5).

**WM7 — A world's NAME is display text, so it stays an `OpaaxString`; interning is for KEYS**
(settled 2026-08-05, closing a `//Todo: OpaaxStringID` on `CreateWorld`). `OpaaxStringID` buys one thing:
O(1) integer compare on a key. Nothing compares or looks up a world by name — every `World::GetName()` in
the tree is a log arg, the toolbar's display string, or the copy `CloneWorld` hands the clone. It IS
persisted now (`LevelFile::Save` writes the level's `name`, 2026-08-08) — which changes nothing here: it
is written as the text it already is, and a value written to a file is the last thing that wants an
intern-table index behind it (**MP1**). Converting would make the *only* live path
slower (`ToString()` = `shared_lock` + an `OpaaxString` copy out of the pool, where `.CStr()` is a
pointer) and would intern unbounded author content — the pool never evicts, and a world's name is the
level's `name` key or a file stem (**BO4b**). The tree already draws this line correctly: the two
registries intern *because* they do `GetName() == InName`, `MapId` interns *because* `EntityMeta::OwnerMap`
is compared per entity (**WM2**), and `EntityMeta::Name` — display, like this one — does not.
**Trigger to revisit:** the first thing that resolves a world BY name (a `FindWorld(name)`, or persisted
editor session state naming one).

**WM8 — `Level` is the ONLY thing that puts a map in a world or takes it out** (landed 2026-08-08, WM4's
trigger). One per World, owned by it (`World::SetLevel`), so *"which level is loaded"* is a question the
world answers and nothing above has to track. Null for a bare world in a test, exactly as `WorldContext`
is — a Level needs the `ComponentRegistry` and `IPaths` that only a real engine has.
- **Its four references are the parameter list `LevelLoader` used to thread through every call**, held
  once. That is what lets `Mount(path)` take nothing else, and it is the whole point: the level knows
  which maps.
- **UNMOUNTING IS A FILTER, NOT A TEARDOWN** — WM2 paying off. "Destroy the entities whose `OwnerMap` is
  this": no store to drop, no refcount to release, because the `MapResource` ref was already dropped once
  the entities existed. Collect-then-destroy, since destroying while iterating an entt view is not safe.
- **Mounting is checked by PATH *and* by `MapId`.** Path catches the same file twice; id catches two
  files claiming one map. *Amended 2026-08-09: this used to add "the id can legitimately be invalid — a
  map with no entities has nobody to claim it — and the file-stem fallback is the EDITOR's rule, not the
  engine's." **MP10** ended both halves: a map names itself, `MapFile::Load` owns the stem fallback, and
  a mounted map's id is always valid.* Both checks stay — two paths can still name one map.
- **A CLONE COPIES MOUNT STATE AND MOUNTS NOTHING** (`AdoptMountedFrom`, called by `CloneWorld`). The
  clone's entities arrive in the unfiltered snapshot (**WM6**), so mounting again would re-read every map
  and `CreateEntityWithGuid` would refuse the lot as live duplicates (**WM3**). This is the line a later
  reader is most likely to "fix".
- **Switching level means a NEW WORLD**, never editing the current one into shape: `IEngine::OpenLevel`
  creates, mounts, activates, and only *then* destroys what it replaced — the same order as `PIE::Stop`,
  so no frame runs without an active world. **`FinishStartup` is that call the first time**, not a second
  boot path to keep in step. An empty `LevelPath` gives a NullLevel world with an empty Level, which is
  what editing a map belonging to no level needs.
- **The manifest is authored THROUGH the Level** (`AddMap`/`RemoveMap`/`SetPersistentMap`), because the
  Level is what mounts — a second copy of the manifest anywhere else is drift waiting to happen.
  `RemoveMap` refuses the persistent map rather than silently re-pointing persistence at whatever ended
  up first, and carries `PersistentMapIndex` back when something ahead of it is removed (it is a
  POSITION, so a removal before it would silently rename what it means).

---

## MP — Map/Level file layer (landed M5, 2026-08-03)

Four layers, each knowing only its neighbours — which is why the M3 snapshot core needed **no change at
all** to gain persistence:
`World` ⇄ `MapSerializer`/`MapFactory` ⇄ `MapData` ⇄ `MapJson` ⇄ `MapFile` ⇄ a file.

**MP1 — Everything interned is written as its STRING, and that is why this layer exists.** An
`OpaaxStringID` is an intern-table *index*, so a map that wrote one as a number would mean something
different on the next run (**WM2**). `MapJson` is the only place that conversion happens.
- The **invalid** id is written as `""`, not as the `"None"` its `ToString()` answers. The round trip
  would in fact survive `"None"` — `OpaaxStringIDPool` reserves index 0 for it, so re-interning that text
  yields the invalid id back — but the file would claim a runtime-spawned bullet belongs to a map called
  None. A map file is read by humans; `""` is the encoding that does not lie. *(The original plan
  overstated this as data loss; the test that asserted the round trip broke is what corrected it.)*
- A `Guid` is 32 lowercase hex chars, High then Low, no dashes (`Guid::ToString`/`FromString`, out-of-line
  in the DLL). Big-endian nibbles, so lexicographic order over the text matches numeric order over the
  value. `FromString` parses into **locals** and only then writes its output: a half-written Guid is a
  *different identity*, not a rejected one, and the caller could not tell.

**MP2 — Entities are SORTED BY GUID on write, and two consumers depend on it.** entt's view order is
storage order, so an unsorted write would reshuffle the whole file every time an entity was destroyed
(a map file lives in git), **and** the editor's derived dirty check — which compares serialized text —
would report a world nobody edited. Components are a json **object** keyed by authoring name, so a
duplicate type on one entity is unrepresentable rather than merely unlikely. Nothing reads a map in order.

**MP3 — Reading a map file is TOLERANT AND TOTAL; refusing leaves the caller's data untouched.** nlohmann
throws on a type mismatch, so every read is guarded — a hand-edited file is an ordinary input. A missing
field defaults, an unknown field is ignored, and an entity with **no usable guid is SKIPPED** rather than
given a fresh one (minting an identity silently retargets every reference that pointed at it, **WM3**). A
`version` NEWER than this build is refused rather than half-read. Every failure path down the stack leaves
`OutData` alone, so a map that fails to read cannot half-replace one already held and then be written back
over the original.

**MP4 — Saving is NOT a `ResourceManager` capability.** That API is frozen at
Load/Resolve/Pin/FlushAll/Update, and its own header says every future capability is a separate system
*consuming* it. `MapFile::Save` is that system, and it sits beside `MapFile::Load` in one unit because the
two are halves of one format contract — splitting them across files is how a writer and a reader drift.
**`LevelFile::Save` followed the same rule** (2026-08-08), and deliberately without a `LevelJson` beside
it: `MapJson` exists because a `MapData` carries interned ids that must be written as strings (**MP1**),
and a `LevelData` is a name and some paths. `Serialize` is public separately from `Save` because the
editor's dirty check compares against the text without writing anything.

**MP5 — The editor's dirty flag is DERIVED, never tracked.** Capture the world filtered by a map's
`MapId`, serialize, compare against the text last written or read — **one baseline per MOUNTED MAP, all
of them in `EditorLevelDocument`** (2026-08-08). `EditorMapDocument` owns none and is a CURSOR: a second
baseline for the same map would rebase on Save Map while the other did not, and the marker would start
lying ([[L30]]). Records are **reconciled, never rebuilt** (`TrackMounted`) — re-taking every baseline
after mounting one map would silently adopt every other map's unsaved edits as the clean state.
- **There is nothing to hook.** Every edit goes through a registered drawer whose contract is
  `bool(Entity&)` meaning *was it drawn*, not *was it changed*. Tracking would mean changing that contract
  and every drawer with it, a game's included.
- It gets the interesting case right: drag a quad away and back and a **flag** says dirty while the file is
  already correct; the comparison says clean, which is true.
- It cannot drift — a derived answer has no second copy of the state to fall out of sync with.
- **A world holding several maps does not disturb it**: each capture is filtered by ITS map's `MapId`
  (**WM2**), so the maps mounted beside it are simply not in that comparison (**MP7**). The level's own
  marker is the OR of all of them plus the manifest (**MP9**), which is why it can be lit while the
  focused map's is not.
- **Changing which map is focused risks nothing and confirms nothing.** Every map keeps its baseline, so
  the map being left stays exactly as dirty as it was. Only a world-DESTROYING action (Open Level, or
  opening a map that belongs to no open level) prompts, and it prompts on the whole level.
- **It must be throttled, not per-frame** (4×/s, cached in `EditorService` where the frame clock is, so
  `IsDirty` stays pure). Built per-frame first, which also put ~1700 identical lines in a 10-second log —
  hence `MapSerializer::Capture` logging at **Trace**: it is a pure transformation with several callers,
  and an Info belongs to things that happen *to* something (`MapFile` Save/Load, `MapFactory` Instantiate).
- **The throttle was never enough, and MEASURING said by how much** (2026-08-13,
  `Engine/Tests/Perf/MapCapturePerf.cpp`). One capture+serialize pass costs **~12 µs per entity in
  Release, ~110 µs in Debug** — the config the editor is actually run in. That is **11 ms/pass at 100
  entities in Debug**, two thirds of a 60 Hz frame, four times a second, *for a world nobody touched*.
  A throttle bounds the RATE; it does nothing about a cost that scales with the level, so the ceiling
  was ~150 entities (Debug) before one pass ate a whole frame.
- **So the poll is now GATED on `World::GetRevision()`** — a monotonic `Uint64` bumped in
  `AddEntityCount`/`RemoveEntityCount`/`Clear`, plus a public `MarkChanged()`. `RefreshDirty` skips the
  per-map captures outright when it has not moved. Idle went from ~4 captures/s to **zero** (verified:
  2 lines in an 18-second editor run, both at boot).
  - **The revision is NOT the dirty flag, and this bullet is why MP5's title still says "never
    tracked".** It gates *when* the answer is derived, never *what* it is. It is a conservative
    over-approximation: a spurious bump costs one wasted capture, and a missed bump is the only real
    failure — so it is bumped from chokepoints that cannot be bypassed, and pinned by six cases in
    `WorldEntityTests.cpp` including **monotonicity** (a revision that could go backwards could land on
    the stored value and make a changed world read unchanged).
  - **The one mutation no chokepoint sees is a component edited IN PLACE**: entt stores by value and a
    drawer receives a raw `TComponent&`, exactly as the "nothing to hook" bullet above says. So
    `InspectorPanel::Draw` bumps on `ImGui::IsAnyItemActive()` — asked of **ImGui, not of the drawer**,
    because a `bool Draw()` contract would let one forgetful drawer report *clean while dirty*, which
    fails silently and permanently. It also bumps on the frame *after* activity: a Checkbox commits on
    release, by which time ImGui has cleared `ActiveId`.
  - **`SaveMap` now rebases `bDirty` alongside `Baseline`** — two halves of one fact. The old code left
    the flag for the next refresh (a ≤250 ms lie); under the gate a save does not change the WORLD, so
    that lie would have lasted until the next unrelated edit.
  - Still **O(maps × entities)**: each `CaptureMap` walks the whole world and filters, so N mounted maps
    is N full walks. Not fixed — there is one mounted map today and the gate removed the cost that
    actually bites. The measurement above is the trigger to revisit.
- **THEN THE PASS ITSELF WAS PROFILED AND CUT** (2026-08-13, after the gate). Splitting one pass into its
  stages said the cost was not where the suspects list assumed. At 1k entities, Release, per entity:
  **capture 1.26 µs · ToJson 2.13 µs · dump(4) 1.46 µs · OpaaxString wrap 0.06 µs.** Inside capture, the
  entt walk is *1.3 ns* and the `Has()` probes ~10 ns — **all of it is building json**. Four changes, no
  format change and no touch to the `CComponent` contract (**I8**):
  - **`json{ {k,v}, … }` → `obj[k] = v`.** The initializer-list form cannot know it is an object until the
    list closes, so it builds an array of two-element arrays and rebuilds that as an object: **1269 → 632
    ns per entity**, the single biggest win. Bytes are identical because `object_t` is a `std::map` — the
    file records sorted keys, not insertion order.
  - **`Serialize`/`SerializeCompact` gained a `MapData&&` overload** that MOVES component payloads into
    the json instead of deep-copying each tree. Every hot caller passes a temporary.
  - **`IdToText` returns `const char*`** (`OpaaxStringID::CStr()`, the interned bytes) instead of an
    `OpaaxString` copy — a guid is 32 chars and `OpaaxString`'s SSO is 15, so that was a heap round trip
    per id per entity for text nobody kept.
  - **`CaptureEntities` reserves `Entities`** from the view's `size()`, and `DumpToString` carries the
    dump's LENGTH into `OpaaxString` instead of re-`strlen`ing half a megabyte.
- **THE COMPARISON FORM IS NOT THE FILE FORM** (`MapJson::SerializeCompact`, and
  `EditorLevelDocument::SerializeMap` renamed to **`CompareText`** to say so). The dirty check only ever
  asks *same or not*; indentation is 542 KB vs 231 KB at 1k entities. Worth ~8%, less than it looks — the
  per-value work dominates, not the whitespace. **A baseline built compact may only be compared against
  compact**, so the one place that genuinely needs the file's bytes — MP6's round-trip check — now
  serializes indented explicitly, once per mount.
  - `MapFile::SaveText` was added for the same reason: `SaveMap` held the file text *and* called
    `MapFile::Save(path, data)`, which serialized the whole map a **second** time.
- **NET (Release, medians of 5 interleaved A/B runs, `MapCapturePerf.cpp`):** the editor's check at 1k
  entities **5.98 → 4.08 ms** (1.5×), at 100 entities **0.61 → 0.38 ms**, at 10k **84.9 → 66.6 ms**. The
  same *indented* work — what a Save pays — is **5.98 → 4.35 ms** (1.37×).
- **THE PIE-FREEZE WORRY WAS MISDIAGNOSED, and the bench now says so.** `WorldManager::CloneWorld`
  captures but **never serializes** — MapData goes straight to `MapFactory::Instantiate`. It pays stage 1
  alone: **1.14 ms per 1000 entities**, not the full pass. The remaining ceiling is `nlohmann::json`'s
  `std::map<std::string, json>` object: ~390 ns to build a five-float component. Cutting that means
  `ordered_json`, which `NLOHMANN_DEFINE_TYPE_INTRUSIVE` hard-codes against — it would break every game
  component, so it stays a last resort (**I8**).
- **THE CHECK DOES NOT RUN OUTSIDE EDIT MODE** (2026-08-13). It used to, and it was answering a question
  about the wrong world: during PIE the active world is the Play **clone**, whose entities carry the
  source's `OwnerMap` (`MapFactory` restores it) and whose Level adopted the source's mounts (**WM6**).
  So the check compared a world being *simulated* against the *authored* baseline — every map lit `*` the
  moment the game moved anything, and the full capture was paid on the frames least able to afford it.
  `RefreshDirtyCache` now gates on `PIE.IsEdit()`, the same predicate `MapOps::CanEdit` already used. The
  general shape: **a derived answer is only meaningful against the world its baselines were taken from.**

**MP6 — Adopting a map checks ROUND-TRIP STABILITY and logs it either way.** The milestone rests on
world→file→world→file being a fixed point; when it is not, every Save rewrites the map with churn around
the one value that changed — and that is silent otherwise, because the map still loads and the world still
looks right. `EditorMapDocument::AdoptExisting` compares its fresh baseline against the bytes on disk and
says *"Round trip is stable"* or warns. It is the one gate on the whole layer that needs no human.
- **BYTE-EXACT INCLUDES THE ENTITY ORDER, and that order is the CAPTURE's, not the file's** (found by
  ②'s transform migration, 2026-08-27). Formatting parity — nlohmann `dump(4)`, keys sorted, no
  trailing newline — is the easy half and is not the whole contract: `entities` is a json ARRAY, and
  `MapSerializer` walks `view<EntityMeta>`, whose order is not the order `MapFactory::Instantiate`
  created them in. A hand-authored file therefore has to match an order only a RUN can reveal. It
  bites exactly when a NEW entity is hand-inserted, because every pre-existing one is already in
  writer order. **Cheapest reliable route: make the change, let the EDITOR save the file, and take
  its output as the source of truth** rather than hand-producing the writer's format.
- **The warning NAMES the divergence now**, because "it differs" sent a reader to diff two 130-line
  files by eye and cost four wrong hypotheses: it prints the first differing byte offset, both
  lengths, and a window from each side. One run then identified it. A diff-shaped check that yields
  one bit is [[L15]]'s discriminate rule half-applied.

**MP7 — "Open Map" LOADS NOTHING, because every map of the open level is already mounted** (landed
2026-08-08 with **WM8**). It re-targets the document onto a map that is in the world, and the selection
survives untouched because none of the entities it points at go anywhere. The editor holds an
`EditorLevelDocument` beside its map one — a session has a **Level** open — and that document holds only
the path and the baseline: **the manifest lives in the world's `Level`**, one owner (**WM8**).
- **Which map a file IS gets asked of its ENTITIES, never of its path** (`MapData::OwnerId`, **WM2**). A
  path arrives from tinyfd as `C:\...\Main.opaaxmap` and from `AssetToAbsolute` with forward slashes, so
  a string compare could not answer "is this one already in the world?" at all.
- **A map belonging to no open level gets its OWN world with an empty Level**, rather than being merged
  into a level it is not part of — which is also what keeps "which maps are in this world?" to one answer.
  *(An earlier pass mounted the level's persistent map alongside any picked map and deduped by `MapId`;
  that was the missing `Level` object leaking into the document, and it is gone.)*
- **`EditorMapDocument::DeriveMapId` takes a `MapData`, not a `World`.** The first valid `OwnerMap` in the
  registry was correct only while a world held ONE map; it holds several now, so it would answer with the
  *persistent* map's id and every Save would write the wrong map under the right name. The entities are
  still the authority — they are asked of the file. `AdoptExisting` already read that file for **MP6**, so
  it costs no extra IO: one read, deserialized for the id and compared as text.
- **Re-targeting still CONFIRMS unsaved work**, even though nothing is destroyed: `AdoptExisting` re-bases
  the baseline off the world, so edits to the map being left would quietly stop being reported.
- **A VERB THAT ACTS ON ONE MAP NAMES ITS MAP** (2026-08-09, user's call). The menu bar carried
  `Level → Remove Open Map` and `Level → Set Open Map Persistent`, which acted on whatever map happened to
  be FOCUSED. A level holds several maps (**WM1a**), so an implicit focus is not how an author picks one
  of them — and choosing the persistent map especially needs somewhere to **see** the current answer, not
  only a verb aimed at the cursor. Both left the bar for the Hierarchy's map headers, where the map you
  right-click IS the argument and the persistent one is labelled. `Level → Add Map...` is the one entry
  that stays, because it is the one that does not need a map named first — it goes and picks one.
  - The bodies live in **`Editor/Operation/MapOperations.h`** (`MapOps`), not on `EditorService`: two call
    sites now choose the target differently (the File menu from the cursor, the Hierarchy from the header)
    and a verb duplicated per call site is a verb that drifts. `File → Save Map` is the same `MapOps::Save`
    the header's entry calls, passed the focused id.
  - Entries are **DISABLED rather than left to be refused**. `Level::RemoveMap` turns down the persistent
    map and `SetPersistentMap` on it is a no-op; both would otherwise answer a click with a log line the
    author never reads. The menu states the rule instead of discovering it.
  - **A PANEL'S DRAW PASS READS THE WORLD; ANYTHING THAT WRITES IT RUNS AFTER THE PASS** (2026-08-09,
    fixing a crash). The context menu **records** the verb and `HierarchyPanel::RunPendingAction` runs it
    once the walk is over. Called inline, `Remove from Level` destroyed the entities whose handles the
    rows *under that same header* had been collected from at the top of `Draw`, and entt asserted on the
    first `Get<EntityMeta>` — *"Set does not contain entity"*. The queue is also cleared on
    `OnActiveWorldChanged`: a verb naming a map of the world that just left must not run against the one
    that replaced it. **This generalises to every panel** — an ImGui click arrives mid-iteration by
    construction, so a command that mutates the world cannot be invoked from inside a loop over it.
  - **`*` LIVES ON THE MAP, in the Hierarchy** (2026-08-09, user's call), and the menu bar's
    `Level.opaaxlevel > Map.opaaxmap` status text is **deleted**. One marker per map beside the map it
    belongs to says which map changed; a single `*` beside a focused-map name could not. The answers come
    from `EditorLevelDocument::RefreshDirty` — cached per record, **throttled by `EditorService` at 4×/s**
    (**MP5**), because the Hierarchy asks once per map per frame and the check is a capture + serialize.
    The throttle stays with the frame clock, the answers stay with the baselines. A **transition** log
    (`Map 'Decor' has unsaved changes` / `matches its file again`) makes a marker that is otherwise one
    pixel verifiable at all ([[L12]]), and fires twice per edit session rather than per check.
  - **`File/New Map...` is the workflow that was missing in front of Save As** (landed 2026-08-09).
    Registered FIRST, since registration order is draw order. It writes the file **immediately** —
    empty, carrying its own `mapId` (**MP10**) — then `Level::AddMap`s it and focuses it. Written
    rather than held as an unsaved cursor because every layer below assumes a map has a file, and a
    "not on disk yet" state would be the only one of its kind in the editor. It **refuses a path that
    already exists**: the OS dialog's overwrite prompt is one an author is used to clicking through,
    and New Map truncating a populated map is not a thing to leave to that. `Open Map` and
    `Level/Add Map...` are the verbs for a file that exists.
    - **What it writes is already canonical**, which is why a fresh map opens CLEAN: `MapJson`
      re-serializes it byte-identically, so **MP6**'s adopt-time round-trip check passes and the map
      does not report unsaved changes it does not have. Pinned by a test.
    - `MapFile::StemId` went **public** for it — the same rule `Load` uses as its last fallback is
      `New Map`'s first answer, and a naming rule with two copies is a naming rule with two answers.
      `EditorMapDocument::DeriveMapId` now calls it too, deleting a third.
  - **CONTENT IS BATCHED; STRUCTURE IS NOT** (settled with the user 2026-08-09, after they lost a
    membership to the old behaviour). Entity edits accumulate and wait for `Save Level` — that is what
    batching is *for*. Changing **which maps a level has** — New Map, Add Map, Remove from Level, Set
    as Persistent — is a deliberate one-off act, so `EditorLevelDocument::SaveManifest` writes the
    `.opaaxlevel` **as it happens** and rebases its baseline.
    - **The failure this fixes was observed, not theorised.** `New Map` wrote the map file and left
      the membership pending; the editor closed, the file stayed, and the level had forgotten it. A
      command that half-commits to disk is the worst of both — and once the menu bar's status text
      was gone, nothing drew the pending half either.
    - **The chosen fix removes the need for a marker rather than adding one**, which is why it was
      preferred to putting a level name back on screen. A manifest is now never dirty for long enough
      to need drawing.
    - `Save Level` still writes the manifest and normally finds nothing to say (**MP9** unchanged).
      Adopting a level does **not** write it — verified: the file's bytes and mtime survive a boot.
    - A failed manifest write leaves the baseline **untouched**, exactly as `SaveMap` does, so the
      document keeps reporting unsaved structure rather than claiming a file it never wrote.
    - Cost, accepted: an accidental `Remove from Level` is on disk at once. The map FILE is untouched,
      so `Level/Add Map...` puts it straight back.
- **The editor boots on the first NON-PERSISTENT mounted map**, falling back to the persistent one when
  that is all there is. The persistent map is the shared backdrop, authored once precisely so that it is
  not the thing being worked on. Asked of the MOUNTED maps, not the manifest — a map that failed to load
  is not editable.
- **The Hierarchy groups rows by `OwnerMap` and privileges NONE of them.** An earlier pass greyed every
  map but the focused one, justified by "a filtered Save will not write the others" — **MP9** made that
  justification false, so the greying went with it. The focused map's group merely starts open. The one
  real difference that survives is `(runtime - not saved)`: no map authored those entities, so no Save
  can ever write them (**WM2**), and a bare "(runtime)" label was not enough to convey it.
- **The headers come from the LEVEL, not from the entities** (2026-08-09). Groups are seeded from
  `Level::GetMountedMaps()` in mount order and the entities are bucketed into them, so a map that is in
  the world with **nothing in it** still has a header. Derived from entities alone it had none — an empty
  map was invisible in the one panel that lists a level's maps. Bucketing stays a plain `OwnerMap`
  compare: **MP10** made every mounted map's id valid, so the runtime bucket (the invalid id) is the only
  thing an unclaimed entity can land in.

**MP8 — `IPaths::AbsoluteToAsset` exists because the editor AUTHORS asset references.** A level manifest
names its maps asset-relative and a file dialog hands back an absolute native path, so the inverse of
`AssetToAbsolute` stopped being optional the moment `Add Map...` existed. It canonicalises **both** sides
before comparing (`weakly_canonical`, since a Save As target need not exist yet) — a string compare would
call a file plainly inside `Assets/` "outside" it. **Empty is a real answer, not a failure:** a file from
under **no mount** cannot be named by a manifest at all, and every caller refuses with that reason.
*Amended 2026-08-24 (**I16**'s mount bullet): it answers TWO roots now — the project's `AssetsDir`
first, then `EngineAssetsDir()` as `/Engine/…` — so "empty" means "under neither", not "outside
`AssetsDir`". Two consequences, both accepted:* a Save-As dialog pointed inside the engine's own assets
now yields a `/Engine/` path instead of empty (a deliberate navigation, and naming an engine-shipped map
is legitimate), and the *only* thing that still answers empty is a file genuinely outside both trees.

**MP11 — A MANIFEST ENTRY THAT NEVER MOUNTED IS NAMED BY ITS PATH, AND MUST BE REMOVABLE** (landed
2026-08-27, user-verified with the one click that fixed the level). A map whose file is missing,
renamed or moved leaves an entry with **no `MapId`** — an id comes from the file's entities
(**MP10**) and there is no file — so `RemoveMap(MapId)` cannot name it, and the Hierarchy seeds its
headers from the MOUNTED maps, so it had no row to hang a menu on.
- **The entry was therefore UNREACHABLE from the editor**, and the only repair was hand-editing the
  `.opaaxlevel`. That is not a theoretical hole: `Sandbox`'s `Main` level warned on every boot for
  weeks and got recorded as "pre-existing, a content call" precisely because nothing could act on it.
  **A state the app can produce and cannot repair is a missing verb, not a content problem** —
  [[L19]]'s shape, one layer up: the note explaining why it was not fixed *was* the work item.
- `Level::RemoveMissingMap(path)` refuses a path that IS mounted (that one has entities and goes
  through `RemoveMap`) and refuses the persistent map either way, because re-pointing persistence
  silently is the last surprise an author repairing a broken level needs. Both removal verbs share
  ONE `EraseFromManifest`, so the `PersistentMapIndex` fix-up cannot drift between them.
- **The Hierarchy DERIVES the missing set** as the difference between the two lists `Level` already
  publishes — a getter would allocate one per frame to say the same thing — and lists each with the
  same reasoning **MP10** gives for an empty map having a header: this is the only place a level's
  maps are listed, so it is the only place one can be named.
- **A group with an invalid `MapId` collides with the RUNTIME bucket**, which also has one. Without
  an explicit skip every runtime-spawned entity files itself under a map that does not exist. The
  general shape: **when a sentinel gains a second meaning, every existing comparison against it is
  now ambiguous** — grep them rather than trusting that the new case is disjoint.

**MP10 — A MAP NAMES ITSELF (`MapData::Id`, the `mapId` key), and "capture one map" / "capture the whole
world" are TWO NAMED FUNCTIONS** (landed 2026-08-09).

**The defect this closed.** A map's identity used to be derived from its entities alone
(`MapData::OwnerId` — the first valid `OwnerMap`, **WM2**), so a map with **no entities was anonymous**.
Meanwhile `MapSerializer::Capture(world, registry, filter = {})` read an **invalid** filter as *no
filter, take everything* — correct for the PIE clone (**WM6**), catastrophic for "save this map". The two
met in `EditorLevelDocument`, which keys its records by `MapId`: a mounted empty map would have been given
a baseline holding **every entity in the world**, and the next Save would have written them all into that
one file. Silent, and indistinguishable from a working save.

- **Identity is settled at the boundary that has the information, in three steps, each in its own layer:**
  `MapJson` reads the **`mapId` key**, falling back to what the **entities** claim; `MapFile::Load` falls
  back once more to the **file's stem**, because it is the only layer holding the path. Everything above
  reads `MapData::Id` and may assume it is valid. *This supersedes **WM8**'s "the file-stem fallback is
  the EDITOR's rule, not the engine's" — that was right while identity could only come from entities, and
  the editor still keeps its own copy for a Save As target, which is a file that does not exist yet.*
- **`mapId` is ALWAYS written**, as `""` for a capture that named no map (**MP1**'s convention). A map
  that only sometimes says what it is puts the reader back to guessing exactly where guessing was the bug.
- **NO FORMAT VERSION BUMP**, and the rule is why: `MAP_FORMAT_VERSION` moves only for what a v1 reader
  would MISREAD. A v1 reader ignores `mapId` and derives from the entities — the same answer for every map
  that has any, and for one that has none it lands where it already was. Same reasoning that added
  `persistentMap` to the level format.
- **`Capture` SPLIT INTO `CaptureWorld` / `CaptureMap`, and that is the load-bearing half.** One shared
  private walk, two public names: `CaptureWorld` takes everything and leaves `Id` invalid (a snapshot is
  not a map and never reaches a file); `CaptureMap` filters, **stamps `Id`**, and an invalid id captures
  **nothing** with a Warn. The dangerous reading is now unwritable rather than merely discouraged — the
  [[L18]] shape: make the wrong thing impossible, not unlikely.
- **What it deleted.** The `TrackMounted` guard that skipped anonymous maps, the Hierarchy's stem-label
  branch and its disabled "no map id" entries, and `EditorMapDocument::DeriveMapId`'s entity walk. Every
  mounted map now has a record, a header, and working verbs — an empty map is an ordinary map.
- The general shape is [[L30]]'s: a field two things share only because one of them is always empty.
  `MapId` was serving as both *"which map"* and *"unfiltered"*, and the empty map is where the two
  meanings collided.

**MP9 — "Save Level" writes the manifest AND EVERY MAP IN IT** (settled with the user 2026-08-08:
*"all maps has to be saved if we say 'save level', its mean save the level so all the maps in it"*).
A Level IS its maps, so writing the list of maps while leaving the maps themselves unwritten is the
shape of a save that loses work.
- **Maps first, then the manifest** — a list that names content must never be written ahead of it.
- **A map whose text matches its baseline is SKIPPED**, not rewritten with identical bytes: a Save
  should not touch the mtime of a file it had nothing to say about.
- `Save Map` still exists and still writes exactly one — the focused map. The two commands differ in
  scope, not in mechanism; both go through the same per-map record (**MP5**).
- **The earlier behaviour was the bug, not the UI that exposed it.** Filtering a Save to the focused map
  made the Hierarchy grey the others to warn that their edits would be lost — a warning is not a design.

---

## X — Old/new coexistence

**X1 — The old world is quarantined in `Engine/Source/Legacy/` and dropped from the build (2026-07-19).**
`CoreEngineApp`, every `*Old` type, `EventOld/`, the old `Scene/`/`World/`/`ECS/`/`Physics/` trees, the old
editor (`Engine/Source/Editor/`), the old subsystems (`Core/Systems/{GameSubsystem,MoverSubsystem,
PhysicsSubsystem,Movement}`), the old renderer (`Camera/` controllers, `Pass/`, `Systems/WorldRenderSystem`,
`RenderSubsystem`), the static `RenderCommand` path, and **`Core/OpaaxPath`** (the old static path system —
five mutable statics, i.e. an **I1** violation; superseded by `IPaths`/`ResolveProjectLayout`, quarantined
2026-07-28) — all live under `Engine/Source/Legacy/`, **NOT globbed by the engine DLL** (compiled = zero). Their old-world tests live in `Engine/Tests/Legacy/`. **Do not add new
dependencies on any of it, and do not re-glob `Legacy/`.** It will be deleted; anything you hang off it dies with
it. *(Exception: `Core/Systems/Subsystem.h` — `ISubsystem`/`ISubsystemManager` — stayed LIVE; it is the base of
the new `EngineSubsystemBase`.)* Next: Gregory-layer the live remainder (plan `~/.claude/plans/inherited-orbiting-clover.md` — the
auto-named plans live in the USER-level `.claude/plans/`, not the repo's).
**X2 — New systems get collision-proof identities up front.** When old and new coexist, the *new* type gets
a scoped `enum class` / distinct name — never rely on include order or forward-decl tricks to avoid a
clash. Two unscoped enums sharing enumerator names collide the moment one TU needs both (**L4**).
**X3 — A rename to `*Old` carries through to enumerators and consumers**, not just the type name.
**X4 — New code takes the LIVE vocabulary, never the quarantined one** (M2d, 2026-07-27). `Asset` belongs
to the retired `Legacy/Assets` world (`IAsset`, `AssetRegistry`, `AssetManifest`); the live system is
`CResource` / `ResourceManager`. So the editor's file browser is `ResourceBrowserPanel` on a
`ResourceTypes()` route, not `AssetBrowser`/`AssetTypes`. Before naming anything, grep the term: if
`Legacy/` owns it, the name is taken — reusing it makes every future search ambiguous and quietly
suggests a lineage the new code does not have. Older planning docs predate such renames; the CODE is the
vocabulary of record.
- **The manifest's last traces are gone (2026-08-21).** `AssetManifest.json` outlived its reader by
  months in three places that were not code and so never showed up as dead: the two shipped `.json`
  files, a CMake block that **regenerated one if you deleted it**, and the `Assets.EngineManifest`
  config field. A retired system's *data* and *build steps* are part of the quarantine — grep the
  term in `CMakeLists.txt` and `*.config` too, or the tree keeps re-creating the corpse.

**X5 — A SETTING with no reader is deleted, not kept as a spec** (2026-08-21, user's call). The
config had five groups; `Assets`, `Log` and `Physics` plus `Render.Interpolation` had **no reader
anywhere** — kept, deliberately, as the shape the unbuilt systems would want. Two things made that
wrong once the editor grew a Config panel (**BO1a**): the values became *visible and editable*, so
the file now promised behaviour it did not have, and the "spec" was never a spec — a physics system
will decide its own settings, not inherit a guess made before it existed. `EngineConfigData` is now
`Window` + `Render`, and **every field in it has a reader**. Nothing is lost: the shape is one
struct and one macro line, re-added with the system that reads it, and a config file still carrying
the old groups opens fine (nlohmann ignores undeclared keys — pinned by a test).
- Same rule retired the unread keys in `.opaaxproj`: `ParseProjectIdentity` reads four,
  `Sandbox.opaaxproj` carried nine. Its `version` key went with them, exactly as **BO1b** retired
  the config's. `.opaaxlevel`/`.opaaxmap` keep theirs — `LevelFile::Load` genuinely checks it.
- **The counter-case, and the line between them:** `IProjectManager`'s `startupScene`/`defaultScene`
  fallbacks have zero users in the tree and **stay**. They are not a spec for something unbuilt, they
  are *tolerance for input you cannot see* — a project file on a disk this repo does not contain.
  Three test cases pin them as **X4** migration tolerance. Dead-by-grep and dead-by-contract are
  different questions; only the second licenses a delete.

---

## PL — Placing a new system (decision procedure)

1. **Name its Gregory layer** (**I4**). Passive facility ⇒ app service. Ticks / owns game concepts ⇒ engine subsystem.
2. **App service?** interface + `OPAAX_SERVICE_TYPE` (out-of-line `.cpp` — **I2**), `Null()`/`IsNull()`
   (**I3**), provided in `Bootstrap` at the right dependency point (**BO**), torn down reverse.
3. **Engine subsystem?** `EngineSubsystemBase` + `OPAAX_SUBSYSTEM_TYPE`, registered with `EngineSubsystemMgr`,
   implement `Startup`/`TearDown`/`Shutdown` honoring **LC**; borrow siblings via **F3**.
4. **Ownership** follows lifetime (**I5**); **no new static** (**I1**).
5. **Simplicity gate:** does a mid-sized indie engine really need this now? Simple > clever
   (project value #1). If a premise balloons past its sketch, STOP and surface a scoped fork with a
   recommendation (**L3**) — don't build the expensive version unseen.

---

## CH — Change protocol (how we work; full text in CLAUDE.md)

- **Build truth:** `./build.bat` ends with `OPAAX_BUILD_OK` / `OPAAX_BUILD_FAIL` and a real exit code.
  Grep the marker; never trust "looks done." `build.bat fast [target]` = seconds-long incremental check.
- **Forks up front:** surface the 2–3 real scope/premise forks *during planning*, with a recommendation,
  decide once, then build the decided version. Most back-and-forth in `lessons.md` is a fork found mid-build.
- **Gates:** every milestone step has a demo gate + unit-test gate. A red build right after your change is
  not proof your change broke it — `git status` shows your true blast radius (**L5**).
- **Perf is a gate, not a vibe:** hot-path systems (bullets, sprites, bodies) carry a *loose* perf budget.
  Bench cases live in `Engine/Tests/Perf/` as a doctest suite `"perf"` marked `skip()` — they never run in
  `build.bat test`/CI (unit baseline stays 106/426). Run RELEASE via **`./build.bat bench`**. Soft gate:
  budgets catch algorithmic/allocation regressions (O(n²), per-op alloc), NOT micro-noise; watch the printed
  ns/op yourself for ~2× drift. Helper: `Engine/Tests/Perf/PerfBench.h` (median-of-epochs, homegrown — a
  loose gate doesn't need nanobench). Add a case → add its path to `Engine/Tests/CMakeLists.txt` (explicit list).

---

## Pointers

- **Post-mortems / rules:** `.claude/lessons.md` (L1–**L79**).
- **Live session state:** `.claude/CLAUDE.local.md` (current milestone, standing decisions).
- **Working checklist:** `.claude/task/todo.md`.
- **Ground truth for engine design:** `.claude/data/` — *Game Engine Architecture* (Gregory). Prefer it over
  generic advice.
