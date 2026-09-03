# ⑥ S3 — SPRITE ANIMATION

## Context

⑥ S2 landed sprite sheets: `SpriteSheetData` holds a texture plus an ordered, *named* list of
`SpriteFrame`s, and `RendererManager::ResolveSpriteDraw` already turns a `SpriteComponent`'s
`Sheet` + `Frame` into UVs (**SS1–SS3**). Nothing advances that `Frame` over time, so every sprite
in the engine is a still image — the shmup and the platformer both need a moving one.

**SS1 already promised this block's shape:** `SpriteFrame::Name` was made an `OpaaxStringID` (the
user's call) with the stated reason *"Animation will look frames up by it."* This delivers on that.

**The headline property: it is purely additive.** `Renderer2D`, `RendererManager` and every shader
are untouched — an animator writes into the `SpriteComponent` the renderer already reads. It is
**D7** exactly: authoring data + a world subsystem, never a polymorphic component.

### Decisions taken with the user (2026-09-03)

| Fork | Chosen |
|---|---|
| Clip storage | **TWO assets.** A `.opaaxclip` is ONE clip — the unit that grows a notify track later. A `.opaaxanim` is a **library**: an alias table mapping short gameplay names to clip assets, so switching Idle→Run is an integer write, not a string copy. |
| Frame sources | **Both.** A clip names a sheet (steps reference frames by name) *or* carries a list of textures. |
| Preview | **Play worlds only** (`ShouldCreate`, exactly `QuadOscillatorSubsystem`). Authoring preview lives in the clip panel on the document's own copy (**SS4**). Nothing can dirty an authored map. |
| Frame reference | **By name**, resolved to an index once at bind. Survives re-slicing a sheet; a missing name warns once. |

**Why the clip is its own asset** (the user's call, and it is the load-bearing one): a notify track,
curves and events all hang off a *clip*, not off a character's list of clips. Making the clip the
asset means adding them later is one field in one file, with no library, component or subsystem
change. It also makes a clip **reusable** across characters, which a library-embedded clip is not.

---

## Design

### 1. Clip — `Engine/Source/Engine/Subsystems/Resources/Types/AnimationClip*`

`SpriteSheetData` / `SpriteSheetFile` / `SpriteSheetResource` is the stack to mirror, one layer over.

**`AnimationClipData.h` / `.cpp`** — the `.opaaxclip` payload.

```cpp
enum class EAnimPlayMode : Uint8 { Once, Loop, PingPong };
// + ToString(EAnimPlayMode) (I11) + OPAAX_ENUM_VALUES (I15 → a dropdown, no drawer written)

struct AnimationStep
{
    OpaaxStringID                  Frame;      // SHEET clips: the SpriteFrame's name (SS1's promise)
    TResourcePath<TextureResource> Texture;    // TEXTURE-LIST clips: the image
    Uint32                         Hold = 1;   // how many ticks at the clip's Fps (Unreal's FrameRun)
};

struct AnimationClipData
{
    TResourcePath<SpriteSheetResource> Sheet;   // SET ⇒ steps read Frame; EMPTY ⇒ steps read Texture
    TDynArray<AnimationStep>           Steps;
    float                              Fps      = 12.f;
    EAnimPlayMode                      PlayMode = EAnimPlayMode::Loop;
    Uint32                             TotalTicks() const;
};
```

- The `Sheet`-set-wins fork is **SS3 one layer down**, deliberately the same rule a sprite already
  follows, so there is one thing to learn rather than two.
- `Hold` in integer ticks (not seconds) is Unreal's `PaperFlipbook` shape and Aseprite's: pixel-art
  timing is quantized, so integers remove float drift and read better in the panel.
- **The notify track is what this asset exists for and is deliberately NOT built** — it would be
  `TDynArray<AnimationNotify>` where a notify is `{Uint32 Tick; OpaaxTag Tag;}`. **I14**'s tag is
  already the right type (hierarchical, four bytes, no registry), and the subsystem would publish on
  the bus it already has through `WorldContext::Events`. Naming the shape now is what keeps the file
  format additive; it has no reader today (**X5**).

**The one pure, free, tested function** — `MakeFrameUV` / `SliceGrid` / `PlanQuadBatches`' idiom:

```cpp
struct AnimationSample { Uint32 Step; bool bFinished; };
OPAAX_API AnimationSample SampleClip(const AnimationClipData& InClip, float InTimeSeconds);
```

`tick = floor(time * Fps)` → `Once` clamps and reports finished, `Loop` wraps `% total`,
`PingPong` mirrors over `2*total - 2` → prefix-scan `Hold` to the step. **Stateless per tick**, so
playback cannot drift and a frame hitch skips rather than queues. Degenerate input (no steps, `Fps
<= 0`, negative time) answers step 0 — never a divide by zero, the same refusal `MakeFrameUV` makes.

**`AnimationClipFile`** — `.opaaxclip` read/write, `SpriteSheetFile`'s shape exactly (version key,
`dump(4)`, keys sorted, **no trailing newline** — **MP6**).
**`AnimationClipResource`** — the `CResource` adapter, `SpriteSheetResource`'s body:
`EFailPolicy::Placeholder` (an empty clip animates nothing and the sprite keeps its authored frame —
degraded but honest), `OPAAX_RESOURCE_FORMAT("Animation Clip", CLIP_EXTENSION)`, no `Initialize` (no
GPU), and like the sheet it does **not** `Acquire` its sheet — **SS3**'s placement fact is unchanged.

### 2. Library — `…/Types/AnimationLibrary*`

`.opaaxanim`. Its whole job is to give a character's clips short, stable, **gameplay-facing** names.

```cpp
struct AnimationLibraryEntry
{
    OpaaxStringID                        Name;   // what gameplay writes: OPAAX_ID("Run")
    TResourcePath<AnimationClipResource> Clip;
};

struct AnimationLibraryData
{
    TDynArray<AnimationLibraryEntry> Entries;
    OpaaxStringID                    DefaultClip;
    const AnimationLibraryEntry*     Find(OpaaxStringID) const;  // invalid → DefaultClip → first → null
};
```

`Name` is explicit rather than derived from the file stem, because the alias is the *point* — a
`"Hero_Run.opaaxclip"` should be reachable as `"Run"`. To keep the two from drifting for no reason,
the editor **pre-fills** `Name` with `PathString::Stem` (**I13**'s one stem rule) when a clip is
dropped in, and leaves the field editable.

Both resources registered in `Engine::RegisterNativeResourceFormats()` (`Engine/Engine.cpp:83`).

### 3. Component — `Engine/Source/World/Components/SpriteAnimatorComponent.h`

```cpp
struct SpriteAnimatorComponent
{
    TResourcePath<AnimationLibraryResource> Library;   // SET ⇒ Clip names an entry in it
    OpaaxStringID                           Clip;      // invalid = the library's DefaultClip
    TResourcePath<AnimationClipResource>    ClipAsset; // used when Library is EMPTY
    float                                   Speed    = 1.f;
    bool                                    bPlaying = true;

    // ---- TRANSIENT: in neither the json macro nor the property table ----
    float         PlayTime = 0.f;
    OpaaxStringID BoundClip;         // detects a clip switch → restart at 0
};
```

**A one-off animated prop must not need two assets**, so a component names a library **or** a clip
directly, and `Library` wins when set. That is the *third* instance of an idiom already in the tree
(`SpriteComponent`'s `Sheet`|`Texture`, a clip's sheet|texture-list) rather than a new one — a
spinning coin costs one `.opaaxclip`, a hero with five states costs one `.opaaxanim` and five clips.

**Transient state in a component is new here and needs saying once** (a contract line):
`_WITH_DEFAULT` never sees these fields, so they cannot reach a `.opaaxmap`, and a PIE clone
round-trips through the map snapshot (**MP5**/**WM6**) — so a cloned world starts every animation at
zero, for free, with nothing to reset. Unreal's `PaperFlipbookComponent::AccumulatedTime` is the same
shape.

Registered in `Engine::RegisterNativeComponents()` as `"SpriteAnimator"`, and its drawer in
`EditorService::RegisterNativeDrawers()` (`EditorService.cpp:356`) — **I15**'s last bullet: the
engine registers its own components' drawers, and since it is `CReflected` the registration *is* the
implementation.

### 4. System — `Engine/Source/World/Systems/SpriteAnimationSubsystem.h` / `.cpp`

The **first engine-owned world subsystem**, so `Engine.cpp` gains a fourth
`RegisterNativeWorldSubsystems()` beside the three it has — the same "a native route with no
`RegisterNativeX()`" gap drawers had.

- `static bool ShouldCreate(const World&)` → **Play worlds only** (**WS2**), so in an Edit world the
  type is never constructed and an authored `Frame` is never written.
- `Update(double)` over `Each<SpriteComponent, SpriteAnimatorComponent>`:
  1. resolve library → entry → clip, or the direct `ClipAsset` (`ResourceRef` caches keyed by path
     id — `RendererManager::ResolveTexture`'s shape, logged once each, hit or miss);
  2. `Clip != BoundClip` ⇒ `PlayTime = 0`, rebind;
  3. `bPlaying` ⇒ `PlayTime += dt * Speed`;
  4. `SampleClip` → step;
  5. **sheet clip:** `Sprite.Frame = <bound index>`, and `Sprite.Sheet = clip.Sheet` *only when it
     differs* (a `TResourcePath` assignment is a string copy — not per tick);
     **texture clip:** `Sprite.Texture = step.Texture` (same guard), `Sprite.Sheet` cleared.
- **Binding resolves names once**, into a `{clip path id} → TDynArray<Int32>` table the subsystem
  owns; a name the sheet does not have warns **once** and leaves that step's frame alone (**BO4c**'s
  skip-and-say-so). This is where the sheet is loaded and scanned — never per tick, never per entity.
- **Contract line to write down:** while an animator drives a sprite it OWNS `Sheet`/`Texture`/`Frame`;
  the sprite's authored values are what shows when there is no animator. `bPlaying = false` freezes on
  the current step rather than reverting.

**`WorldContext` gains `const IPaths& Paths`** — needed to turn `"Anims/Hero.opaaxclip"` into an
absolute path for `ResourceManager::Load`. One member in `World/Systems/WorldContext.h`, one argument
at `WorldManager.cpp:206`; `WorldManager` **already resolves `m_Paths`** (`WorldManager.cpp:38`).
**WS3** explicitly sanctions this (*"adding a member later is one line"*) and gets amended in the
same change.

### 5. Editor — `Editor/Source/Editor/`

Mirrors the sheet stack file for file (`EditorSpriteSheetDocument` 139 · `SpriteSheetPanel` 559 ·
`SheetOperations` 134 · `SpriteSheetUndoables` 177 lines). Two documents, because there are two
assets — but they are very unequal in size.

**The clip editor (the real one).**
- `EditorAnimationClipDocument` — owns its own `AnimationClipData` copy, baseline-derived `IsDirty`,
  `Open`/`Close`/`MarkSaved`. **SS4** verbatim.
- `AnimationClipPanel` — clip fields (Sheet, Fps, PlayMode) · step list (add/remove/reorder, `Hold`) ·
  a **frame picker** listing the sheet's frame names · a **preview** (play/pause/scrub) drawing the
  current step · a `ResourceRef` claim released in `Shutdown` (**LC3**). The scrub row is where a
  notify track later hangs.
- `ClipOps` — `AddStep` / `RemoveStep` / `MoveStep` / `SetStepFrame` / `SetStepTexture` /
  `SetClipFields` / `Save`, each recording its own undo step (**UN1**).

**The library editor (small).**
- `EditorAnimationLibraryDocument` — the same document shape, ~90 lines.
- `AnimationLibraryPanel` — a row per entry (name field + clip drop target), add/remove/reorder, and
  the default-clip picker. No canvas, no preview.
- `LibraryOps` — `AddEntry` / `RemoveEntry` / `RenameEntry` / `SetEntryClip` / `SetDefaultClip` /
  `Save`.

**Shared by both.** `Save` writes the file **and** calls `ResourceManager::Reload<T>` — **SS4/SS5**,
the half that was missed once ([[L75]]) and must not be again. Every undo step carries its
document's **PATH** (**SS4**: one document open at a time, the stack outlives it); a step whose
document is not open is a no-op **with a warning**. Registration is
`RegisterNativeResourceTypes()` (`EditorService.cpp:386`) + `RegisterNativePanels()` (hidden by
default) + Ctrl+S routed by focused panel (`EditorService.cpp:905`'s existing branch).

**The icon PNGs are content I cannot author** — both fall back to a glyph (`[C]`, `[A]`) until they
exist (**I16**: glyph and icon are separate facets precisely so a missing image still draws).

### 6. Named, not built

Notify tracks (shape above) · a state machine or blending · a per-entity start offset for crowd
de-sync · multi-document editing. Each has **no reader today** (**X5**), and none changes a shape
above.

---

## Steps (each ends at a commit — **L17** — and stops for verification)

**S1 — Clip runtime.** `AnimationClipData` + `SampleClip` + `AnimationClipFile` +
`AnimationClipResource`, registered as a native format. `AnimationTests.cpp` added **explicitly** to
`Engine/Tests/CMakeLists.txt` (a file not listed there is never compiled): `SampleClip` Loop wrap /
`Once` clamp+finished / PingPong mirror / `Hold > 1` / degenerate; a byte-exact file round-trip and a
missing-key `_WITH_DEFAULT` case.
*Gate:* `build.bat test` green, **case count checked**, a hand-authored `.opaaxclip` loading and
logging its step count.

**S2 — Library runtime, component, subsystem, `WorldContext::Paths`, Sandbox dogfood.**
Ships **both** clip sources and **both** component routes at runtime (hand-authored files prove each
branch). A `.opaaxclip` beside `TSS_Default_128.opaaxsheet` and a Sandbox entity carrying the
animator.
*Gate:* a **number** in the log (`clip 'Idle' -> 4 step(s) @ 12 fps`), a sprite visibly cycling in
`Sandbox.exe` **and** in PIE, and the Edit-mode map **not** dirtied by a PIE run. This is the composed
gate — the feature has to have run in the real app ([[L23]]).

**S3 — Clip editor** (document, panel with preview + frame picker, ops, undo, registration).
*Gate (their eyes — a smoke run reaches none of this):* open a `.opaaxclip` from the browser, add
steps by picking frame names, watch the preview play, Ctrl+S, Ctrl+Z/Ctrl+Y, and see a **running**
sprite pick up the save (the `Reload` half).

**S4 — Library editor** (document, small panel, ops, undo, registration).
*Gate:* build a library from scratch — drop three clips in, name them, set a default — and switch a
running entity between them.

**S5 — Texture-list authoring in the clip panel.** Drop N textures into a clip, reorder, per-step
`Hold`.
*Gate:* a clip with no sheet animating from loose PNGs, authored entirely in the editor.

---

## Verification

- `build.bat test` after every step; grep `OPAAX_BUILD_OK`, never the exit code ([[L8]]); check the
  **case count**, not just green ([[L70]]).
- Full preset build before any smoke run, and `ls -la` the exe against the DLL ([[L24]]).
- Editor smoke runs: back up `Sandbox/Editor/Save/imgui.ini` first, restore after.
- Read the **whole** `Sandbox/Save/Log/OpaaxEngine.log`, not the lines I went looking for ([[L27]]).
- Hand-authored `.opaaxclip`/`.opaaxanim` must match the writer byte-for-byte or **MP6** warns —
  verify with `od -c` + `json.dumps(sort_keys, indent=4)`.
- Contract updates in the same change: a new **AN1–ANn** section, plus amendments to **WS3**
  (`Paths` member) and **SS1** (its "animation will look frames up by it" promise, now kept).
