# UI modifiers

A modifier is a small object a widget owns, with one job: a transition under the pointer, a tween, a
drop shadow, an aspect ratio. A widget's look and motion are built from them instead of fields on the
widget classes (Unity's components on a UI object, Unreal's render transform and styles). Modifiers
are objects like widgets: a `.opaaxui` saves them in their node, the UI panel lists and edits them,
and a game's own modifier class is one `OPAAX_REGISTER_OBJECT` line away.

| Modifier | Does |
|---|---|
| `UITransformModifier` | A pose that stays: moved, scaled, turned, faded, tinted |
| `UITransitionModifier` | A pose per interaction state (hovered, pressed...), eased to when it changes |
| `UITweenModifier` | Keyed poses over time: a pop-in, a fade, a pulse, a shake |
| `UIShadowModifier` | A drop shadow: a copy of the widget's quads under it |
| `UIOutlineModifier` | A rim: four copies, around |
| `UIAspectRatioModifier` | Keeps the rect at a width / height ratio |
| `UISizeLimitModifier` | Keeps the size between a minimum and a maximum |

## On a widget

A widget keeps its modifiers in the order they were added. In code:

```cpp
UIWidget* lPause = lUI.GetCanvas().Root().FindByName("Pause");
lPause->AddModifier<UITweenModifier>().SetTween("UI/Presets/PopIn.opaaxasset").SetPlayOn(EUITweenPlay::Show);
lPause->Show();   // plays it

if (UITweenModifier* lShake = lPause->FindModifier<UITweenModifier>(OPAAX_ID("Shake")))
{
    lShake->Play();
}
```

`AddModifier<T>()` returns the new modifier, whose setters chain; `RemoveModifier` gives it back (what
undo keeps), `MoveModifier` changes its place, `FindModifier<T>()` finds one by class, and by `Name`
too. A modifier knows its owner (`GetOwner()`), and `bEnabled` off makes it do nothing while it stays
in the file. Code that keeps a modifier it does not own holds a `TWeakObjectPtr`: it reads null once
its widget is gone.

In a `.opaaxui`, a node's `Modifiers` list holds each one's `Type` and fields, before its `Children`
([AssetFormats.md](AssetFormats.md#modifiers)).

## When they run

A modifier runs in the stage its job belongs to, and costs nothing in the others. An idle canvas does
no work: no modifier is visited on a frame where nothing changed.

| Stage | When | Hook |
|---|---|---|
| Attach | added, removed, its widget destroyed | `OnAttach`, `OnDetach` |
| Show and hide | `Show()`, `Hide()`, joining the game's canvas shown | `OnShow`, `OnHide` |
| State | hovered, pressed, focused or disabled changes | `OnStateChanged` |
| Tick | each frame, while it is awake | `Tick` (false sleeps; `WakeUp` wakes) |
| Layout | the widget's rect resolves | `ModifyBounds`, `ModifyDesiredSize` |
| Rebuild | the widget's quads are rebuilt | `ModifyQuads` |
| Pose | the poses are composed again | `GetPose` (pose modifiers) |
| Reset | the editor's preview stops | `OnReset` |

The UI ticks on the frame's own time, after the input is routed and before it is drawn: menus animate
while the game is paused or slowed down.

## Poses

A pose moves (`Translation`, canvas units), scales and turns (`Rotation`, degrees counter-clockwise) a
widget about a `Pivot` (0..1 of its rect), and fades (`Opacity`) and tints it (`Tint`). It applies to
the widget and everything under it, at draw time: the layout, the rect and every saved field stay as
they are, so a pose never makes a document change.

Poses compose in list order, each about its own pivot, on top of the earlier ones; opacities and tints
multiply. A hover at 1.05 and a press at 0.95 give 0.9975: both show, and nothing has to decide which
wins.

`bMovesHitArea` says whether clicks follow a pose. On, the widget is hit where it is drawn (a widget
that slides in); off, it keeps being hit at its layout rect, so a button that shrinks when pressed
cannot cancel its own press at the edge. It is on for a transform and a tween, off for a transition.
Pointer events reach each widget in its own layout space, so a widget compares them with its rect
whatever its pose.

`ui.list` answers what is drawn: `drawn` (the box that covers the posed widget), `scale`, `rotation`,
`alpha`.

## The interaction state

Every widget has one, the strongest first: `Disabled` (it takes no input: a disabled button),
`Pressed` (held on it, and the pointer still over it), `Hovered`, `Focused`, `Normal`. Hovered,
pressed and focused reach the parents: a panel is hovered while a widget in it is. `ui.list` answers
it as `interaction`.

## Transitions

`UITransitionModifier` holds a pose for `Hovered`, `Pressed`, `Focused` and `Disabled` (`Normal` is
the rest pose), a `Duration` and an `Ease`. When the state changes it eases from the pose it shows to
the new state's: interrupted halfway, it goes back from there, without a jump. A button added
disabled starts at its `Disabled` pose.

A `Preset` names a **UI Transition** asset (New Asset > UI > UI Transition) holding the same values:
when it loads, its values are used instead of the modifier's own. Many buttons share one preset, and
saving it reaches every one of them, playing or not.

## Tweens

A **UI Tween** asset holds `Keys`, each a `Time`, the `Ease` from the previous key to it and a `Pose`;
`Loop` (`Once`, `Loop`, `PingPong`) and `bHoldEnd` (a `Once` tween keeps its last pose at the end, or
goes back to rest). Keys are sorted by time when edited. Sampling is stateless: the pose of a moment is
the same whatever the frame rate.

`UITweenModifier` plays one, or its own `Keys` when it names no `Tween`:

- `Play()` starts it from its first key, shown at once; `Stop()` puts the widget back at rest.
- `Delay` holds the first key's pose before it moves: rows sharing one tween follow one by one.
- `PlayOn`: `Manual` (only when asked), `Show` (when the widget is shown), `Hide` (when it is hidden).
- `OnFinished` fires once when a `Once` tween ends.

`ui.tween {widget, tween}` plays one by name in a running game.

## Show and hide

`Hide()` plays the widget's `Hide` tweens, then turns `bVisible` off when the last one ends (at once
without one). While it hides, the widget is still drawn and not hit. Its children go with it; their
own hide tweens do not play. A `Hide` tween that loops never ends: the widget stays hiding.

`Show()` turns `bVisible` on, cancels a hide under way, drops the pose a hide left (a faded-out
widget is seen again) and plays the `Show` tweens of the widget and the shown widgets under it. A
widget joining the game's canvas shown plays them too.

Writing `bVisible` stays the instant way: no tween, no hiding. A widget hidden by a fade and shown
again by writing `bVisible` keeps its faded pose; use `Show()`.

`ui.show` and `ui.hide` do it in a running game; `ui.list` answers `hiding`.

## Quad effects

`UIShadowModifier` (`Offset`, `Color`) and `UIOutlineModifier` (`Distance`, `Color`) copy the widget's
quads after it rebuilds them and draw the copies under the originals. A copy keeps its quad's texture
and cut: a text's shadow has its letters, a half-filled bar's shadow is half too. Its alpha is the
effect's times the quad's. The outline's four copies leave the diagonal corners bare, as Unity's does;
there is no blur.

## Layout

`UIAspectRatioModifier` keeps the rect at `Ratio` (width / height) after it resolves, with Unity's
four modes: `WidthControlsHeight`, `HeightControlsWidth`, `FitInParent` (as large as fits in the
parent) and `EnvelopeParent` (as small as covers it). `UISizeLimitModifier` keeps the size between
`MinSize` and `MaxSize`, each axis alone, 0 for none. Both change the rect about its pivot, and the
size the widget asks of a stack or a scroll box, which then leave room for it. The parent modes keep
the rect's own pivot: stretch the widget over its parent to centre it.

## In the UI panel

Under the selected widget's fields, **Modifiers** lists its modifiers: each with its class and name,
up, down and remove, and its fields; **Add Modifier** offers every registered modifier class, a game's
too. Each verb is one undo step, and a field edit closes with the inspector's gesture, as a widget's
does. The automation commands are `ui.modifier.add`, `list`, `set`, `remove` and `move`
([Automation.md](Automation.md#commands)).

**Play**, above the preview, runs the canvas as the game would: ticked, its Show tweens played,
the pointer over the image hovering and pressing it; selecting and dragging pause meanwhile. Each
tween modifier has a **Play tween** button. **Stop** puts everything back at rest
(`UICanvas::ResetRuntime`): nothing saved changes, so a play never makes the document dirty. Without
Play, the editor's canvas is not ticked and nothing animates in it by itself.
**Preview state**, beside it, shows the selected widget in a state without the pointer: Normal,
Hovered, Pressed, Focused or Disabled (a button's art for that state; its transitions and state tweens
while Play runs). **Live** gives it back to the pointer; Stop clears it, and nothing saved changes.
`ui.preview.play`, `ui.preview.hover` and `ui.preview.state` drive them.

## Writing one

A modifier is a `UIModifier` subclass, or a `UIPoseModifier` one for a pose, with `OPAAX_OBJECT`, its
fields in `OPAAX_PROPERTIES` (a default on each) and one `OPAAX_REGISTER_OBJECT` line: the file, the UI
panel and automation take it with no other code. Override the hooks of the stages it works in, and
say when its result changes:

- a pose: answer `GetPose`, call `InvalidatePose()` when it changes, `WakeUp()` to be ticked;
- quads: `ModifyQuads`, and `InvalidateOwnerContent()` when a field changes;
- the rect: `ModifyBounds` and `ModifyDesiredSize`, and `InvalidateOwnerLayout()`;
- a hide that takes time: return true from `OnHide`, call `FinishHide()` when done.

The editor writes fields directly, then calls `PostEditChange()`: override it to invalidate what the
fields feed. Keep the logic in pure functions beside the class (`SampleTweenKeys`, `AspectSize`,
`LimitSize`), with unit tests.

## Not here yet

- A timeline animating several widgets together.
- Animating a widget's own fields (a colour, a fill, a size) rather than a pose.
- A turned mask clipping to its turned shape: it clips by the box around it. A non-uniform scale under
  a turned parent loses its shear.
- Springs, blurred shadows, tooltips, dragging, and keyboard or gamepad focus moving between widgets.
- Poses on game time (a world-space HUD slowing with the game).
