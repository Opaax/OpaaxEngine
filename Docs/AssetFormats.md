# Asset file formats

Every Opaax asset other than images, sounds and fonts is a UTF-8 JSON file. The editor writes them,
but a person or an agent can write them too: a key that is missing keeps its default, so a short file
is enough. Open a level written by hand in the editor and use **File > Resave Level (All Maps)** (or
`level.save` with `all`) to put it in the engine's own form, so later saves change only what changed.

Paths inside assets are relative to the project's `Assets` folder (`Textures/Hero.png`), or name
engine content with `/Engine/` (`/Engine/Textures/T_Square_256_White.png`).

| Extension | What | In the editor |
|---|---|---|
| `.opaaxproj` | The project: name, startup level | (project root) |
| `.opaaxlevel` | A level: the maps it loads | File > New/Open/Save Level |
| `.opaaxmap` | A map: entities and placed prefabs | Hierarchy, Inspector |
| `.opaaxprefab` | A prefab, a nested prefab or a variant | Create Prefab, Prefab panel |
| `.opaaxui` | A UI canvas | UI editor |
| `.opaaxaction`, `.opaaxinputmap` | Input actions, and the keys that drive them | Input panels |
| `.opaaxsheet` | A texture cut into named frames | Sprite Sheet panel |
| `.opaaxclip`, `.opaaxanim` | An animation clip, a library of clips | Animation panels |
| `.opaaxdata` | A data asset (a `Material2D`, a game's own structs) | Data Asset panel |
| `.opaaxmover`, `.opaaxmovemode` | A Mover's modes, and one mode's tuning | Mover panels |
| `.opaaxfont` | A font family: faces by style | Font Family panel |
| `Configs/*.config` | Engine, renderer and editor settings | Config panel |

Images are `.png`, sounds `.wav`, `.mp3` or `.flac`, fonts `.ttf`: put them under `Assets`.

## Project (`.opaaxproj`)

```json
{
    "name": "Tetris",
    "startupLevel": "Levels/Main.opaaxlevel",
    "loadingScreen": "UI/Loading.opaaxui",
    "loadingScreenMinSeconds": 0.5,
    "uiReferenceHeight": 1080
}
```

`MakeOpaax.sh` (or `OpaaxCreator`) creates a project with this file, its folders and its CMake target.

## Levels and maps (`.opaaxlevel`, `.opaaxmap`)

A level lists its maps; the persistent one is where new entities go.

```json
{ "name": "Main", "maps": ["Maps/Main.opaaxmap"], "persistentMap": "Maps/Main.opaaxmap", "version": 1 }
```

A map holds entities. Each has a `guid` (32 hex digits, unique in the project: references and prefab
overrides use it), a `name`, the map it belongs to (`ownerMap`, the map's `mapId`), an optional
`parent` (another entity's guid; absent for a root) and its components by type name. A behaviour is
saved like a component, under its class name, with its `OPAAX_PROPERTIES` fields.

```json
{
    "mapId": "Main",
    "version": 4,
    "entities": [
        {
            "guid": "5d0f6c1e2b3a4f5e8d7c6b5a49382716",
            "name": "Player",
            "ownerMap": "Main",
            "components": {
                "Transform": { "Position": { "x": 0.0, "y": -100.0 }, "Rotation": 0.0, "Scale": { "x": 1.0, "y": 1.0 } },
                "Sprite": { "Texture": "Textures/Hero.png", "Size": { "x": 64.0, "y": 64.0 } },
                "Collider": { "Shape": "Box", "Size": { "x": 64.0, "y": 64.0 }, "Channel": "Pawn" },
                "Rigidbody": { "Type": "Dynamic" },
                "PlayerController": { "JumpSpeed": 600.0 }
            }
        },
        {
            "guid": "8a1b2c3d4e5f60718293a4b5c6d7e8f9",
            "name": "Camera",
            "ownerMap": "Main",
            "parent": "5d0f6c1e2b3a4f5e8d7c6b5a49382716",
            "components": { "Camera": { "OrthoSize": 300.0, "Priority": 0 } }
        }
    ]
}
```

The engine's components: `Transform`, `Sprite`, `Quad`, `Text`, `Camera`, `Environment`, `Light2D`,
`ShadowCaster2D`, `SpriteAnimator`, `Collider`, `Rigidbody`, `Mover`, `AudioSource`, `AudioListener`
(and `PrefabInstance`, written by the editor on placed prefabs). Their fields are the ones the
Inspector shows; `entity.get` (Docs/Automation.md) prints an entity with every field and its value.
Enums are saved by name (`"Type": "Dynamic"`), colours as `{ "x", "y", "z", "w" }` (red, green, blue,
alpha from 0 to 1).

### Placed prefabs

A map places prefabs in `prefabInstances`. Each placement names the prefab, has its own `instanceId`
(a guid), and overrides what differs from the prefab, keyed by the prefab entity's guid:

```json
"prefabInstances": [
    {
        "prefab": "Prefabs/Coin.opaaxprefab",
        "instanceId": "0c1d2e3f405162738495a6b7c8d9eafb",
        "overrides": {
            "<the Coin entity's guid in Coin.opaaxprefab>": {
                "name": "BonusCoin",
                "components": { "Transform": { "Position": { "x": 200.0, "y": 50.0 } }, "Coin": { "Value": 10 } }
            }
        }
    }
]
```

An override is a JSON merge patch of that entity: the fields given change, the others follow the
prefab (so a later change to the prefab still reaches them). `null` for a component removes it;
`null` for a whole entity removes it from the placement; `"parent"` re-parents it (`""` detaches).
The placed entities get guids derived from the placement and the prefab's guids: place prefabs with
the editor (`prefab.place`) rather than by hand when their entities must be referenced.

## Prefabs (`.opaaxprefab`)

The same entities as a map, without `ownerMap`, version 3. A prefab can place other prefabs
(`prefabInstances`, as in a map): a nested prefab. A file with no entities and one placement is a
variant of that prefab, its overrides being what the variant changes.

```json
{
    "version": 3,
    "entities": [
        {
            "guid": "d1e2f3a4b5c6d7e8f90a1b2c3d4e5f60",
            "name": "Coin",
            "components": {
                "Transform": { "Position": { "x": 0.0, "y": 0.0 } },
                "Sprite": { "Texture": "Textures/Coin.png", "Size": { "x": 32.0, "y": 32.0 } },
                "Collider": { "Shape": "Circle", "Radius": 16.0, "Mode": "Overlap", "Channel": "Trigger" },
                "Coin": { "Value": 1 }
            }
        }
    ]
}
```

Behaviours spawn prefabs with `Spawn("Prefabs/Coin.opaaxprefab", Position)`.

## UI canvases (`.opaaxui`)

A tree of widgets, designed at `ReferenceHeight` (the canvas scales to the screen's height):

```json
{
    "Version": 1,
    "ReferenceHeight": 1080.0,
    "Root": {
        "Type": "UIPanel", "Name": "Hud", "bHitTestable": false,
        "Rect": { "AnchorMin": { "x": 0.0, "y": 0.0 }, "AnchorMax": { "x": 1.0, "y": 1.0 } },
        "Children": [
            {
                "Type": "UIText", "Name": "Score", "Text": "Score: {}", "Binding": "Hud.Score",
                "Font": "/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf", "Size": 48.0,
                "Rect": { "AnchorMin": { "x": 0.0, "y": 1.0 }, "AnchorMax": { "x": 0.0, "y": 1.0 },
                          "Pivot": { "x": 0.0, "y": 1.0 }, "AnchoredPosition": { "x": 40.0, "y": -40.0 },
                          "SizeDelta": { "x": 400.0, "y": 60.0 } }
            }
        ]
    }
}
```

Every widget has `Type`, `Name`, `Rect`, `Opacity`, `bVisible`, `bHitTestable` and `Children`. A
`Rect` anchors the widget in its parent (`AnchorMin`/`AnchorMax`, fractions; equal = a point, apart =
stretch), places its `Pivot` at `AnchoredPosition` (canvas units, Y up) and gives its `SizeDelta`.

| Type | Its fields |
|---|---|
| `UIPanel` | none: a container |
| `UIImage` | `Texture` or `Sheet` + `Frame`, `Color`, `Border` (nine-slice), `Fill`, `FillAmount`, `FillBinding` |
| `UIText` | `Text`, `Binding`, `Font`, `Size`, `Color`, `HAlign`, `VAlign`, `bWrap`, `LineHeightScale`, `bKerning` |
| `UIButton` | `Normal`, `Hovered`, `Pressed`, `Disabled` colours, `bEnabled`, `Texture` or `Sheet` + `Frame` |
| `UIStack` | `Axis`, `Spacing`, `Padding`, `ChildAlign`, `bFitContent`: lays its children out in a row or a column |
| `UIMask` | `Texture` (a shape; empty = the rect), `bShowMaskGraphic`: clips its children |
| `UISafeArea` | `Insets`: keeps its children inside the screen's safe part |

A binding (`"Hud.Score"`) reads a property of a source the game registers on the canvas
(`Bindings().Add(OPAAX_ID("Hud"), MakeBindingReader(*this))`); with a binding, `Text` is the format and `{}`
the value. A game mounts a canvas with `GetContext().UI->MountAsset("UI/Hud.opaaxui")` and finds a
widget with `FindByName`.

## Input (`.opaaxaction`, `.opaaxinputmap`)

An action is what the game reads (`GetAction`, `BindAction`); a map binds keys to actions.

```json
{ "Name": "Move", "ValueType": "Axis2D", "HoldSeconds": 0.5, "Description": "Walk and climb",
  "Modifiers": [ { "Type": "Normalize" } ] }
```

```json
{
    "Priority": 0,
    "Mappings": [
        { "Action": "Input/Move.opaaxaction", "Key": "D", "Modifiers": [], "bConsume": true },
        { "Action": "Input/Move.opaaxaction", "Key": "A", "Modifiers": [ { "Type": "Negate" } ], "bConsume": true },
        { "Action": "Input/Move.opaaxaction", "Key": "W", "Modifiers": [ { "Type": "Swizzle" } ], "bConsume": true },
        { "Action": "Input/Move.opaaxaction", "Key": "S", "Modifiers": [ { "Type": "Swizzle" }, { "Type": "Negate" } ], "bConsume": true },
        { "Action": "Input/Jump.opaaxaction", "Key": "Space", "Modifiers": [], "bConsume": true }
    ]
}
```

`ValueType` is `Bool`, `Axis1D` or `Axis2D`. Modifiers apply in order: `Negate`, `Swizzle` (x and y
swapped), `DeadZone` (`DeadZoneLower`, `DeadZoneUpper`), `Scalar` (`Scale`), `Normalize`. Keys are
named as in `EKeyCode` (`A`...`Z`, `Space`, `Escape`, `Left`, `Mouse_Left`...). A game adds a map with
`GetContext().Actions->AddContextAsset(OPAAX_ID("Gameplay"), "Input/Gameplay.opaaxinputmap")`.

## Sprites and animation (`.opaaxsheet`, `.opaaxclip`, `.opaaxanim`)

A sheet cuts a texture into named frames (pixels, from the top-left corner); `Grid` only records how
the editor sliced it:

```json
{
    "Texture": "Textures/Hero.png",
    "DefaultFrame": 0,
    "Frames": [
        { "Name": "Idle", "Offset": { "x": 0.0, "y": 0.0 }, "Size": { "x": 64.0, "y": 64.0 } },
        { "Name": "Run_0", "Offset": { "x": 64.0, "y": 0.0 }, "Size": { "x": 64.0, "y": 64.0 } }
    ]
}
```

A clip plays frames of a sheet (or whole textures) in order; `PlayMode` is `Once`, `Loop` or
`PingPong`, and `Hold` keeps a step for that many ticks:

```json
{ "Sheet": "Sprites/Hero.opaaxsheet", "Fps": 8.0, "PlayMode": "Loop",
  "Steps": [ { "Frame": "Run_0", "Hold": 1 }, { "Frame": "Run_1", "Hold": 1 } ] }
```

A library names clips for a `SpriteAnimator` to switch between:
`{ "DefaultClip": "Run", "Entries": [ { "Name": "Run", "Clip": "Anims/Run.opaaxclip" } ] }`.

## Data assets (`.opaaxdata`)

A registered struct (`OPAAX_REGISTER_DATA_ASSET`) by type name, with its fields:

```json
{ "Type": "Material2D", "Data": { "bLit": true, "NormalMap": "Textures/Bricks_N.png",
                                  "EmissiveColor": { "x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0 }, "EmissiveStrength": 1.0 } }
```

A sprite uses a material through its `Material` field.

## Movement (`.opaaxmover`, `.opaaxmovemode`)

A mover lists the modes a `Mover` component can switch between; each mode is its own tuning file:

```json
{ "DefaultMode": "Ground", "Entries": [ { "Name": "Ground", "ModeAsset": "Movers/Hero_Ground.opaaxmovemode" },
                                        { "Name": "Fly",    "ModeAsset": "Movers/Hero_Fly.opaaxmovemode" } ] }
```

```json
{ "Mode": "GroundMove", "MaxSpeed": 420.0, "Acceleration": 12.0, "GroundDeceleration": 9.0,
  "AirSteer": 0.35, "JumpSpeed": 620.0, "GravityScale": 1.0, "MaxSlopeAngleDeg": 50.0,
  "MinSpeed": 10.0, "StopSpeed": 100.0 }
```

`Mode` names a registered mode (`GroundMove`, `FlyMove`, or a game's `OPAAX_REGISTER_MOVER_MODE`).
Gameplay drives a mover through its component: `Input.MoveDir`, `Input.bJump`, `PendingMode`.

## Fonts (`.opaaxfont`)

A family maps styles to font files, so text asks for "Bold Italic" rather than a file:
`{ "Entries": [ { "Face": "Fonts/Title-Bold.ttf", "Style": { "Weight": "Bold", "Slant": "Normal",
"Width": "Normal", "Subset": "Latin" } } ] }`.

## Configs (`Configs/*.config`)

`Engine.config` holds the window, rendering backend, physics (gravity, world bounds) and audio
settings; `Renderer.config` the renderer's; `EditorImgui.config` the editor's look. They are edited in
the editor's Config panel; a missing key keeps its default.
