# Customizing the engine and the editor

How to extend Opaax: the types a game adds, the renderer's settings and shaders, and the editor's
extension points. The engine itself is written the same way, so its sources are the reference
(Sandbox and TestWorld are complete examples).

## A game's types

Every type registers itself with one line next to its definition, in a header or a `.cpp`
(`Engine/Registries/AutoRegistration.h`). The engine runs the registrations at startup, before any
world exists; a game module is linked whole, so nothing else is needed. An Object class (an asset, a
subsystem) always registers with `OPAAX_REGISTER_OBJECT`: what the engine does with it follows from
the class it derives from ([Objects.md](Objects.md)).

| Line | Adds | Example |
|---|---|---|
| `OPAAX_REGISTER_COMPONENT(Health)` | Data on an entity, saved in maps, edited in the Inspector | QuickStart step 2 |
| `OPAAX_REGISTER_BEHAVIOUR(Spinner)` | Gameplay code on an entity | QuickStart step 2, `TestWorld/Source` |
| `OPAAX_REGISTER_OBJECT(ScoreSubsystem)` | A world subsystem (`Opaax::WorldSubsystem`): code that lives as long as a world and ticks with it | below |
| `OPAAX_REGISTER_OBJECT(ScoreKeeper)` | A game instance subsystem (`Opaax::GameInstanceSubsystem`): lives as long as a game session, across levels | below |
| `OPAAX_REGISTER_OBJECT(ProgressSave)` | What the game keeps for a player between launches: an `Opaax::SaveGame` subclass, its fields as properties, saved in a slot by the game's `SaveGameSubsystem` | [Saving.md](Saving.md), `TestWorld/Source/TestWorld/Probes/SaveProbes.h` |
| `OPAAX_REGISTER_OBJECT(MyGame)` | The game's own `Opaax::GameInstance` class, named in the project file: code run when the game starts and ends | below |
| `OPAAX_REGISTER_OBJECT(EnemyStats)` | An asset class (an `Opaax::Asset` with `OPAAX_OBJECT`): `.opaaxasset` files, made and edited in the editor | `Renderer/Materials/Material2D.h`, `Sandbox/Source/Sandbox/Data` |
| `OPAAX_REGISTER_OBJECT(EnemyWave)` | A file format of the game: an asset class read from its own files (`OPAAX_ASSET_SOURCE(".wave")`), loaded and cached like any asset | `Sandbox/Source/Sandbox/Data/EnemyWave.h` |
| `OPAAX_REGISTER_OBJECT(Config_Gameplay)` | A config: settings saved in `Configs/`, edited in the Config panel | below |
| `OPAAX_REGISTER_OBJECT(HealthBar)` | A UI widget for `.opaaxui` canvases: a `UIWidget` subclass, its fields as properties | `UI/Widgets` |
| `OPAAX_REGISTER_OBJECT(SwimMoveMode)` | A movement mode of the Mover: a `MoveMode` subclass, its tuning as properties and its step as `Tick` | `Movement/Modes` |

A component or behaviour is saved under its type name: rename the C++ type and maps no longer find
it. `OPAAX_REGISTER_NAMED_COMPONENT(Type, "Name")` keeps a stable name, and
`OPAAX_REGISTER_COMPONENT_ALIAS("Old", "New")` lets old maps load after a rename.

Fields listed in `OPAAX_PROPERTIES` are saved and drawn by the Inspector with no other code;
`SetRange`, `SetDragStep` and `SetTooltip` shape the editing.

### An asset class

Data shared by many entities (a kind of enemy, a weapon, a tuning) is an asset class:

```cpp
class EnemyStats final : public Opaax::Asset
{
    OPAAX_OBJECT(EnemyStats, Opaax::Asset)

public:
    float Speed = 120.f;

    OPAAX_PROPERTIES(EnemyStats, OPAAX_PROP(Speed).SetRange(0.f, 2000.f))
};

OPAAX_REGISTER_OBJECT(EnemyStats);
```

Its `.opaaxasset` files are made in the editor (New Asset in the Resource Browser), by `asset.new`, or
by hand. A component or another asset names one with a `TAssetRef<EnemyStats>` field; `LoadAsset`
loads it, shared, and the `TAssetHandle` it returns reads the new values when the file is saved.
`PostEditChange` fixes values after an edit, `IsPropertyVisible` hides fields that do not apply.
[Objects.md](Objects.md) covers the file, the hooks, custom editors and old files.

### A subsystem

```cpp
class ScoreSubsystem final : public Opaax::WorldSubsystem
{
    OPAAX_OBJECT(ScoreSubsystem, Opaax::WorldSubsystem)

public:
    // Optional: without it, every world gets one (the editor's edit worlds too).
    bool ShouldCreate(const Opaax::World& InWorld) const override { return InWorld.GetMode() == Opaax::EWorldMode::Play; }

    // Optional: Default otherwise.
    Opaax::Int32 GetTickOrder() const override { return Opaax::WorldSubsystemOrder::Gameplay; }

    bool Startup() override { return true; }   // GetContext() is valid from here to Shutdown
    void Update(double InDeltaTime) override { /* the world's frame */ }
};

OPAAX_REGISTER_OBJECT(ScoreSubsystem);
```

Every world makes one of each registered `WorldSubsystem` class whose `ShouldCreate` accepts it
(asked on the class's default object), then starts them all: a `Startup` can find a sibling that has
not started yet. They tick in `GetTickOrder()` order (`WorldSubsystemOrder`: Input, Gameplay before
physics, Physics, PostPhysics, Default, Presentation for camera, animation and HUD, Debug), equal
orders by class name. The constructor makes nothing: the default object is built with it, outside any
world. `GetContext()` gives the world's services (its events, input actions, audio, resources, the
running game), and a behaviour finds the subsystem with `GetSubsystem<ScoreSubsystem>()`.

A game instance subsystem derives from `Opaax::GameInstanceSubsystem` and registers the same way.
Every game makes one of each class, in `GetTickOrder()` order, and it lives from the start of the game
to its end, across level changes (a score, a save); `GetContext()` gives the game's services. A
behaviour finds it with `GetGameSubsystem<ScoreKeeper>()`, null while no game runs (in the editor
outside Play). `TestWorld/Source/TestWorld/Probes/LevelProbes.h` has one, and the engine's own
`SaveGameSubsystem` keeps the player's saves ([Saving.md](Saving.md)).

The game session itself can be a class of the game: a `GameInstance` subclass with `OPAAX_OBJECT`,
registered, and named in the project file (`"gameInstanceClass": "MyGame"`). `OnStart` runs once its
subsystems have started, before the first world; `OnEnd` once its Play worlds are gone, before its
subsystems stop. A behaviour reaches it through `GetContext().Game`. Most session features are better
as game instance subsystems, which every game makes. `ProbeGame` in
`TestWorld/Source/TestWorld/Probes/ExtensionProbes.h` is one.

### A config

Settings a game reads (difficulty, a spawn rate) and edits in the editor's Config panel are a config:
a data struct saved in `Configs/<Name>.config`, held by a `Config` class.

```cpp
struct GameplayConfigData
{
    float SpawnRate = 2.f;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GameplayConfigData, SpawnRate)
    OPAAX_PROPERTIES(GameplayConfigData, OPAAX_PROP(SpawnRate).SetRange(0.f, 10.f))
};

DECLARE_T_CONFIG(Gameplay, GameplayConfigData)   // Config_Gameplay, saved as Gameplay.config

OPAAX_REGISTER_OBJECT(Config_Gameplay);           // in a .cpp
```

`IConfigSystem::Get<Config_Gameplay>().GetData()` reads it. The engine makes one of each registered
config class at startup and loads its file (written with the defaults when missing), so the Config
panel lists it before the game first reads it, draws its values from their properties, and saves or
resets them. A missing key keeps its default. Subscribe to `OnChanged()` to follow edits made while
the game runs.

What a player changes in a settings menu goes over the project's values without touching its file:
`GetUserSettings().SetConfigValues("Gameplay", R"({"SpawnRate": 3})")`, then `GetData()` reads 3 and
`GetProjectData()` the file's value ([Settings.md](Settings.md)).

### Events, input and sound

From a behaviour: `Send(entity, event)` / `Listen<&Class::OnEvent>()` for an entity's events,
`Broadcast(event)` / `Subscribe<&Class::OnEvent>()` for everyone's, `SetTimer<&Class::OnTimer>(seconds)`,
`BindAction<&Class::OnJump>("Jump", EInputTrigger::Started)` for input actions (`InputActionAsset`
assets, bound to keys by `InputMappingContextAsset` assets), `PlaySound("Audio/Coin.wav")`. Handlers
can be `const` members.

### Sound

- **Play:** `PlaySound(path)` takes a sound file or a clip definition (`"Audio/Steps.opaaxasset"`: its
  clips are variations, one chosen per play, with its bus, volume, pitch spread, loop and space).
  `PlaySoundAt(path, position)` places it in the world; `StopSound(handle, fadeSeconds)` fades it out.
  A handle stays harmless once its sound ended. An `AudioSource` component plays with its entity
  (`Sound` for a definition, or `Clip` with its own settings) and an `AudioListener` hears for the
  level (else the camera).
- **Buses:** `Audio.Buses` in `Engine.config` is the mix tree under the master (Music, SFX, Dialogue,
  Ambience, UI by default, and any of yours). From code:
  `GetSubsystem<AudioSubsystem>()->GetAudio()` gives the `AudioManager`, with `SetBusVolume`,
  `FadeBus(OPAAX_ID("Music"), 0.f, 2.f)` (a fade apart from the player's volume), `SetBusMuted`,
  `SetBusSolo`.
- **Banks:** an `AudioBank` asset lists clip definitions and sound files to load before they
  play. A level lists its banks in an `AudioBanks` component on any entity of its map; `Audio.GlobalBank`
  in `Engine.config` is held for the whole game (menus, the player). A sound in no bank is logged,
  loads in the background and plays once loaded.
- **Tests:** `AudioManager::SetRandomSeed` makes the choices (variations, spread) repeat. Without a
  device (`Audio.bEnabled` off, a server) the mix runs headless and sounds still end on time.
- **Another audio library:** implement `IAudioDevice` and `IAudioDecoder` in `Audio/<Library>`, add its
  `EAudioBackend` value and its case in `AudioBackendFactory.cpp`; `Audio.Backend` picks it.

### Online

- **The game's boards and stores:** `Configs/Online.config` (`Config_Online`) lists the stores a game
  tries (`Backends`), its leaderboards (name, sort, display) and each store's settings (`Steam.AppId`).
  The editor's Config panel edits it; it is read once at startup.
- **Calls:** `GetGameSubsystem<OnlineSubsystem>()` uploads scores, reads boards and files, and broadcasts
  the overlay's opening (`OnOverlayChanged`) for the game to pause. Answers come at a later frame.
- **Another store:** a backend in `Online/<Store>/` behind `IOnlineBackend`, its SDK declared with
  `opaax_add_online_sdk` and its case in `OnlineBackendFactory.cpp` ([Online.md](Online.md#adding-a-store)).

## The renderer

A level is framed by a **Camera** entity: its world position and `OrthoSize` (half the visible
height, in world units). A camera parented to the player follows it. With several cameras, the
highest `Priority` frames the level: raise another's (a map view, a cutscene) to switch to it.

How a camera moves smoothly at any frame rate (follows, zooms, shakes, respawns), the rules it
follows and how to check one: [Cameras.md](Cameras.md).

A level is lit when it has an **Environment** (any entity): it is then drawn in HDR, lit by its
**Light2D**s and the ambient light, then bloomed, exposed and tonemapped. Without one, sprites are
drawn as they are and lights do nothing (the log says so once).

| Component or asset | What it does |
|---|---|
| Environment | Ambient light, ambient occlusion (`bAmbientOcclusion`, radius, strength), bloom (threshold, softness, intensity), exposure, tonemapper (ACES, Reinhard, None) |
| Light2D | Point, Spot (along the entity's rotation) or Global; colour, intensity, radius, falloff, height (how low lights graze normal maps); shadows (`bCastShadows`, softness, strength) |
| ShadowCaster2D | The entity's sprite (by its alpha) or quad blocks lights that cast shadows, and darkens the ambient light around it |
| Material2D (an asset) | On a Sprite: lit or not, a normal map (tangent space, Y up, the sprite's layout), an emissive colour and strength |

### Smooth edges: which setting

Jagged or shimmering pictures have several causes in a 2D game, and each has its own cure. All four
are in `Renderer.config`, apply at once, and are off by default (today's pictures).

| What you see | Why | Setting |
|---|---|---|
| Art drawn smaller than its pixels shimmers and loses thin strokes | A reduced texture is read at too few texels | `TextureFilter` `Smooth` (mipmaps) |
| Art drawn larger than its pixels is blocky | The nearest texel is read when enlarged | `TextureFilter` `Smooth` |
| Stair steps on the edge of a turned quad, a tile, a solid sprite | One sample per pixel | `Multisampling` |
| Stair steps inside a texture, on a sprite's transparent border, a hollow quad, an edge brighter than white | Multisampling only sees the edges of what is drawn | `bFXAA` |
| Everything, at a price; or a GPU too slow for the screen's pixels | | `RenderScale` (above or below 1) |

For a sprite game the texture filter is the largest gain: a sprite's edge is in its texture, where
multisampling does nothing. The player can choose the last three ([Settings.md](Settings.md#the-picture)).

### Textures

`TextureFilter` in `Renderer.config` (Config panel, Renderer) says how every sprite and UI image is
read when it is drawn at another size than its pixels. It applies at once, without a restart.

| `TextureFilter` | Drawn smaller | Drawn larger | For |
|---|---|---|---|
| `Nearest` | the nearest texel | the nearest texel | Hard pixels at any size; shimmers when small or moving slowly |
| `Pixel` (the default) | blended | the nearest texel | Pixel art |
| `Smooth` | blended, through the texture's mipmaps | blended | Drawn or painted art: thin strokes stay when small, no blocks when large |

Every colour texture gets its mipmaps when it is uploaded (a third more video memory), so the choice
can change while the game runs. Text is not concerned: a font atlas is always blended.

Two things hold under every filter:

- A texture is clamped at its edges: a sprite's border takes nothing from the opposite side.
- When an image is loaded, its fully transparent texels take the colour of the nearest texel that is
  not (their alpha stays 0). A paint program leaves any colour there, often black, and a blended read
  at a sprite's edge would draw it as a dark line. An image that keeps data under alpha 0 loses it.

A sprite or a UI image drawn from a sheet frame reads inside its frame only: each quad carries its
frame's rectangle and the sprite shader keeps blended reads half a texel inside it, so frames that
touch do not take colour from each other, enlarged or reduced. One case is left, with `Smooth` and a
frame drawn much smaller: a mipmap texel covers 2, 4, 8... texels of the sheet, and one that lies
across two frames holds both colours. Frames whose position and size are multiples of 8 or 16 texels
(a grid of such cells) stay clean down to an eighth or a sixteenth of their size; otherwise leave a
few empty texels between frames (the sheet's `Spacing`). Code that draws from a frame itself passes
it to `Renderer2D::DrawSprite` (`InUVFrame`, from `MakeUVBounds`).

`renderer.info` answers the filter in use (`textureFilter`), and `TestWorld/Levels/Filtering` shows
the three side by side.

### Multisampling

`Multisampling` in `Renderer.config` (`Off`, `X2`, `X4`, `X8`; `Off` by default) draws the world
with that many samples per pixel, then averages them, so the edges of shapes lose their stair steps:
a turned quad, a tile, a solid sprite. It applies at once. A driver that gives fewer samples uses
its most; `renderer.info` answers the samples given for each world view (`views`), and the log says
it at each change.

It only sees the edges of what is drawn, not what a texture holds:

- A sprite whose border is transparent texels is not changed: its edge is in the texture (the
  texture filter decides how it looks).
- What the sprite shader cuts out with `discard` (hollow quads, masks) stays hard.
- In a lit world the samples are averaged before the tonemap: on an edge brighter than white (an
  emissive above 1) the mean still clips, and the edge stays hard.
- Overlays drawn over the world and the UI are drawn after it, into the view's target, at one sample.

With it on, an unlit world is drawn into a scene target of its own (RGBA8) and copied into the view;
a lit world's HDR target is multisampled. Its cost is memory and fill: X4 draws four times the
pixels of the world. `TestWorld/Levels/AntiAliasing` and `AntiAliasingLit` show each case.

### FXAA

`bFXAA` in `Renderer.config` (off by default) smooths the finished world picture after the tonemap
(fast approximate anti-aliasing, after Lottes' FXAA 3.11). It looks for sharp changes of brightness
in the picture itself, so it sees what multisampling cannot: a sprite's transparent border, a
`discard` edge, an edge brighter than white. It applies at once, and can be used with multisampling.

- The overlays drawn over the world and the UI come after it: text and editor gizmos stay sharp.
- It blurs a little: thin lines and small text drawn in the world soften.
- Its cost is one full-screen pass and one more target of the view's size (RGBA8): the world is
  written there, then FXAA writes the view.

`renderer.info` answers `fxaa` (asked) and, per world view, `fxaaTarget` (used). Without
`Engine/Assets/Shaders/FXAA.glsl` the setting does nothing, and the log says so at startup.

### Render scale

`RenderScale` in `Renderer.config` (0.5 to 2; 1 by default) draws the world into a scene target of
that scale of the view, then fits it to the view with a blended read. It applies at once.

- Below 1, for a slow GPU: fewer pixels, a softer world. Shadows and ambient occlusion follow the
  scene's density, so at 0.5 they cost a quarter too.
- Above 1: every edge is smoothed, inside textures too, at the full price: 2 draws four times the
  pixels (2 to 1 averages each 2x2 block exactly).
- The overlays drawn over the world and the UI keep the view's own pixels.
- In a lit world the fit to the view is made before the tonemap: a coloured emissive above 1 looks
  white inside, but where its edge is blended with the background its own hue shows as a thin
  fringe (multisampling does the same, more faintly).

`renderer.info` answers `renderScale` (asked, clamped) and each world view's scene target size.

A player can choose multisampling, FXAA and the render scale in a settings menu; their values go over
the project's ([Settings.md](Settings.md#the-picture)). The texture filter stays the project's.

### How a world view is drawn

A world view with every setting off and no Environment is drawn straight into its target, as before
these settings existed. Otherwise it goes through a scene target: RGBA16F when lit, RGBA8 otherwise,
of the view's size times `RenderScale`, with the samples asked. The world and the overlays behind it
are drawn there; the samples are averaged; a lit world is bloomed, exposed and tonemapped, an unlit
one copied; with FXAA that lands in a middle target, which FXAA reads to write the view. The overlays
over the world and the UI are then drawn into the view's target.

`PlanSceneTarget` (`Renderer/Post/SceneTargetPlan.h`) makes that choice for each view, alone and
tested. The targets are kept per size, format and samples (`TRenderTargetCache`,
`Renderer/Post/RenderTargetCache.h`) and dropped once unused for a few frames, so two views of different sizes (the editor's viewport and its camera preview)
recreate nothing from frame to frame: `renderer.info` answers `targets.recreated`, 0 in a steady frame.

### Shaders

The shaders are in `Engine/Assets/Shaders`, one file per program with `#type vertex` and
`#type fragment` sections. They are written in GLSL 4.50 with explicit `layout(binding = N)`, and
ported to GLSL 4.10 at load time (`RHI/OpenGL/GLSLPort`), since macOS stops at OpenGL 4.1.

| Binding | Block or sampler | Shader |
|---|---|---|
| 0..15 | `u_Textures[16]`: the batch's textures. In lit passes, 14 is the ambient occlusion map and 15 the shadow map | Sprite |
| 1 | `CameraUBO`: view-projection, pass flags | Sprite |
| 2 | `PostUBO`: exposure, tonemapper, bloom | Tonemap |
| 3 | `LightsUBO`: ambient, ambient occlusion, up to 32 lights | Sprite |
| 4 | `ShadowUBO`: up to 16 shadowed lights | Shadow2D |
| 5 | `AmbientOcclusionUBO` | AmbientOcclusion2D |
| 6 | `BloomUBO` | Bloom2D |
| 7 | `FXAAUBO`: one texel, the sub-pixel blend | FXAA |

A pass reads its blocks and its textures through its bind group (`IBindGroup`): the group lists the
binding points of its blocks, and binding it before the draw is what puts each buffer and each
texture in place. Creating a uniform buffer binds nothing, so a block a shader reads must be in the
group of every pass that uses that shader, or the shader reads whatever the pass before left there.

C++ mirrors of these blocks are in `Renderer/Lighting/Lighting2D.h` and `Renderer/Post`; the constants
a shader repeats from C++ (light counts, slots, sizes) are checked by `EngineShaderTests`, so change
both sides together. The passes after the world is drawn (shadow and ambient occlusion maps, bloom,
the tonemap composite) are in `Renderer/Post/ScenePipeline2D`: a new post effect is a pass there, with
its settings on the Environment.

To see a change, play a level and take a screenshot: `TestWorld --exec TestWorld/Tests/Lighting.json`
writes `Lighting.png`, or `Sandbox --capture shot.png --capture-frame 30`. A script checks a picture
by itself with `frame.sample`, which counts the colours of a part of the frame, and `renderer.info`
says what the renderer is using: the GPU, the driver and its limits
([Automation.md](Automation.md#commands)).

In a dev build, a wrong graphics call is an Error line in the log, in the `OpenGL` category, with
the driver's own text; doubtful use the driver rates medium or high is a Warn line. Each message is
logged once. Where the driver cannot report calls (macOS), the error flags are read once a frame:
the line then says that a call was wrong, not which. `renderer.info` says which of the two is in use
(`errorChecks`). A ship build checks neither.

## The editor

A game's editor module (`Editor/Source/<Game>Editor/<Game>EditorModule.cpp`) registers its
extensions in `OnRegister`:

```cpp
void MyGameEditorModule::OnRegister(Opaax::Editor::EditorExtensionRegistrar& InRegistrar)
{
    // A command (a struct with Params and Execute), then a menu entry that runs it by tag.
    InRegistrar.Commands().Register<ValidateLevelCommand>(MY_VALIDATE_TAG);
    InRegistrar.TitleBar().Category("Tools").SubCategory("Checks").AddCommand("Validate Level", MY_VALIDATE_TAG);

    // A panel: derives from IEditorPanel and is built from the EditorContext.
    InRegistrar.Panels().Register<WaveEditorPanel>(Opaax::Editor::PanelDesc{ OPAAX_ID("Waves") });

    // An Inspector drawer replacing the one made from the component's OPAAX_PROPERTIES.
    InRegistrar.Drawers().Register<TagsComponent, TagsComponentDrawer>();

    // How an asset class shows: its New Asset category, its glyph, and an editor of its own (optional:
    // without one, its window lists its properties).
    InRegistrar.AssetTypes().Register<EnemyStats>()
        .SetCategory(Opaax::OpaaxString("Gameplay"))
        .SetGlyph(Opaax::OpaaxString("[E]"))
        .SetEditor<EnemyStatsEditor>();
}
```

An asset editor derives from `IAssetEditor` and edits the asset directly in `DrawContents`; the
asset's window around it records each change as an undo step and has the Save button
([Objects.md](Objects.md)).

Also available: `ViewportTools()` (buttons over the viewport). A UI widget needs no drawer: the
UI panel draws any widget from its class's properties, and the Config panel draws any config from
its values' properties. A subsystem for edit worlds only is a `WorldSubsystem` like any other,
registered with `OPAAX_REGISTER_OBJECT`, whose `ShouldCreate` accepts edit worlds.
`Sandbox/Editor/Source/SandboxEditor` has a working command, drawer and asset type (the `.wave` files).

A command receives the `EditorContext`: the worlds, the selection, undo, the open documents, the
dialogs. Edits made through `EntityOps` (create, destroy, add or remove a component) and
`EntityComponentsEdit` (property changes) are undoable. The editor's own commands are in
`Editor/Commands/EditorNativeCommands.h`; any of them can be bound to a menu or run by tag.

## Automation

Every command a script or an agent can send lives in the app's command registry
(`ICommandRegistry`, an app service there in every run): the engine's are in
`Automation/EngineAutomationCommands.cpp`, the editor's in `Editor/Automation`. A game adds its own
in two ways.

For the whole run, by overriding `OnRegisterCommands(ICommandRegistry&)` in its application class
(called once the engine started; a name the engine has too runs the game's):

```cpp
void MyGameApp::OnRegisterCommands(Opaax::ICommandRegistry& InCommands)
{
    InCommands.Add("game.version", "The game's version: {version}.", [](const nlohmann::json&)
    {
        return Opaax::CommandResult::Ok(nlohmann::json{ { "version", "1.2" } });
    });
}
```

For the lifetime of a subsystem, from its context (`GetContext().Commands`: a reference in a game
instance subsystem, a pointer in a world subsystem, null in a bare test world). `Register` returns a
`CommandHandle`; keep it as a member, and the command is gone with the subsystem:

```cpp
bool ScoreSubsystem::Startup()
{
    m_ScoreCommand = GetContext().Commands.Register(
        Opaax::CommandSpec("game.score", "The current score: {score}."),
        [this](const nlohmann::json&) { return Opaax::CommandResult::Ok(nlohmann::json{ { "score", m_Score } }); });
    return true;
}
```

When two registrations share a name, the newest runs, and each handle removes only its own: the
next level's world can register its command before the last level's world is gone.

Declared parameters (`Param`, `Choices`, `Optional`) let the dev console check a typed line and
complete it with Tab; `Cheat()` marks a command that voids a run, `DevOnly()` one a ship build leaves
out ([Console.md](Console.md)).

See [Automation.md](Automation.md) for the protocol, and [AssetFormats.md](AssetFormats.md) for the
asset files a tool or an agent can write directly.

## Rules the engine follows

- Third-party APIs stay behind the code that wraps them: EnTT in `World`, GLFW in `Platform`, Box2D
  in `Physics/Box2D`, miniaudio in `Audio/Miniaudio`, OpenGL in `RHI/OpenGL`, tinyfiledialogs in the editor's
  dialogs. Type ids come from `Core/Reflection/TypeInfo.h`.
- The engine is a static library linked whole into each executable; a game is declared by
  `opaax_add_game(<Name>)` (`CMake/OpaaxGame.cmake`).
- Code style and the rest: [AGENTS.md](../AGENTS.md).
