# Objects and assets

Every asset type of the engine (a texture, a font face, a material, a sprite sheet, an animation clip,
a mover, a font family, an input action) and of a game is a class derived from `Opaax::Asset`, itself
an `Opaax::Object`. A
class with fields is enough: the file format, the loading, the sharing, the editor window, undo,
automation and the conversion of old files come from the base. It is the idea of Unreal's `UObject`
and `UDataAsset`, and of Godot's `Resource`. Subsystems, UI widgets, behaviours, the game instance,
worlds and levels are objects too ([Subsystems](#subsystems), [UI widgets](#ui-widgets),
[Behaviours](#behaviours), [The game, worlds and levels](#the-game-worlds-and-levels)).

Components are plain structs, not objects. Levels, maps, prefabs and UI canvases keep their own
formats ([AssetFormats.md](AssetFormats.md)).

## An asset class

```cpp
#include "Engine/Registries/AutoRegistration.h"
#include "Resources/Asset/Asset.h"

class EnemyStats final : public Opaax::Asset
{
    OPAAX_OBJECT(EnemyStats, Opaax::Asset)

public:
    Opaax::Int32 MaxHealth = 30;
    float        Speed     = 120.f;

    OPAAX_PROPERTIES(EnemyStats,
                     OPAAX_PROP(MaxHealth).SetRange(1.f, 10000.f),
                     OPAAX_PROP(Speed).SetRange(0.f, 2000.f).SetTooltip("World units per second."))
};

OPAAX_REGISTER_OBJECT(EnemyStats);
```

- `OPAAX_OBJECT(Class, Parent)` gives the class its `ObjectClass`: its name in files (the C++ name
  without namespace), its parent, a default object holding every field's default.
- `OPAAX_PROPERTIES` lists the fields that are saved, drawn in the editor and set by automation:
  numbers, `bool`, strings, names (`OpaaxStringID`), enums declared with `OPAAX_ENUM_VALUES` (saved by
  name), vectors and colours, another asset (`TAssetRef<Material2D>`, `TAssetRef<Texture2D>` for an
  image), a file of another kind (`TResourcePath<PrefabResource>`), and lists of structs that have
  `OPAAX_PROPERTIES` and `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`. Give every field a default: a
  file written before a field existed still loads.
- `OPAAX_REGISTER_OBJECT(Class)` is the only registration. After renaming a class,
  `OPAAX_REGISTER_OBJECT_ALIAS("OldName", "NewName")` keeps its old files loading.

`Sandbox/Source/Sandbox/Data/EnemyStats.h` is this class; the engine's are next to the code that uses
them (`Renderer/Materials/Material2D.h`, `Renderer/Textures/SpriteSheet.h`, `Movement/Assets`...).

## The file

One format for every class, `.opaaxasset`: the class's name, its properties by field name, and the
format's version.

```json
{ "Class": "EnemyStats", "Properties": { "MaxHealth": 50, "Speed": 90.0 }, "Version": 1 }
```

A missing property keeps its default. A value of the wrong type, or an unknown class, makes the file
fail to load, and the log names the field. The asset is named after its file
(`Data/Grunt.opaaxasset` is `Grunt`).

## Using an asset from code

A component, a behaviour or another asset names one with a field:

```cpp
Opaax::TAssetRef<EnemyStats> Stats;   // saved as its path: "Data/Grunt.opaaxasset"
```

and loads it with `LoadAsset`, which shares one copy between every user:

```cpp
const Opaax::TAssetHandle<EnemyStats> lStats = Opaax::LoadAsset(Resources, Paths, Stats);
if (const EnemyStats* lData = lStats.Get())   // null when the file is missing or holds another class
{
    Speed = lData->Speed;
}
```

Keep the handle, not the pointer: saving the file in the editor reloads it in place, and the handle
reads the new values. `GetOrDefault()` answers the class's defaults when the file is missing, for a
class whose defaults make sense (a `Material2D` is lit and flat); an abstract class has none. A path
written in code works too:
`LoadAsset<EnemyStats>(Resources, Paths, Opaax::OpaaxString("Data/Grunt.opaaxasset"))`.

## Hooks

An asset can react to what happens to it by overriding:

| Hook | When | Example |
|---|---|---|
| `PostLoad()` | Its properties were just read (a file, an undo). It may run on a loading thread: touch nothing but the object. | derived values |
| `PostEditChange()` | An editor or a script changed it. Fix or refuse the values here; the window records the fixed ones. | `Mover`, `AnimationLibrary`: a repeated name is cleared; `SpriteSheet`: a default past the last frame goes back to 0 |
| `IsPropertyVisible(name)` | The editor asks whether to show a field | a field that only applies to some values of another |

`Resources/Asset/NamedAssetEntries.h` has the lookups and fix-ups of a list that gives other assets
short names (a Mover's modes, a library's clips), for a class of the same kind.

## Assets with code

An asset class can carry behaviour as well as data. The move modes do: `MoveMode` is an abstract asset
class whose subclasses are ways of moving, each with its tuning as properties and its step as a
`const` `Tick` (`Movement/Modes/GroundMoveMode.h`). One asset serves every entity that moves that way,
so a mode keeps no state of its own; the entity's lives on its component. A new way of moving is a new
subclass and one `OPAAX_REGISTER_OBJECT` line, and its window shows exactly its fields.

When a class is split into subclasses like this, a line in `AssetMigration::GetClassSplits` (the old
class, the property that named the kind, the suffix that makes the subclass's name) lets the files
written before load as their subclass, and `--upgrade-assets` rewrites them.

## Assets read from files of their own

A class can be read from files of its own format instead of `.opaaxasset` files, as Godot reads a
`.png` as a texture: the file is the asset, with nothing beside it. The class lists the extensions
after `OPAAX_OBJECT` and reads one file in `LoadSource`:

```cpp
class EnemyWave final : public Opaax::Asset
{
    OPAAX_OBJECT(EnemyWave, Opaax::Asset)
    OPAAX_ASSET_SOURCE(".wave")

public:
    Opaax::TDynArray<Opaax::Uint8> Bytes;

    bool LoadSource(const char* InAbsPath) override   // may run on a loading thread
    {
        return Opaax::FileIO::ReadAllBytes(Opaax::OpaaxString(InAbsPath), Bytes);
    }
};
OPAAX_REGISTER_OBJECT(EnemyWave);
```

The engine's images and fonts are such classes: `Texture2D` reads `.png`, `.jpg`, `.tga` and `.bmp`
files (`Renderer/Textures/Texture2D.h`) and `FontFace` reads `.ttf` and `.otf` files
(`Renderer/Text/FontFace.h`); a sprite names an image with `TAssetRef<Texture2D>`. A field names
one like any asset (`TAssetRef<EnemyWave>`, saved as `"Waves/First.wave"`), and `LoadAsset` loads and
shares it. Two more hooks serve such a class: `InitializeOnMainThread()` runs on
the main thread once the file is read (a texture hands its pixels to the GPU there), and
`GetResourceSize()` says what it holds, for the resource statistics. One extension belongs to one
class: registering a second class for `.wave` is refused. In the editor the file shows as an asset of
its class and opens in a window on double-click, read-only (its file is the source, never rewritten);
New Asset does not offer the class, since the file comes from elsewhere.

## In the editor

- **Make one:** right-click a folder in the Resource Browser > **New Asset** > its category > its
  class. Scripts use `asset.new`.
- **Edit one:** double-click it. Each asset opens in a window of its own, docked or floating; several
  opened together cascade. The window's header has the class, Save, Undo and Redo, and its title an
  asterisk while it has unsaved changes. Ctrl+S, Ctrl+Z and Ctrl+Y act on the focused window. Each
  asset has its own undo history: a change is one step once the widget being dragged or typed in is
  released.
- **How a class looks:** with no registration, it is offered in New Asset itself (below the
  categories), named from its class name (`EnemyStats` is "Enemy Stats"), and its window lists its
  properties. An editor module can give it a label, a glyph or an icon, a New Asset category, and an
  editor of its own:

```cpp
void MyGameEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    InRegistrar.AssetTypes().Register<EnemyStats>()
        .SetLabel(Opaax::OpaaxString("Enemy"))
        .SetCategory(Opaax::OpaaxString("Gameplay"))
        .SetGlyph(Opaax::OpaaxString("[E]"))
        .SetEditor<EnemyStatsEditor>();   // optional
}
```

### A custom editor

A class whose fields need more than a list (a picture, a matrix, a preview) gets an `IAssetEditor`:
built from the `EditorContext`, it draws below the window's header and edits the asset directly.

```cpp
class EnemyStatsEditor final : public Opaax::Editor::IAssetEditor
{
public:
    explicit EnemyStatsEditor(Opaax::Editor::EditorContext& InContext) : m_Context(InContext) {}

    void DrawContents(Opaax::Editor::AssetDocument& InDocument) override
    {
        EnemyStats* lStats = Opaax::Cast<EnemyStats>(InDocument.GetAsset());
        // ImGui and the property drawers (DrawProperty, DrawProperties): change lStats's fields.
    }

private:
    Opaax::Editor::EditorContext& m_Context;
};
```

The window records each change as one undo step when no widget is active any more, after running the
asset's `PostEditChange`: the editor writes no undo code. Something edited by dragging (a frame over
an image) must keep an ImGui item active for the whole drag, an `InvisibleButton` over the area,
or each frame becomes a step. The engine's own are in `Editor/Source/Editor/Assets/Editors`: the
mapping context (a key table, the 2D composite buttons), the clip (a playing preview, the frame
picker), the font family (a style matrix, a text sample) and the sprite sheet (frames over the image).
Keep what they compute in pure functions beside them (`AnimationClipEditing.h`), so tests reach it
without a window.

## Automation

`asset.classes`, `asset.new`, `asset.open`, `asset.get`, `asset.set`, `asset.undo`, `asset.redo`,
`asset.save`, `asset.close` and `asset.list` do in a script what the windows do, for every class
([Automation.md](Automation.md)). `asset.set` goes through the same undo and the same
`PostEditChange` as a person's edit; a value of the wrong type is refused.

## Old files

Before this model, each asset type had its own extension (`.opaaxsheet`, `.opaaxclip`...). Starting
the editor with `--upgrade-assets` converts every such file of the project and of the engine's content
into an `.opaaxasset` of the same name, rewrites the paths that named it in levels, maps, prefabs,
canvases, assets and configs, and rewrites in its subclass every file of a split class (a
`MoveMode` naming its `Mode`). A path written in game code is not rewritten: one that still names an
old file loads the converted one and logs a warning saying where to change it. When a class of yours
replaces a format of your own, add one line to the table in `AssetMigration::GetLegacyFormats`.

## Subsystems

The engine's services, a game's and a world's derive from `Opaax::Subsystem`, an abstract object
class, through one of three bases:

| Base | Lives | Made by |
|---|---|---|
| `EngineSubsystem` | from engine start to stop (resources, worlds, rendering, audio, input) | the engine, in code |
| `GameInstanceSubsystem` | one game, across level changes | every game: one of each registered class |
| `WorldSubsystem` | one world | every world: one of each registered class its `ShouldCreate` accepts |

A game or world subsystem registers with `OPAAX_REGISTER_OBJECT` like any class: its owner finds it in
the class registry, asks the class's default object whether to make one (`ShouldCreate`) and where it
ticks (`GetTickOrder`), makes them all, gives each its context (`GetContext()`), then starts them in
order. The constructor makes nothing, since the default object is built with it outside any owner.
How to write one: [Customizing.md](Customizing.md#a-subsystem).

## UI widgets

Every widget of a canvas is an object of a `UIWidget` subclass (`UIPanel`, `UIImage`, `UIText`, a
game's own). Its class is the `Type` a `.opaaxui` file saves, and its properties are the fields the file
holds and the UI panel draws: a new widget is a class with `OPAAX_OBJECT`, `OPAAX_PROPERTIES` and one
`OPAAX_REGISTER_OBJECT` line, and the canvas file, the editor's Add menu and its inspector take it with no
other code. `UIWidget::Create(name, classes)` makes one by class name. A parent owns its children through
`TStrongObjectPtr`; code that keeps a widget it does not own can hold a `TWeakObjectPtr`, which reads
null once the widget is gone.

## Behaviours

A behaviour (gameplay code on an entity) is an object of a `Behaviour` subclass, declared with
`OPAAX_OBJECT(Coin, Opaax::Behaviour)` and registered with `OPAAX_REGISTER_BEHAVIOUR(Coin)`, which
registers its class as well as its place on entities. It lives in its entity's storage like a
component and never moves there, so another object can keep a `TWeakObjectPtr<Coin>` to it: it reads
null once the behaviour or its entity is gone. A behaviour is saved with its entity, its properties
under its class name; it is not copied, the Play copy of a world being made from the saved fields.

## The game, worlds and levels

A game session is an object of the `GameInstance` class, or of a game's own subclass named in the
project file (`"gameInstanceClass": "MyGame"`, [AssetFormats.md](AssetFormats.md#project-opaaxproj)).
It is made when the game starts, before its first world, and lives across level changes until the
game ends; `OnStart` and `OnEnd` are its hooks, and its subsystems are made whatever its class. A name
that is not a registered `GameInstance` class logs an error and the game runs a plain `GameInstance`.

A `World` is an object too: its name is its object name (a level's world is named after the level),
and its handle is its identity. `WorldManager` makes and owns the worlds, and each world owns its
`Level`, the object that mounts its maps. Neither is made by class name, so neither has fields to save:
what a level holds is its file.

## Lifetime and the garbage collector to come

Every object has a slot in one table (`ObjectTable`), with a generation that changes when the slot is
reused. Handles resolve through it, so a handle to a destroyed object reads null instead of pointing
at freed memory. Three pointer kinds, as in Unreal:

| Pointer | Does |
|---|---|
| `TStrongObjectPtr<T>` | Owns: the object lives while one exists. `NewObject<T>()` returns one. |
| `TObjectPtr<T>` | A reference held in a property: reads null once the object is gone, and `ObjectClass::CollectReferences` finds it. |
| `TWeakObjectPtr<T>` | Never keeps the object alive; reads null once it is gone. |

Assets loaded from files are owned by the resource manager, which releases one that nothing holds;
subsystems by their owner's collection, widgets by their parent, the game instance by
`GameInstanceManager`, worlds by `WorldManager` and a level by its world, each through a
`TStrongObjectPtr`; behaviours by their world's entity storage.
There is no garbage collector yet; what one needs is in place: the table, the object flags (managed,
class default, being destroyed), and references found through the properties. A mark-and-sweep would
start from the strong pointers and the resource manager, follow `TObjectPtr` properties, and destroy
the managed objects it did not reach, without changing the three pointers' API.

Objects are not copyable (an object is an identity, not a value): build one in place, or make one
with `NewObject`.
