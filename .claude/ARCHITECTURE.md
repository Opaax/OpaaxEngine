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
  Platform, Paths, Logger, Config, ProjectManager, JobSystem, WindowManager. They do not tick.
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
  `IRenderTarget` (backbuffer or offscreen FBO — `RendererManager` picks via `m_PrimaryTarget`, size read
  from the target). "scene" is retired vocabulary — a render *pass into a target with a view*. New render
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
every frame; nothing is retained. Lines render as thin rotated quads through the existing
`Renderer2D::DrawQuad` on the `ERenderLayer::Debug` band — **zero new RHI/shader/vertex-layout surface**;
keep it that way. Engine-owned, not editor-owned (D10: it serves dev builds of `Game.exe`, which never
links `OpaaxEditorLib`); reached via `IEngine::GetDebugDraw()`.

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
"stop calling" cannot give you, since the toggle must live outside the producer. Deliberately **not built**
while there is one producer (`m2-panels.md` §F3 — never an API with no caller). **Trigger:** the second real
producer (physics/collision debug), which also earns the editor toggle panel. The cross-module identity
question this raised is already **closed** — `OpaaxStringID`'s intern pool was moved out-of-line into the
DLL the same day (see **I2**), so channel ids agree across the DLL line by construction.

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
- `MakeViewProjection` and `ScreenToWorld` are the two questions a view answers, defined **out of line
  and exported**: inline would drag `glm/gtc/matrix_transform.hpp` into every TU that includes
  `World.h`, and the editor calls `ScreenToWorld` from the exe (**I6**). `ScreenToWorld` is the ONE
  screen→world rule — zoom-at-cursor needs it now, picking and gizmo placement need the same answer.

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
- Two one-shot Info lines ([[L48]]): the seed, and the first move. Without them "pan does nothing"
  cannot be told apart from "the gesture never arrived", and only one of those is fixable.

**CAM5 — `CameraComponent::Position` is DEBT ON PURPOSE.** It carries the standing comment
`SpriteComponent` and `DummyComponent` already carry: it moves the day a transform exists. That makes
③'s fold **three** components, not two — recorded in `.claude/plans/engine-sequence.md` §③.

**CAM6 — What ① deliberately did NOT build** ([[L23]] — never an API with no caller): follow, shake,
priority/blending, `ViewportRect` and multi-view (⑥ owns it; `RenderView` already promises it is nearly
free), perspective, confiner/bounds, a scene view that detaches from the game camera during PIE, and
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
replacing `MenuRegistry`). `Menus()` now returns an **`EditorMenu`** (`Editor/Menus/`): root categories
in `Category()`-call order, each holding one ordered list of `TUniquePtr<IEditorMenuNode>` —
`EditorMenuCategory`, `EditorMenuCommandNode`, `EditorMenuSeparatorNode` — so categories, entries and
separators interleave and nesting goes as deep as it is written.
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
in the editor that calls `ImGui::Begin`.
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
`QuadOscillatorSubsystem` is the worked example (baselines on first tick). If something ever genuinely
needs a populated-world moment — physics rebuilding bodies from authoring components — the answer is an
explicit post-instantiate hook (Unreal's `OnWorldBeginPlay` beside `Initialize`), **named here and
deliberately not built**: it has no caller yet.

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

- **Post-mortems / rules:** `.claude/lessons.md` (L1–**L32**).
- **Live session state:** `.claude/CLAUDE.local.md` (current milestone, standing decisions).
- **Working checklist:** `.claude/task/todo.md`.
- **Ground truth for engine design:** `.claude/data/` — *Game Engine Architecture* (Gregory). Prefer it over
  generic advice.
