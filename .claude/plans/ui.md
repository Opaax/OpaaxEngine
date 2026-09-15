# UI — record (branch `User_Interface`, 2026-09-14 → 2026-09-15) — CLOSED THREE TIMES, user-verified

> **U11–U12 closed 2026-09-15** on *"ok works, close the block"*. After *"Do we miss something in
> UI?"* I read the module back against a shmup HUD and priced the gaps; they took the recommendation:
> the reference height made the PROJECT's (a hole U7's field had made reachable) and "a HUD from
> real art" — anchor presets, a button with art, sheet frames on image and button through one
> resolve. `9562051`, `c45e914`. **920 / 10037 / 7**, drawers `7 for 7`. Durable: UI2/UI19 amended,
> **UI25**. **Found on the way, not reported:** the inspector's undo gesture had double-recorded
> every U7 verb button. **Their next ask, in the same breath:** *"The preview: can be nice to zoom
> etc..."* — the designer's view (zoom / pan / a chosen aspect) is the block after this one.
>
> **U7–U10 closed 2026-09-15** on *"ok works, close the block"*, after *"All works. Only binding is
> not working correctly"* → the level-swap fix `4f9288b` → verified. Reopened the same day
> (*"Lets continue UI works"*): they picked all four growth points I priced — designer polish ·
> layout groups · opacity + fade · `UIBinding` — sequenced by what becomes visible, each committed
> alone. Five commits `2498a33`→`4f9288b`. **913 cases / 9772 assertions / 7 skipped**, `UI widget
> drawers: 7 for 7`, `commands=50`. Durable: **UI22–UI24**, UI3/UI21/MR2i amended, [[L98]].
> **Their one report was the one thing no gate of mine could reach** — a world-tier owner on a
> GameInstance-tier table across a level swap; the U6 harness would have caught it and I did not
> run it (L98). **Still unseen by anyone:** a swap started inside PIE · two masks nested · the
> Camera Preview staying UI-free · a min cover floor of 0 (the Sandbox is at 3 s).
>
> **U1–U6 closed 2026-09-15** on *"close the block"*, after *"eye gate good. pie selection fixed, next
> level works."* Every seeded step landed and was eye-gated; 12 step commits + 2 fixes + their own
> `a1bce9a`. **896 cases / 9548 assertions / 7 skipped**, `UI widget drawers: 6 for 6`. Durable:
> **§UI** (UI1–UI21), IM6/MV1/GI2/TX10/F4d/F5 amended, [[L95]]–[[L97]].

| Step | Commit | What landed |
|---|---|---|
| **U11** | `9562051` | **The reference height is the project's** (UI2 amended). `ProjectIdentity::UIReferenceHeight` (`uiReferenceHeight`, 1080 when absent/bad/zero); the tenant's canvas takes it at Startup (`… reference height 1080 (project)`). `UISubsystem::LoadTree` + `MountAsset(path, parent)` replace three copies of the load block (cover, HUD, menu) and warn once on a height mismatch; `HudSubsystem` / `PauseMenuSubsystem` shrink to one call. `New UI…` seeds the project height; the panel's canvas inspector shows `(project: N)` and a warning line when different. 1 case, **913 → 914 / 9772 → 9776**. |
| **U12** | `c45e914` | **A HUD from real art** (**UI25**, UI19 amended). `ApplyAnchorPreset` / `CurrentAnchorPreset` (pure; anchors + pivot, then `FitRect` so nothing moves) and the panel's `Anchors…` 4×4 popup, one step. `IUIAssetProvider::ResolveSheetFrame` (defaulted) + the renderer's impl from its own caches. `UI/UIImageSource`: `ResolveImageSource`, runtime > sheet > texture, "named but not ready" apart from "nothing named". `MapQuadUVsInto`. `UIImage::Sheet`/`Frame` (slice against the FRAME, remap); `UIButton::Texture`/`Sheet`/`Frame`. **The inspector's undo gesture now brackets only its own fields** — every U7 verb button had recorded a phantom "Edit Widget" behind its own step. 6 cases, **914 → 920 / 9776 → 10037**. Harness ([[L81]], removed): frame 3 of the engine's 2×2 sheet, sliced — `9 quad(s); first UV (0.5, 0)-(0.5625, 0.0625)`. |
| **U7** | `2498a33` | **Designer polish.** `FitRect` (UIRect.cpp — `ResolveRect`'s inverse, anchors kept) · 8 grips on the selection, resize through `EditorRectGeometry`'s `HitTestRect`/`ResizeRect` in PIXEL space then `ScreenToCanvas` ×2 → `FitRect`, one step "Resize Widget", cursor per edge · `UIWidget::AddChild(child, index)` · `UICanvasFile::CloneWidget` / `SerializeNode` / `DeserializeNode` · `UICanvasOps` `DuplicateWidget` (" (1)"), `CopyWidget`/`PasteWidget` (the node's JSON is the clipboard text — paste crosses documents), `MoveWidget` (▲▼) · Ctrl+D/C/V/Up/Down window-local · **`DeleteUIWidgetCommand` + `.DeleteCommand`** — Delete in the UI panel used to delete a LEVEL entity · the canvas's reference height in the inspector when nothing is selected ("Edit Canvas") · `UIMask::bShowMaskGraphic`. 3 cases, **896 → 899 / 9546 → 9603**, `commands` 49→50. |
| **U8** | `55e2ac5` | **Opacity + the fade** (**UI22**). `UIWidget::Opacity` on the base (Unreal's `RenderOpacity`, range 0..1, the 5th base property); `UIDrawItem::Alpha` = the ancestors' product × own, carried by `CollectTree`, multiplied into the colour by `Submit`; a subtree at 0 emits nothing; read at draw time so a fade is 0/0. `PauseMenuSubsystem::Update` chases 1 / 0 over 0.15 s and hides the panel when the fade out lands — `Close` no longer hides, `Open` no longer needs to show more than the flag. 1 case, **899 → 900 / 9603 → 9618**. |
| **U9** | `03a14c3` | **`UIStack` + the pause menu as an asset** (**UI23**). One type with an `Axis`; `Spacing`, `Padding`, `ChildAlign`, `bFitContent`. The walk learned slots (`UpdateTree(…, InSlot, …)`), `m_bArrangesChildren` + `ArrangeChildren`, the up-propagation in `InvalidateLayout` / `AddChild` / `RemoveChild`. A hidden child keeps its slot. The designer does not move an arranged child; the inspector says "(placed by parent)". `UI/PauseMenu.opaaxui` (9 widgets: Dim, Box, Stack of Title / Resume / Next Level) hung under the code root; buttons bound by name; ~40 lines of builders deleted. All MR2i routes: registry 7, drawers `7 for 7`, drawer test, `MakeRegistry`. 7 cases, **900 → 907 / 9618 → 9699**. |
| **U10** | `c0f6610` + `4f9288b` | **`UIBinding`** (**UI24**). `UI/UIBinding.h/.cpp`: `UIBoundValue`, `ReadBoundProperty<T>` (fold by name, four readable kinds), `MakeBindingReader` (borrowed), `UIBindingTable` (`Add`/`Remove`/`Read("Source.Property")`, warn once), `FormatBoundText`. `UICanvas::Bindings()`; the pull at the top of `Update` through `UIWidget::OnPullBindings`, skipped when the table is empty. `UIText::Binding` (Text = the format, `{}`), `UIImage::FillBinding`; a first resolve traces once. `HudSubsystem` → `HudModel` registered as `"Hud"`, no widget named; `Hud.opaaxui` carries `Hud.Jumps` / `Hud.Speed`. 6 cases, **907 → 913 / 9699 → 9759**. Smoke: `'Jumps' bound 'Hud.Jumps' — shows "Jumps: 0"`, `'SpeedFill' bound 'Hud.Speed' — fill 0`. |
| **U1** | `276b905` | `Engine/Source/UI/`: `UIRect` + `ResolveRect`, `UIWidget` (3 flags, 2 verbs, one walk), `UICanvas` (view = `CameraView{0, H/2}`, stats), `UIPanel`, `UIImage` (fill). 16 cases, **810 → 826 / 9029 → 9114**. No caller yet. |
| **U2** | `6578fea` | The canvas over the world: `bDrawUI` opt-in on the world pass, `SubmitUICanvas`, `RenderCanvases` with **`ELoadOp::Load`'s first caller**, ST rows. `Text2D` box (wrap/align, scan-then-emit). `IUIFontProvider` + the re-arm. `UIText`. `UISubsystem` tenant + `WorldContext::UI`. Sandbox `HudSubsystem` (Jumps + speed bar). 10 cases, **826 → 836 / 9114 → 9161**. Contract **§UI** (UI1–UI8). **User-verified:** *"Eye gate good. Resize -> UI stay and resize correctly"*. |
| **U3** | `66b7b48` | Input bubbles (Slate's FReply), press captures, focus routes keys, detach clears (**UI9**). Three modes `GameOnly/UIOnly/GameAndUI` on the tenant, which now ticks BEFORE mapping; `UIInputRouter` reports a consumed mask the evaluator pre-consumes (**UI10**). `UIButton`. PIE pointer is viewport-local (**UI11**). Sandbox `PauseMenuSubsystem` (HUD button opens, UIOnly modal, Resume/Escape close). **A phantom Started on a masked-then-held key fixed in the evaluator** (`bMaskSuppressed`). 21 cases, **836 → 857 / 9161 → 9266**. §UI9–11, IM6 amended. |
| **U4** | `d020feb` | `.opaaxui`: tagged nodes + `SaveFields`/`LoadFields` + `UIWidgetRegistry` (**UI12**); `UICanvasResource` re-parses per instance, `FindByName`, **`HudSubsystem` LOADS the asset** (**UI13**); `EditorUICanvasDocument` + `UICanvasPanel` previewing the document through a **targeted** canvas submission (**UI14**); `UITreeEdit` = the whole tree as text, selection as an index path (**UI15**); **New UI…** + Save + `SetActivate`. 7 cases, **857 → 864 / 9266 → 9335**. All four MR2i routes: `panels` 19→20, `commands` 47→49, `resourceTypes` 14→15, `titleBar` 39→42. |
| **U5** | `628d1c7` | **THE MASK** (**UI16**): a `UIMask` container cuts everything under it, `mask.r * mask.a` — white shows, black hides. `QuadVertex` +`MaskUV`/`MaskIndex` (48→60 B, `-1` untouched), `Sprite.glsl`, `QuadMask` on the 3 Draw calls, `PlanQuadBatches` takes a second texture. `IUIFontProvider`→`IUIAssetProvider`, `UIImage::Texture` path, `UICanvas::Submit` split into a pure `BuildDrawList` (**UI17**). 9 cases, **868 → 876 / 9335 → 9387**. |
| **U5b** | `3674d22` | **9-slice + the safe area** (**UI20**), the two they split out of U5. `UIMargin` (four edges, one type, two units); `UI/UISlice.h` — pure `BuildSlicedQuads` (against a texture SIZE) + `ClipQuadsTo`; `UIImage::Border` with **no mode enum**; the fill re-expressed as a CLIP so **fill and slice compose**; `UIWidget::ResolveBounds` (one protected virtual) and `UISafeArea` on top of it. 12 cases, **882 → 894**, 9530 assertions. |
| **PIE fix** | `03610cd` | The viewport never picks during PIE (their U4 finding, restated): `PickGesture::Measure` gated on `PIE.IsEdit()` — one gate for the point and the marquee; a pick whose press began in Edit is dropped. Their first-named shape, the one-line one. **User-verified:** *"pie selection fixed"*. |
| **Panel designer** | `0559f7d` | Their *"Editing hud in ui panel more easier"*: click SELECTS (`PickAt`, ignoring `bHitTestable` on purpose), drag MOVES (`UnitsPerPixel`, one step), arrows NUDGE, an OUTLINE for the selection and the hover, the RESOLVED rect as numbers. `WorldToScreen` beside `ScreenToWorld`. **Their own `a1bce9a` starts working:** a group's range/step now FLOW to its children (`InheritMeta`). **894 → 895.** **User-verified** (*"eye gate good"*). |
| **U6** | `976b5b0` | **The deferred `OpenLevel` and the loading cover** (**UI21**). `IEngine::RequestOpenLevel` — flag-then-resolve at the top of `Loop`, `LevelLoadRequested`/`LevelLoadFinished` on the bus; the `UISubsystem`'s SECOND canvas with its root's visibility as the flag (read at draw time, so a mid-frame request covers that frame); a hidden root opens no pass; the cover is AUTHORED — `.opaaxproj` `loadingScreen` → `UI/Loading.opaaxui`. Dogfood: the pause menu's **Next Level** (Main ⇄ PhysicsTest). **895 → 896.** **User-verified:** *"next level works"*. |

**Settled in U11/U12, not the plan:** U11 held. U12's one find was in the PANEL, not the plan's
scope: wiring the preset's click as "its own step" exposed that the inspector's `IsAnyItemActive`
gesture had been opening on ANY press in the frame — so every U7 verb button (Delete, Duplicate,
▲▼) recorded a phantom "Edit Widget" behind its own step; two Ctrl+Z for one action. Sampling
"already active before the fields" fixed the preset and the six buttons at once. The image source
was factored ONCE for image and button, with the fields kept flat so `UIImage::Texture`'s file
key did not move. **Not proven by a smoke run:** the preset popup and the drops — theirs; the
frame path IS proven (the harness read the sliced icon's UVs back through the renderer's caches).

**Their report on U10** (*"All works. Only binding is not working correctly … its work on first map
loaded, then after do not change"*): a LEVEL SWAP. `OpenLevel` creates the new world before it
destroys the old, so the new HUD's `Add("Hud")` replaced the old's reader, and the old HUD's
`Shutdown` then removed BY NAME — the new HUD's source went with it, its pull resolved to nothing,
and the widget kept its authored `Jumps: {}` (which is the literal they then tried to fix by
changing the placeholder; `{}` is right, `{x}` replaces the whole text). Fix: `Add` returns a
`UIBindingHandle`, `Remove` takes it, a stale remove is a no-op — a test reproduces the exact
order. Proven in-game with a throwaway swap at frame 120 ([[L81]], removed): new HUD `.043`, old
shutdown `.055`, the new HUD's `'Jumps' bound` at `.059`, no warning. And their second ask, the
**cover floor**: `loadingScreenMinSeconds` in the project file — `Loading cover down after 3.00 s
(floor 3 s)` in the same run.

**Settled in U10, not the plan:** the plan held; the two things the run added were both about
seeing it. The table warns once on a failure, but a success was silent — which is [[L15]]'s
absence-of-error exactly — so the first resolve of each bound widget traces once, and the smoke
log now says what the text SHOWS. And the test's first `Update` had to precede the add, or the
root's own first build sat inside the number the case is about. **Not proven by a smoke run:**
the numbers changing on screen (`0 jump(s) counted` — nothing pressed Jump).

**Settled in U9, not the plan:** a container that was re-laid must ALWAYS re-arrange, even onto the
same rect — the plan's walk descended only on a CHANGED rect or a dirty subtree, and the first test
run showed `SetChildAlign` and `RemoveChild` moving nothing, because the stack's own fields are
inputs to the slots and its rect is not the only one. One condition, in the arranging branch only;
a plain parent keeps the same-rect short-circuit. The stats count a container's empty rebuild, as
they already did for a panel's — the numbers in the tests say so rather than pretending otherwise.
**Not proven by a smoke run:** the menu on screen (`opened 0 time(s)`) and the re-flow in the panel.
The asset was written by Python in the writer's key order; the editor's first Save canonicalizes
the float spellings.

**Settled in U8, not the plan:** nothing — the plan held. The one thing the test corrected was my own
reading of it: a widget's opacity counts for its OWN quads, not only its children's, which is what
"multiplies down" has to mean for a leaf image to fade at all. **Not proven by a smoke run:** the
fade — `opened 0 time(s)` in the boot log says the smoke never clicked the button.

**Settled in U7, not the plan:** the resize is done in PIXEL space, where the grip was hit, and only
the two corners cross into canvas units — no edge flip (the canvas is Y-up, the image is not), no
unit conversion of deltas, and `EditorRectGeometry`'s min-size clamp comes free. `FitRect` is the
one new piece of arithmetic, and it is the inverse the model always implied. The reference height
went into the INSPECTOR rather than the header: "nothing selected" is the canvas, and the
inspector's existing gesture (`IsAnyItemActive` → whole-tree step) brackets it with no second
mechanism — a header field would have opened that same gesture anyway and double-recorded.
**Not proven by a smoke run:** every gesture (the panel starts hidden and nothing opens a document
headless). **Found while wiring it:** Delete with the UI panel focused ran the LEVEL's delete —
the P8 V4 trap, one panel over.

**Settled in U6, not the plan:** the request had to publish IMMEDIATELY and the cover had to be a
flag read at DRAW time, or the frame that asked would never be covered — the tenant's submit has
already run when a world-tick trigger asks, and the UI tick's own button asks mid-route. "Submit
while loading" was the first design and it is exactly one frame late in both cases. The cover is
a second canvas rather than a last child, because a HUD hung later would draw over a last child.
A hidden root opening no pass is what makes an always-submitted canvas free. The cover is AUTHORED
through the project file, not code-built: UI13's rule, and the way their loading screen becomes
the progress screen without the UI changing. **Measured** (harness, removed): request at frame 60
from the WORLD tick → `cover up AFTER the request = true` → `LOADING COVER DRAWN — 11 draw
item(s)` that same frame → frame 61 opens PhysicsTest, cover down; one covered frame exactly.
**Not eye-verified:** the black flash itself, and a swap from PIE.

**Settled in U5b, not the plan:** the fill did not need a second geometry path — re-expressing it as
a CLIP over whatever was emitted made a filled 9-slice fall out for free and DELETED the inline
crop, with the pre-existing fill case proving the two are the same arithmetic. The safe area cost
**one virtual**: overriding the resolved bounds beats passing a different rect to children, because
then the widget's own bounds would lie to the hit-test and the preview. Insets are FRACTIONS —
that is the half of "adapt to wide" a reference height does not cover. **No renderer change at all**,
which is what their U5 split was for. **Not proven by a smoke run:** the picture — a harness
([[L81]], removed) logged `9 quad(s) over a 600x120 rect` and `safe area 1728x972 inside a
1920x1080 root`, which is geometry, not pixels. `Sandbox/Assets/UI/Panel9.png` is a 64x64 rounded
frame (radius = border = 16) generated to author with, `MaskDisc.png`'s role for U5.

**Settled in U5, not the plan:** masked TEXT cost **nothing** — `UIText` already goes through
`UIQuad`, so `Text2D` was not touched at all (the exploration's best find). A rect-only mask cannot
use id 0 (that means "no mask"), so it takes a real id resolving to white and all rect-only masks
share it. A mask resolves its texture at REBUILD, not at submit — submit is `const` and has no host
to ask. The **draw list** was worth building for its own sake: mask inheritance and draw order are
now assertable headless, where before they lived inside a function only a running frame reached.
**Their scope call held:** 9-slice + `UISafeArea` deferred to U5b, so the renderer change landed
alone. **Not proven by a smoke run:** the PIXELS — theirs.

**Settled in U4, not the plan:** the resource holds TEXT and re-parses per `BuildTree` — a widget is
a non-copyable node that knows its parent and canvas, so "hand out the tree" is either shared
mutable state or a per-type deep clone; re-reading IS the clone, with nothing to keep in step as
types are added. The panel needed a canvas submission that NAMES a target (UI14) — U2's broadcast
would have poured the game's canvases into the panel's framebuffer. The JSON is hand-written beside
the properties, as components already do; the reflection-driven serializer that would delete the
second list is a growth point and costs no format change. **Deploy needed no CMake edit** —
`Sandbox/Assets` copies whole, so `UI/` rides along (MR2i's deploy row, satisfied by what exists).
**Pre-existing, observed not fixed:** `FileIO::WriteAllText` is text-mode, so every engine-written
asset is CRLF locally and git normalizes to LF — `.opaaxui` behaves exactly like `.opaaxmap`.
**Not proven by a smoke run:** everything interactive in the panel — their eyes.
**User-verified** (*"Overall its working well"*), with ONE finding their eyes caught that the block
did not cause but did expose: **the viewport still PICKS during PIE**, so a click on a `UIButton`
also selects the entity behind it. The UI's consumption mask (**UI10**) cannot fix it — that mask
reaches input MAPPING, while the editor's pick is the ImGui side. Their two shapes, unpicked: a
gate on `PIE.IsEdit()` (~1 line, the viewport toolbar already reads it) or an editor config
toggle. Filed in `task/todo.md`.

**Settled in U3, not the plan:** the input mode chose the seam — bubbling (their steer) + Unreal's
three modes, the mask being IM6 with the UI on top, not a new mechanism. **The phantom-edge bug the
harness caught is the payload of the block:** masking a HELD key then unmasking it read a rising
edge, so a menu bound to the key that closed it reopened next frame. It was latent in IM6 (a higher
context popping mid-hold) with no caller until a mode toggled a mask. `bMaskSuppressed` gates
`bStarted`. → [[L95]]. The world is NOT paused while the menu is up (theirs to design). One PIE
harness ([[L81]]), removed; the reopen showed as `opened 2 time(s)` / `0 jumps`, the fix as
`opened 1 time` / `1 jump`. **User-verified:** *"eye gate good"*. **Focus deferred by them:** *"focus will be done with gamepad or when need for keyboard"* — built the day UIOnly must work with no mouse; `SetFocus` + key bubbling already in.

**Settled in U2, not in the seed:** the UI is NOT a view of its own — the runtime fallback keys on
an empty list, so it rides the world pass, opt-in (UI5) · layout happens at RENDER time, per
target (UI5) · the face is a string path through a provider the renderer implements (UI6) · a
`Rebuild` may re-arm, which is how an uploading atlas is waited for with no polling (UI3) ·
"score + health" became **Jumps + speed** — the player has no health and the gun does not fire;
both are real. **Not proven by a smoke run:** the picture itself, and a resize keeping the
corners — their eyes. `Sandbox.exe` opens `Main`, which has no mover, so its bar stays empty
there; `PhysicsTest` in PIE is where both move.

**Settled in U1, not in the seed:** a resolve that lands on the SAME rect stops propagation below
it (a corner-anchored child of a widening root costs one resolve, its subtree nothing) · the root
is `bHitTestable = false` and `UIPanel` defaults to pass-through, so "a widget was hit" can never
mean "the pointer is on screen" · visibility and hit-testability dirty nothing — they are read at
submit / hit-test time · `RemoveChild` hands the node back (undo will want it) and dirties nothing.

---

## The seed (2026-09-14, pre-branch)

> The block after parenting. Their first notes + four decisions, and the shape those decisions
> settle. Noesis is a **reference for shape only** (a UI core that knows nothing about the game;
> ViewModels bound onto it) — **no vendor**, their call. Unity's one-canvas simplicity and Unreal's
> editing are the two other references they named.
> Legacy salvage: `Legacy/World/IOverlayRenderSystem.h` (an overlay pass after the world pass, into
> the same target, no clear — the same shape as MV's named HUD growth point) and `WorldOld.h`'s
> persistent HUD entity — the problem this block solves at a different tier.

## Their decisions (2026-09-13/14)
1. **UI is DECOUPLED from World.** Not entities. Its own object model in its own module.
2. **No vendor.** Noesis is a good reference, nothing more.
3. **Sequence: the core UI system first; the consuming systems after** — the GameInstance tenant,
   any world-side hosting, the editor panel, HUD bindings are all *consumers* of the core.
4. **Async loading will come.** The loading screen is a cover today and must become a progress
   screen later **without the UI changing** — so the UI must tick and render with no world at all.
5. **A canvas is expensive because of resizing etc. — handle it correctly.** Invalidation-driven
   layout; resize as projection first; a number in the Stats panel that reads 0 when idle.

## The shape

### Module — `Engine/Source/UI/`, depends on Core + Renderer, never World
- Consumes `Renderer2D`, `Text2D::Layout` (**TX14**'s sink idiom — the layout is pure, the renderer
  is a sink), `CameraView`, `DebugDraw`. **Never** `World`, `WorldManager`, entt, a subsystem.
- **Testable headless** for the same reason **TX**'s walker is: rects, invalidation, hit-testing
  and text caching are pure; the GL half is one submit loop.
- The hosts own canvases and call three verbs: `Layout`, `Render(Renderer2D&)`, `HitTest(point)`.

### Object model — a widget tree, `CReflected`
- `UIWidget` — owned children, a `UIRect`, visibility, two dirty flags. **`CReflected`, so
  `DrawProperties` draws it unchanged**: the property system already serves non-entity documents
  (config, library entries, mover tunings), which is what makes the editor panel cheap.
- `UIRect` — Unity's RectTransform fields: anchor min/max (0..1 of the parent rect), pivot,
  anchored position, size delta. The one data structure everything rests on; the layout is a rect
  walk down the tree (`ComposeChain`'s shape with rects). *Unreal's slot-based model considered;
  Unity's is the one their notes cite and the one an author already knows.*
- `UICanvas` — the root. Reference height, the tree, the ordering. Owns nothing else.
- Leaves: `UIImage` (Simple / **Sliced** 9-slice / **Filled** — the health bar), `UIText`
  (wrap + alignment, **TX10**'s "no caller" items — the UI is the caller; caches its `TextQuad`s),
  `UIButton` (states + `OnClick` = `TMulticastDelegate`), `UIMask` (rect clip), `UIPanel`
  (container), `UISafeArea` (a container whose rect is the parent's minus the insets — Unreal's
  SafeZone; fewer concepts than a flag on every widget).
- **Rich text is `UIText` with runs**, later: a tag parser into runs, `<b>`/`<i>` = a family
  style-key resolve (**TX1** already does it), wrapping *across* runs is the hard part.

### Space — canvas units ARE reference pixels, and the pass is a `CameraView`
- The canvas pass renders through `CameraView{ centre, referenceHeight / 2 }`. **CAM2** — vertical
  half-extent, width follows the target's aspect — is Unity's CanvasScaler with Match = height,
  for free, chosen for the shmup for the same reason.
- The canvas's visible rect is `(referenceHeight × targetAspect) × referenceHeight`; the root
  widget's rect is that; anchors absorb the width. 21:9: top-right stays top-right, a centred menu
  stays centred, a 0..1-anchored background covers.
- **Resize is a projection change first.** Same aspect → nothing in canvas space moves, zero
  layouts. Aspect change → the root rect changes → one re-layout of what depends on it.
- Portrait mobile is the case Match = height gets wrong; a per-canvas match parameter is the
  escape hatch. Named, not built.

### Invalidation — their canvas-cost note, answered
- **Two dirty flags.** `Layout` (my rect changed → my subtree re-lays) and `Content` (my text /
  sprite changed → my cached quads rebuild). A change propagates **up** only through ancestors whose
  size depends on children (layout groups, later) and **stops at the first size-independent
  ancestor** — the usual "layout root".
- **Layout runs at most once per frame**, at the start of the UI tick, against the latest target
  size — a resize storm coalesces to one walk.
- **Submit is per-frame and cheap, and that is why Unity's cost does not transfer.** `Renderer2D` is
  immediate-mode and **F5** records the pass whole every frame anyway: there is no retained mesh to
  rebuild. What IS retained: computed rects and `UIText`'s `TextQuad`s. Per-frame cost is one
  submit per visible quad.
- **The instrument:** `Layouts` and `Text relayouts` per frame as **ST** rows — *0 when idle* is a
  number, not a claim ([[L59]]). A test asserts a same-aspect resize lays out nothing.

### Rendering — one pass per canvas, MV1's named growth point
- Submitted into **the same target as the game world**, `ELoadOp::Load` — its first caller, exactly
  as `ICommandBuffer.h` says. **F5** sorts within the pass, so draw order within the canvas is tree
  order (traversal index → order bits), independent of world layers.
- **Masks are a clip rect as a VERTEX ATTRIBUTE** — **F4d**'s idiom, a value not a pipeline.
  Unity's `Mask` is stencil, which would break F5's one-pipeline batch mid-pass; `RectMask2D` is
  the shape. Vertex 48 → 64 bytes. Nested masks intersect. Sprite-shaped masks named, not built.
- **Which targets get the pass is a design question for U2, not settled here:** the editor viewport's
  FBO in PIE and the backbuffer in `Sandbox.exe` — never over the editor chrome — and the Camera
  Preview (a game view that *"must look like the GAME"*, **MV1**) deliberately NOT. Two shapes:
  the world view's `RenderPassRequest` carries a `bDrawUI` the host sets, or the tenant asks which
  target framed the active world. Renderer must not learn about UI (direction: UI → Renderer).

### Input
- Pointer → canvas space via `ScreenToWorld` with the canvas view — **CAM2**'s one screen→world
  rule, no second one. Hit-test walks the tree top-most first; a point clipped by a mask misses.
- **"The UI captured the pointer" means A WIDGET WAS HIT — never "the pointer is inside the canvas"**,
  because the canvas covers the whole screen. [[L29]] is this exact bug in the editor
  (`WantCaptureMouse` answered ImGui's question, and one of its windows was the game).
- Consumption: the tenant ticks before input mapping resolves (**BO4d** order); the mechanism —
  a published capture flag the mapping subsystem honours, or a UI mapping context pushed only while
  a widget is hit (**IM6**) — is an open question, decided in U3.
- Keyboard/gamepad **focus and navigation** are the hidden cost of `UIButton`. Named, not built;
  gamepad is a WHEN (**IM11**).

### Hosting — the consumers, after the core
- **`UISubsystem`, a GameInstance tenant (GI2)** — owns the persistent canvases: loading, pause,
  the HUD. Outlives every world (**GI1**), reconstructed per PIE cycle (**GI6**) so nothing leaks,
  ticks and submits with zero worlds. **IM2** made the same tier call for the UI *input* context.
- **World-side hosting** — a world subsystem owning a level's own overlay. Trivial once the core
  exists (a canvas is an object anyone can own and submit); built when a level wants one.
- **Loading screen, today:** the tenant shows the loading canvas; `OpenLevel` becomes a REQUEST
  resolved at the next frame's start (**WS8**'s flag-then-resolve shape) so one frame draws the
  cover before the synchronous swap. **Later:** the same canvas stays up across frames with a
  progress binding. The UI does not change; only what is underneath it does.
- **Editor:** `.opaaxui` = the serialized tree (a resource type). A **UI panel** in the
  AnimationLibrary / FontFamily / prefab-panel family: tree on the left, the canvas rendered at
  reference resolution through **MV** (panel-owned FBO), reference frame + safe zone drawn with
  `DrawBounds`, `DrawProperties` on the right. Its own typed undo steps on the panel's stack
  (**PF10**/**UN2** shape). **MR2i**'s route table applies in full. *This is the "Unreal editing"
  note — UMG's designer from parts that exist.*

### MVVM — named, not built first
- The industry converged on MVVM (Noesis by construction, Unreal's UMG Viewmodel, Unity UI
  Toolkit bindings). Its value is data binding, which needs (1) read a property by name — **the
  reflection already does this** — and (2) change notification, which does not exist and which a
  per-frame pull replaces at HUD scale for free.
- So: a gameplay system writes the score into a `UIText` first. **`UIBinding`** — pull a named
  reflected property from any `CReflected` object the game owns into a widget property each frame
  — is the growth point, built the day three HUD elements do that by hand. Noesis's shape without
  its notification machinery. MVC's controller half is already **IM6**: a menu is a context that
  consumes.

## Proposed order — by what becomes visible (theirs to reorder)
| Step | Lands | Visible |
|---|---|---|
| **U1** | `UI/` module: `UIWidget`, `UIRect`, layout walk, invalidation, `UICanvas`; headless tests (anchors under an aspect change; dirty stops at a fixed-size ancestor; same-aspect resize = 0 layouts) | nothing — the tests |
| **U2** | the canvas pass (Load's first caller), `UIImage` Simple + Filled, `UIText` wrap + align with cached quads, the tenant with a hard-coded tree, ST rows | a score and a health bar over `PhysicsTest` in `Sandbox.exe`, on a resized window |
| **U3** | hit-test, `UIButton` states + `OnClick`, pointer capture vs mapping | a pause menu that consumes Jump |
| **U4** | `.opaaxui` + the UI panel + undo + MR2i routes | the HUD authored, not hard-coded |
| **U5** | `UIMask` (clip attribute), `UIImage` Sliced, `UISafeArea` | a stretched panel; a clipped list; a safe-zone frame |
| **U6** | deferred `OpenLevel` + the loading cover | a level swap with the canvas up throughout |

**Later, each with a caller before it is built:** rich text · layout groups (the row of lives) ·
canvas-group alpha (fade a menu) · `UIBinding` · focus/navigation · tweens · sprite-shaped masks ·
world-side hosting · progress across frames (async).

## Open questions (theirs)
- Pointer capture mechanism: a flag the mapping subsystem honours, or a hit-only UI context (U3).
- Which game views get the UI pass, and how the host says so (U2).
- Does the HUD prefab-like reuse exist — a `.opaaxui` nested in another — or is one file one canvas?
  (**PF13**'s answer was "nesting is the resolver's"; ask before U4.)
- `OpenLevel` deferral: a one-frame request is the smallest change; is it the one they want, or
  does the cover wait for async proper?
