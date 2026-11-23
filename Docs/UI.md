# The game UI

A game's UI (the HUD, menus, a debug panel) is one canvas, owned by the game's `UISubsystem` from start
to quit, across levels. Gameplay reaches it through `GetContext().UI`. The canvas file format is in
[AssetFormats.md](AssetFormats.md#ui-canvases-opaaxui), what widgets draw in [UIStyles.md](UIStyles.md),
and how they move in [UIModifiers.md](UIModifiers.md).

## Mounting a screen

A screen is a `.opaaxui` canvas mounted in a layer:

```cpp
UISubsystem& lUI = *GetContext().UI;

UIWidget* lHud   = lUI.MountAsset("UI/Hud.opaaxui");                             // Hud: the default
UIWidget* lPause = lUI.MountAsset("UI/Pause.opaaxui", nullptr, UILayerMenu());
UIWidget* lStats = lUI.MountAsset("UI/Stats.opaaxui", nullptr, OpaaxTag("Debug"));

UIButton* lResume = static_cast<UIButton*>(lPause->FindByName("Resume"));

lUI.GetCanvas().Root().RemoveChild(*lPause);   // closed: the tree is gone
```

`MountAsset(path, parent, layer)` loads the canvas and adds its tree under `parent` (the canvas's root
when null) and returns the tree's root, which the canvas owns. Under the root, the tree draws in
`layer`, Hud when none is given.

## Layers

The trees under the root draw by layer, a higher layer on top, whatever the order they were mounted in:
a pause menu mounted after a debug panel still draws under it. Within one layer the tree mounted last is
on top. `BringToFront()` puts a tree over the others of its layer, never over a higher one;
`SetLayer(tag)` moves it to another layer, last in it. Neither takes the tree out: what is hovered,
pressed or focused stays.

The layers are a list in the project's `Engine.config`, `UI.Layers`, edited in the editor's Config
panel (Engine, UI, Layers) like the audio buses. A new project has:

| Layer | Order | For |
|---|---|---|
| `Hud` | 0 | What is on screen while playing |
| `Menu` | 100 | Menus: pause, options |
| `Debug` | 500 | Debug panels, over the menus |
| `Loading` | 900 | A game's own loading or fade screens, over everything of its own |

```json
"UI": { "Layers": [ { "Name": "Hud", "Order": 0 }, { "Name": "Menu", "Order": 100 },
                    { "Name": "Menu.Pause", "Order": 10 }, { "Name": "Popup", "Order": 300 },
                    { "Name": "Debug", "Order": 500 }, { "Name": "Loading", "Order": 900 } ] }
```

A higher order draws on top; equal orders are level, so their trees draw in mount order. A listed list
replaces the default one: a game adds its own layers (`Popup` above), renames or moves them. An edit
applies at once, while the game runs: the trees move. A player's settings can set one by its name
(`{"UI": {"Layers": {"Menu": {"Order": 600}}}}`), as for a bus.

Code names a layer by its tag: `UILayerHud()`, `UILayerMenu()`, `UILayerDebug()` and
`UILayerLoading()` for the default ones, `OpaaxTag("Popup")` for a game's own. A tag interns its text:
keep one used every frame in a member or a static.

### Sub-layers

A dotted name is a sub-layer: `Menu.Pause` draws in Menu's place, ordered among Menu's own trees, which
are at 0. At 10 it is over them, at -10 under them. Whatever its order, a sub-layer never leaves its
parent's place: `Menu.Pause` at 9999 still draws under Debug, and moves with Menu when Menu's order
changes. A layer is at most four names deep.

### Names not listed

A tree mounted in a layer the list does not name draws in its nearest listed parent (`Menu.Options` in
Menu), and the log says so once. A tree with no layer (added with `Root().AddChild`) is at 0, where Hud
is by default. What is wrong with the list (a layer without a name, a name listed twice, a sub-layer
whose parent is not listed) is logged when the list is read.

## The pointer across layers

The canvas finds what is under the pointer as one tree, the topmost first:

- Where trees overlap, the higher layer takes the pointer. The widgets under it hear nothing: they are
  not hovered, pressed or clicked.
- A widget that is hit but handles nothing (an image) still covers what is under it, and the click then
  goes to the game. `bHitTestable` off lets the pointer through it.
- A tree that hides (`Hide()` with Hide tweens) is drawn until its tweens end, but takes no pointer: a
  click passes to the layers under it.
- One widget is hovered, one pressed and one focused, for the whole canvas.

## Over the game's canvas

The engine's own canvases draw over all of the game's layers: the loading screen, while a level loads,
and the dev console. A game's `Loading` layer is for its own screens, inside its canvas.

## Checking it from a script

- `ui.layers` answers the layers, bottom to top, with the trees drawn in each.
- `ui.list` answers each widget's `layer`, in draw order: the top layer last.
- `ui.click` and `ui.hover` answer `hit`: the widget on top where they point, which is another one when
  a higher layer covers the widget.
- `config.set` and `settings.set` change the layers while the game runs.

The commands are in [Automation.md](Automation.md#commands).
