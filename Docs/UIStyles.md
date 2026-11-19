# UI styles

What a widget draws in its rect, and how a button changes its art with its state. An image draws one
**brush**; a button draws a brush per interaction state, its own or a shared **UI Button Style** asset
(Unreal's `FButtonStyle`, Unity's sprite swap and colour tint); a widget inside a button can follow the
button's state and swap its art with it. How they move (transitions, tweens) and sound is in
[UIModifiers.md](UIModifiers.md); the file format is in [AssetFormats.md](AssetFormats.md#ui-canvases-opaaxui).

## The brush

`UIBrush` (`UI/UIBrush.h`) is what fills a rect:

| Field | Default | |
|---|---|---|
| `Tint` | white | multiplies the image; the colour of a brush with no image |
| `Animation` | none | an `AnimationClip`, played from its first step; wins over the two below |
| `Sheet`, `Frame` | none, -1 | a sprite sheet frame, -1 the sheet's default; wins over `Texture` |
| `Texture` | none | a texture (a `.png` dropped on it works) |
| `Border` | zero | nine-slice border in texture pixels; zero takes the sheet frame's own |
| `BorderScale` | 1 | canvas units a border pixel takes: sizes the corners, not what they show |

A brush that names no image is a plain `Tint` rect. A brush whose image is not loaded yet draws
nothing and tries again on the next frame. A texture handed over by code (`UIImage::SetTexture`, a
render target) wins over all of them. Slicing and tiling are the same as before brushes: see
[AssetFormats.md](AssetFormats.md#sliced-art).

`UIImage` has one, `Brush`, and its setters write it: `SetColor` (the tint), `SetTexturePath`,
`SetSheetFrame`, `SetAnimation`, `SetBorder`, `SetBorderScale`.

## Animated images

`Animation` names the clip sprites play (New Asset > Animation > Animation Clip). Each step shows its
frame of the clip's sheet, found by name, or its own texture. An image plays its clip while it is shown
on a canvas that ticks: a running game, or the UI panel's **Play**. It rebuilds only when the step
changes, and an image with no clip costs nothing per frame.

- A `Once` clip stops on its last step and the image stops ticking; `Loop` and `PingPong` go on.
- A hidden image pauses and goes on from the same step when shown. `SetAnimation` (or the preview's
  Stop) starts it again from its first step.
- In the editor, without Play, the canvas is not ticked: it shows the first step.
- A clip that does not load shows the brush's `Sheet` or `Texture`; a step naming a frame its sheet
  lacks shows the sheet's default frame. Each says so once in the log (`UIBrush`).

```cpp
UIImage* lSpinner = static_cast<UIImage*>(lUI.GetCanvas().Root().FindByName("Spinner"));
lSpinner->SetAnimation("Anims/Cycle.opaaxasset");   // from its first step
```

## A button's brushes

`UIButton::Brushes` (a `UIStateBrushes`, `UI/UIStateBrushes.h`) holds a brush for each interaction
state, `Normal`, `Hovered`, `Pressed`, `Focused` and `Disabled`, then `FadeDuration` and `FadeEase`.

- **Fallback.** A state brush that names no image shows Normal's image and border in its own tint. One
  piece of art with a colour per state therefore needs one image (a new button is that: no image, a
  grey per state).
- **On a state change** the image swaps at once, the tint fades from the one on screen to the new
  state's over `FadeDuration` (0 jumps; an interrupted fade starts from the tint shown), and the new
  state's clip starts from its first step: a hover glow plays each time the pointer comes over.
- **Focused** has its art for when focus can move between widgets; nothing focuses a button yet.

## Button styles

A **UI Button Style** (`UIButtonStyle`, New Asset > UI > UI Button Style) is one `Brushes`, shared by
every button that names it in its `Style`. A button with a style draws the style's brushes and not its
own, which show again when the style does not load (said once in the log). Saving a style reaches every
button showing it, in the editor's canvas and in a running game.

A state brush's `Tint` defaults to a new button's grey: art meant to show as it is drawn needs a white
tint in each state that names it.

```cpp
UIButton* lPlay = static_cast<UIButton*>(lMenu->FindByName("Play"));
lPlay->SetStyle("UI/Styles/Pixel.opaaxasset");
```

A button made in code takes its own `Brushes` as set before it joins a canvas (`lButton->Brushes.Hovered.
Animation.Path = "Anims/Glow.opaaxasset";`); later, `SetStyle` is the way to change its art.

TestWorld has three: `UI/Styles/Pixel` (the four frames of `UI/SlicedButtons`), `Animated` (a looping
hover clip) and `Icon` (an icon's art per state).

## Following a button

Every widget has `StateSource`: `Self` (the default) or `Button`, the state of the nearest button above
it, whatever lies between (a stack, a panel). A follower:

- draws its button's state: an image with a `Style` swaps its art with the button (its own `Brush` is
  not used while the style loads; there is no tint fade, it swaps at once);
- plays its transitions and state tweens on the button's changes, and its sounds on its hover and
  click ([UIModifiers.md](UIModifiers.md#the-interaction-state));
- does not count its own pointer state; hit testing is unchanged (the pointer over the icon hovers the
  button through its parents). A button inside a button keeps its own followers.

An icon with a style and a label with a transition (a `Pressed` pose 2 units down, a `Hovered` tint),
both following, is Unreal's pressed padding and foreground colour. `UI/Brushes.opaaxui` in TestWorld
has one (`FollowButton`).

```cpp
UIImage* lIcon = static_cast<UIImage*>(lPlay->FindByName("Icon"));
lIcon->SetStateSource(EUIStateSource::Button);
lIcon->SetStyle("UI/Styles/Icon.opaaxasset");
```

## In the editor

The inspector draws a brush as a folding struct, and a button's `Brushes` as one fold per state. All of
a brush's fields show; their tooltips say which wins. In the UI panel:

- **Play** ticks the canvas: clips play, the pointer over the image hovers and presses buttons, and
  sounds play when the editor has audio. **Stop** puts everything back at rest; nothing saved changes.
- **Preview state** shows the selected widget, and the widgets following it, in a state without the
  pointer: Normal, Hovered, Pressed, Focused or Disabled. Its art and its transition pose show at once,
  its state tweens while Play runs. **Live** gives it back to the pointer; Stop clears it.

## A game's widget that draws a brush

A widget class of a game draws a brush as images and buttons do: a `UIBrush` field in its
`OPAAX_PROPERTIES` (the inspector and the canvas file get it), `DrawBrush` in `Rebuild`, and a tick only
while its clip can change.

```cpp
class UIBadge final : public UIWidget
{
    OPAAX_OBJECT(UIBadge, Opaax::UIWidget)
public:
    UIBrush Art;
    OPAAX_PROPERTIES(UIBadge, OPAAX_PROP(Art))

    OpaaxString DescribeBrush() const override { return Opaax::DescribeBrush(Art, false, m_Clip); }

protected:
    void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override
    {
        if (DrawBrush(InContext, GetBounds(), Art, nullptr, m_Seconds, OutQuads, &m_Clip) == EUIBrushDraw::NotReady)
        {
            InvalidateContent();   // not loaded yet: try again
        }
        if (m_Clip.Clip != nullptr && !m_Clip.bFinished && !IsAwake())
        {
            WakeUp();              // a clip that can still change: ticked while shown
        }
    }

    bool Tick(const float InSeconds) override
    {
        m_Seconds += InSeconds;
        UICanvas* const   lCanvas = GetCanvas();
        const UIBrushClip lClip   = SampleBrushClip(lCanvas != nullptr ? lCanvas->GetAssets() : nullptr, Art, m_Seconds);
        if (lClip.Clip == nullptr) { return false; }
        if (lClip.Step != m_Clip.Step) { InvalidateContent(); }   // rebuilt only when the step changes
        return !lClip.bFinished;                                   // false sleeps
    }

    void OnReset() override { m_Seconds = 0.f; m_Clip = {}; InvalidateContent(); }

private:
    float       m_Seconds = 0.f;
    UIBrushClip m_Clip;
};
```

One `OPAAX_REGISTER_OBJECT(UIBadge)` line and it is offered by the UI panel. A widget that draws per
state overrides `OnInteractionStateChanged` and draws `UIStateBrushes::For(GetInteractionState())`. A
game's own button derives from `UIButtonBase` and does the same: it keeps the state and fires the
events.

`DescribeBrush` is what `ui.list` and `ui.widget.list` answer as `brush` (`clip#step`, `sheet#frame`, a
texture path, `runtime`, `none`), so a script checks a widget's art without a screenshot.

## Checking it from a script

- `ui.list` (a game) and `ui.widget.list` (the editor) answer each image's and button's `brush` and
  `interaction`.
- In the editor: `ui.widget.set` edits the selected widget (`{"Style": "UI/Styles/Pixel.opaaxasset"}`),
  `ui.preview.play` and `ui.preview.hover` run the preview, `ui.preview.state` sets a widget's Preview
  state. See [Automation.md](Automation.md).

## Not here yet

- An intro and an exit clip per state (hover-in, then a hover loop; hover-out).
- Crossfading the image between states (it swaps; only the tint fades).
- A project-wide default style (Godot's theme, Unreal's default style set).
- Styles for texts (font, size, colour per state) and for other widgets (a scroll bar, a toggle or a
  slider when they exist).
- Focus moving between widgets (keys, gamepad): the `Focused` art waits for it.
- Boxes drawn without a texture (rounded corners, borders: Godot's `StyleBoxFlat`).
- A style editor showing the five states side by side.
