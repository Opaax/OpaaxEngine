# ⑦-B — The game lifecycle, the GameInstance tier, and Input Mapping

**CLOSED 2026-09-08, user-verified.** Six commits `40c2d05` → `4209c24`.
**686 / 8212 / 7 → 723 / 8423 / 7.** Durable: **BO4d**, new **§GI** (GI1–GI6), new **§IM**
(IM1–IM11). Lessons: [[L83]]–[[L86]].

Counts after: `panels=18, drawers=11, resourceTypes=13, titleBar=39, commands=38`,
formats **13 over 18 extensions**, `gameInstanceSubsystems=1`.

---

## What it is

Gameplay reads `OPAAX_ID("Jump")`, not `EKeyCode::Space`. `PlayerControlSubsystem` adds a mapping
context and binds four actions; **there is no `EKeyCode` in the game module at all**, and rebinding
is a file edit rather than a recompile.

```
GetInputSystem->AddContextAsset(OPAAX_ID("Gameplay"), "Input/Gameplay.opaaxinputmap");
GetInputSystem->Bind(OPAAX_ID("Jump"), EInputTrigger::Started, this, &T::OnJump);
...
GetInputSystem->UnbindAll(this);   // in Shutdown. The lifetime contract.
```

---

## The six slices

| | |
|---|---|
| **B0** `40c2d05` | The game lifecycle (`StartGame`/`EndGame`) + the GameInstance tier. 25 files. |
| **B1** `cfa8aae` | Value model, modifiers, evaluator, the bind surface. Pure, headless. 11 files. |
| **B2** `1e13811` | `.opaaxaction` + `.opaaxinputmap`, and the consumer rewritten. 38 files. |
| **B3** `e84e802` | The editor half — two documents, two panels, ops, undoables. 24 files. |
| **fix** `5cef691` | Ctrl+S saved the map from four document editors; Ctrl+Z could not say why it did nothing. |
| **B4** `4209c24` | The UI half: a Menu context that consumes. 8 files. |

---

## THEIR FOUR DECISIONS, and every one changed the design

1. **"We need a better 'start engine / game' structure. The game instance should be create before
   the world."** My plan hung the session off `CreateWorld` as a side effect. Theirs is a symmetric
   `StartGame`/`EndGame` bracket — no refcount, teardown order free from reverse registration
   order, and `PlayInEditor::Stop` got **shorter**. → **BO4d**, [[L83]].
2. **"One asset per action, like Unreal"** + *"we will do it too"*. I had justified an action-SET
   asset from **AN8**'s absence — a limitation they intend to remove. → **IM9**, [[L84]].
3. **"Gameplay BINDS, it does not poll"** (`Bind(this, trigger, callback)`). This cost **nothing**:
   `TMulticastDelegate` already had `AddMember` → handle, snapshot-based `Broadcast`, and
   `RemoveAll(owner)`. It also made the asset SMALLER — the binding names the trigger, so a key
   mapping carries no trigger and no hold time. → **IM4**, **IM7**.
4. **"We can manage that by checking the Type of the world."** Right that the handler needed
   gating; the mechanism could not be MODE, because both worlds in a level swap are `Play`. The
   discriminator is `World::IsActive()`, which cost almost nothing because `OnActive`/`OnDesactive`
   were already called at every transition and only logged. → **IM8**.

---

## The defects worth remembering

- **Action-level modifiers were missing, and my own test name lied about it.** A B1 case called
  *"the diagonal is not faster"* asserted `(1, 1)` — magnitude 1.41. Modifiers were per-BINDING, and
  a WASD composite is four bindings each contributing a unit vector, so `Normalize` on any of them
  changes nothing: **only the sum can be clamped**. Found while authoring the asset that would have
  shown it in play; confirmed by the user afterwards (*"diagonal speed seems to be good too"*).
  → **IM5**.
- **Three instruments that were mute in a second configuration** — the second host, the second
  object, the second context. → [[L85]], now a mechanical check rather than a hazard to remember.
- **A dispatch ladder forgotten four times**, silently saving the MAP from four document editors
  (two of them pre-existing from ⑦-A). → [[L86]].
- **Float defaults made exactly representable** (`0.5`, `0.25`, `1.0`): `0.4f` serializes as
  `0.4000000059604645`, so a hand-authored asset could never match the writer and every one would
  open with a phantom `*`. **Measured**, not reasoned — `dirty on open: false` for both.

---

## Techniques that paid

- **The X-macro list** (`InputKeyCodeList.h`, 143 entries generated from `InputCodes.h`) gives
  `ToString` and `TEnumValues<EKeyCode>` from one source — `CollisionChannel.h`'s shape. It then
  paid **twice more**, because `TPropertyDrawer` specialises for any `CEnumWithValues`: the key
  field and the modifier type became dropdowns with zero panel code.
- **Hoisting the one piece of real logic out of the untestable shell.** `MakeComposite2D` is a pure
  header-only function reached by `OpaaxTests` through the `EditorGizmo` route, and the test asserts
  it **through the evaluator** (D is +X, W is +Y) rather than by reading modifier names back.
- **Throwaway harnesses** ([[L81]]) four times, each removed: the PIE bracket, two PIE cycles, both
  editor panels' ImGui, and the Menu consumption cycle. They found the `1 world(s)` gate defect, the
  `11 of 2` count defect, and measured the dirty-on-open claim.

---

## Named, NOT built

- **`New Asset` as a `SetCreate` route (B3b), ~2h.** Priced and deferred **by them** at close:
  *"New assets things not yet."* It remains the one item that enters **AN8**'s reserved factory
  ground. Do not build it as a one-off menu entry.
- **Gamepad.** Their *"do not forget"*. Needs a `glfwGetGamepadState` poll at the **IN2** boundary
  plus connect/disconnect. **No format change** — `EKeyCode` already reserves 10000+, the binding is
  authorable today, and the loader refuses it loudly (**IM11**) rather than failing silently.
- **Key CAPTURE in the editor** (press a key to bind). B3 ships a filtered dropdown. Blocked by
  **IN8**: in Edit mode the route is closed and `InputManager` sees nothing, so capture must come
  from ImGui — needing an `ImGuiKey → EKeyCode` table and a query on `IEditorWidgets` to stay behind
  the Qt seam (**MR2d**).
- Chorded triggers · Unreal's `Ongoing`/`Canceled` phases · runtime rebinding persistence ·
  per-player devices (`LocalPlayer` — their split-screen note; GameInstance is the right host until
  then).
- **A searchable key combo.** 143 entries in a plain dropdown works and is unwieldy.
