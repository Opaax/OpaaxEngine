# Engine Sequence — the program after ④b

> **Set by the user, 2026-08-24.** Eight blocks, in this order. **Each block gets its OWN plan** when it
> starts — this file is the sequence and the decisions already taken, not a plan for any one of them.
>
> The user is doing on-the-fly improvements before ① starts. Nothing here is in flight.
>
> **The three "objections" recorded below are INPUTS to those per-block plans**, not blockers now. Each
> one is a decision that costs nothing today and saves a rewrite later; they are written down so the
> block's plan does not have to re-derive them.

---

## ① CAMERA — ✅ LANDED 2026-08-26 (core only)

> **Shipped:** `CameraView` on the World + the two math helpers · `CameraComponent` · `CameraManager`
> (an **ENGINE** subsystem, Play worlds only) · `EditorCamera` (pan/zoom, Edit worlds only) ·
> `Legacy/{Renderer,Editor}/Camera/` deleted. Contract: ARCHITECTURE.md **CAM1–CAM7**.
> **Cut by the user before the build:** follow, shake, the entity drag-drop and `TPropertyDrawer<Guid>`.
> **Two decisions below did NOT survive contact, both corrected by the user or by the cut** — see the
> struck-through notes inline. Plan: `.claude/plans/camera.md`.

## ① CAMERA — as originally planned

Extensible base ("Cinemachine"-like setup) · view target · editor camera · switch between Edit / PIE /
game · **D5 input**.

`RendererManager` hard-codes a centred ortho at `RendererManager.cpp:145` and nothing produces a
`RenderView`. `RenderView` (`Renderer/RenderView.h` — `Matrix44F ViewProjection` + `Viewport`) is the
seam and already exists.

**Decision — build it as DATA + one world subsystem, not the legacy polymorphic pair.**
`Legacy/Renderer/Camera/` holds `ICamera` + `ICameraController` + Follow/Shake controllers. That shape
fights **I8** (no component base class) and **D7** (per-entity behavior is authoring data + a world
subsystem, never a polymorphic component). Cinemachine's own model maps onto this engine directly —
vcams are data, the brain is one ticker:

- `CameraComponent` — ortho size, priority, viewport rect. Data.
- `CameraFollowComponent` — target Guid, damping, deadzone. Data. (Shake is a third.)
- ~~`CameraSubsystem` (world subsystem) — picks the active camera, ticks behaviors, produces the
  `RenderView`.~~ **WRONG, corrected by the user 2026-08-26: it is an ENGINE subsystem
  (`CameraManager`).** The world-subsystem shape rested entirely on *"ticks behaviors"*, and with
  follow cut there is no per-world behavior and no per-world state — the resolve is a pure function of
  the active world. The data-not-polymorphism half of this decision held exactly as written; only the
  tier was wrong. Also shipped: **no `priority`, no `viewport rect`** — a field nothing reads is a spec
  (**X5**), so several cameras means the first wins and it warns. See **CAM3**.

Inspector for free (**I15**), saves into the map for free (**I8**), PIE-clones for free (**WM6**).
Adding a "Cinemachine" later is a new *component*, never a new base class.

**Ship ONE behavior.** Priority/blending is a growth point named-not-built — [[L23]]'s "an API with no
caller", and blending is where this design gets expensive.

**The switch is TWO cases, not three.** The producer is chosen by `EWorldMode`, immutable at
construction (**BO4a**): `Edit` → editor camera, `Play` → the world's active `CameraComponent`.
`Sandbox.exe` **is** the Play path — there is no third branch.

**Name the fallback.** A Play world with **no camera entity** keeps today's centred ortho and warns
once — **BO4c**'s rule one level down. Never a black frame; the first map authored without a camera
must not read as "the renderer broke".

**Input trap, already paid for once.** Editor camera pan/zoom gates on **viewport hovered/focused**,
never on ImGui's `WantCaptureMouse` — the viewport IS an ImGui image, so that flag is always true over
it ([[L29]]).

---

## ② EDITOR ENHANCEMENTS

Screen→world mouse · viewport click-select · focus-selected command · create entity · multiselect.

Screen→world is the prerequisite for the rest. `Editor.md` D5 names it exactly: needs the viewport rect
*and* the camera — which is why this block follows ①.

**Create-entity also closes a real bug:** `SandboxPanel` spawns entities with no `OwnerMap`, so they
land in `(runtime - not saved)` and no Save can ever write them (**WM2**).

**Multiselect splits in two.** Multi-select in the Hierarchy is cheap; multi-**edit** in the Inspector
(which draws one entity today) is its own job. Do not let the second ride in silently.

**Objection → carry into this block's plan: route every hit-test through ONE entity-world-AABB helper.**
Picking, focus-selected, gizmo placement and rubber-band select all ask the same question. Written
directly against `SpriteComponent`/`DummyComponent` `Position`/`Size`, all four get rewritten in ③.
Through one helper, ③ changes one function body.
> **Still entirely ②'s, and ① paid none of it.** ①'s plan expected follow to need an
> entity-world-position helper first; cutting follow removed that, so nothing in the tree reads an
> entity's world position yet. What ① *did* leave for ② is the other half: `ScreenToWorld`
> (`Renderer/CameraView.h`, **CAM2**) is exported and is the one screen→world rule, so picking starts
> from a pixel→world answer that already exists and is tested.

---

## ③ TRANSFORM & GIZMO

Base component · remove old vars · gizmos.

Transform deliberately arrives **with** the gizmo, not before it: alone it is an invisible refactor,
together it is a demoable feature. Code cost is near zero — `World::Each<A,B>` already exists
(`World.h:215`), so the sprite pass becomes a join.

**"Remove old vars" is ONE map migration, not three — and it is FOUR components now, not three.**
Fold together:
- `SpriteComponent.Position` → Transform. Rotation arrives here too (`DrawSprite` already takes one;
  `SpriteComponent.h:26-29` refuses to own it until a transform exists).
- `DummyComponent.Position` → Transform.
- **`CameraComponent.Position` → Transform** (added by ①, **CAM5**). Debt taken deliberately: the
  alternative was building Transform in ①, which this sequence refused on demoability grounds. It
  carries the same standing comment the other two do. `Main.opaaxmap` already has one authored.
- **`DummyComponent` itself** — still iterated BY NAME at `RendererManager.cpp:163`; its own header says
  it was never meant to survive bring-up.

`Sandbox/Assets/Main.opaaxmap` is rewritten byte-exact (**MP6**). `_WITH_DEFAULT` (**I8**) means old maps
*load*, but they load at origin — tolerance is not migration.

Gizmos were explicitly out of scope in `Editor.md` §7 *until* picking + input routing exist. ① and ②
are that gate lifting.

---

## ④ STATS & DEBUG

A real stats graph: CPU · frame time · GPU.

**Split it — GPU timing is not a panel, it is an RHI addition.** Timestamp queries on `IRHIDevice` /
`ICommandBuffer` do not exist. CPU + frame time is a panel; GPU is its own small slice.

**Put the RENDERER counters in this block**, not in ⑥: draw calls, quads, flushes/frame, texture-slot
pressure. Those four numbers are what turn ⑥'s sort bug from a code comment into something observable
before it bites — which is the whole reason this block sits ahead of ⑥.

---

## ⑤ UNDO / REDO

**Objection → the cost of this placement, stated honestly.** By the time this starts, create-entity,
multiselect, gizmo drag and Inspector edit all exist as un-undoable mutation points. Four retrofits.

**Keep the position anyway** — a gizmo drag is the hard case (continuous, needs coalescing), and a
command layer designed without it is a shape that gets rewritten.

**But hold ONE named mutation choke point through ② and ③.** Then undo is "wrap the choke point"
instead of a twenty-call-site hunt. `Editor.md` §7 already promises this ("panels route edits through
identifiable mutation points to keep the door open") — it simply has to be *true* by the time this
block starts. A discipline that can only be asserted from memory is not one ([[L17]]).

Multi-select interacts with this: undoing an edit across N entities is one command, not N.

---

## ⑥ RENDER GAPS

- **Global frame sort — first.** `Renderer2D.cpp:216` says it outright: *"Orders the CURRENT batch
  only."* Past `MAX_QUADS = 1000` or 16 texture slots the frame splits, and the painter's algorithm
  breaks **across** flushes — a later batch draws on top regardless of layer/order. Exactly the
  1000-bullet shmup. Fix is record frame-wide → sort globally → emit batches.
- **Sprite sheets + UV authoring, then animation.** `DrawSprite` already takes UVs; nothing authors
  them. A sheet is its own resource type + component + a world subsystem advancing frames — the
  resource stack from ④/④b is what makes this cheap now.
- **Text rendering.** The DebugDraw/HUD consumer. `Old/milestone/M5_Text_Rendering.md` is salvage.
- **Multi-view.** Nearly free if `RenderView` stays the seam — one `BeginPass` per view, as
  `RenderView.h` already promises.

---

## ⑦ GAMEPLAY GATE

- **Physics 2D.** box2d is already vendored. Biggest single milestone, the platformer gate;
  `Old/milestone/M9_Physics_2D.md` + the Mover backlog is a deep salvage bank.
- **Input action maps.** Before demo gameplay is written, or every game file couples to physical
  keycodes. `Editor.md` D5 already calls action maps a game-layer concept.
- **Prefabs / entity templates.** No way to spawn an authored entity today. **MP1–MP10** is ~90% of the
  machinery — a prefab is a one-entity map with re-minted Guids.
- **Tilemap.** Platformer content gate; without it every tile is an entity and a draw call.
- **Time scale + pause**, **object pooling.** Both small, both high-juice.

---

## ⑧ BUILT BUT NOT CONNECTED

- **Audio.** `AudioManager` exists as a shell and is registered **nowhere** (`Engine.cpp:83-87`);
  `Engine/Source/Audio/` (miniaudio backend) is un-globbed and would not compile. Sandbox's
  `WaveResource` is already the hook. Landing it: glob, register, give the resource a payload.
- **Resource hot-reload.** Top of the user's own `Docs/TODO.txt`. Edit a PNG, see it in-game — the
  natural continuation of ④/④b.
- **Cooked resource pipeline / packs**, **Vulkan device**, **handle-pool DOD RHI.** The Render list's
  long tail. Real, but none of them unblock a game.

---

## Still owed, independent of this sequence

- **④b's interactive gate was never run** — four eyeball checks in `.claude/task/todo.md`. Build, tests
  and log lines only.
- Branch is **32 ahead, unpushed**.
- M5's three interactive gates were exercised in passing, never ticked one-by-one.
- Map capture is **O(maps × entities)** at ~12 µs/entity Release (**MP5**); `CloneWorld` pays it every
  PIE Start.
- **No test covers `DrawSprite` or the texture cache** — both need a GL context.
