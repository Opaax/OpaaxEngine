# Customizing the engine and the editor

How to extend Opaax: the types a game adds, the renderer's settings and shaders, and the editor's
extension points. The engine itself is written the same way, so its sources are the reference
(Sandbox and TestWorld are complete examples).

## A game's types

Every type registers itself with one line next to its definition, in a header or a `.cpp`
(`Engine/Registries/AutoRegistration.h`). The engine runs the registrations at startup, before any
world exists; a game module is linked whole, so nothing else is needed.

| Line | Adds | Example |
|---|---|---|
| `OPAAX_REGISTER_COMPONENT(Health)` | Data on an entity, saved in maps, edited in the Inspector | QuickStart step 2 |
| `OPAAX_REGISTER_BEHAVIOUR(Spinner)` | Gameplay code on an entity | QuickStart step 2, `TestWorld/Source` |
| `OPAAX_REGISTER_WORLD_SUBSYSTEM(Score)` | Code that lives as long as a world and ticks with it | below |
| `OPAAX_REGISTER_GAME_INSTANCE_SUBSYSTEM(Save)` | Code that lives as long as a game session (across levels) | |
| `OPAAX_REGISTER_DATA_ASSET(EnemyStats)` | A struct stored in `.opaaxdata` files, edited in the editor | `Renderer/Materials/Material2D.h` |
| `OPAAX_REGISTER_RESOURCE(WaveResource)` | A file type of the game, loaded and cached | `Sandbox/Source/Sandbox/Resources` |
| `OPAAX_REGISTER_UI_WIDGET(HealthBar)` | A UI widget for `.opaaxui` canvases | `UI/Widgets` |
| `OPAAX_REGISTER_MOVER_MODE(Swim, "Swim")` | A movement mode of the Mover | `Movement` |

A component or behaviour is saved under its type name: rename the C++ type and maps no longer find
it. `OPAAX_REGISTER_NAMED_COMPONENT(Type, "Name")` keeps a stable name, and
`OPAAX_REGISTER_COMPONENT_ALIAS("Old", "New")` lets old maps load after a rename.

Fields listed in `OPAAX_PROPERTIES` are saved and drawn by the Inspector with no other code;
`SetRange`, `SetDragStep` and `SetTooltip` shape the editing.

### A world subsystem

```cpp
class ScoreSubsystem final : public Opaax::WorldSubsystemBase
{
public:
    OPAAX_SUBSYSTEM_TYPE(ScoreSubsystem)

    // Optional: without it, every world gets one (the editor's edit worlds too).
    static bool ShouldCreate(const Opaax::World& InWorld) { return InWorld.GetMode() == Opaax::EWorldMode::Play; }

    explicit ScoreSubsystem(Opaax::WorldContext& InContext) : m_Context(&InContext) {}

    bool Startup() override { return true; }
    void Update(double InDeltaTime) override { /* the world's frame */ }
    void Shutdown() override {}

private:
    Opaax::WorldContext* m_Context = nullptr;
};

OPAAX_REGISTER_WORLD_SUBSYSTEM_ORDERED(ScoreSubsystem, Opaax::WorldSubsystemOrder::Gameplay);
```

Subsystems tick in `WorldSubsystemOrder` order: Input, Gameplay (before physics), Physics,
PostPhysics, Default, Presentation (camera, animation, HUD), Debug. A behaviour finds one with
`GetSubsystem<ScoreSubsystem>()`. The `WorldContext` holds the world's services: its events, input
actions, audio, resources, and the running game (`Game`).

A game instance subsystem derives from `Opaax::GameInstanceSubsystemBase`, takes a
`GameInstanceContext&`, and lives from the start of a game to its end, across level changes (a
score, a save). A behaviour finds it with `GetGameSubsystem<Save>()`, null while no game runs (in the
editor outside Play). `TestWorld/Source/TestWorld/Probes/LevelProbes.h` has one.

### Events, input and sound

From a behaviour: `Send(entity, event)` / `Listen<&Class::OnEvent>()` for an entity's events,
`Broadcast(event)` / `Subscribe<&Class::OnEvent>()` for everyone's, `SetTimer<&Class::OnTimer>(seconds)`,
`BindAction<&Class::OnJump>("Jump", EInputTrigger::Started)` for input actions (declared in
`.opaaxaction` files, bound to keys in `.opaaxinputmap` files), `PlaySound("Audio/Coin.wav")`. Handlers
can be `const` members.

## The renderer

A level is lit when it has an **Environment** (any entity): it is then drawn in HDR, lit by its
**Light2D**s and the ambient light, then bloomed, exposed and tonemapped. Without one, sprites are
drawn as they are and lights do nothing (the log says so once).

| Component or asset | What it does |
|---|---|
| Environment | Ambient light, ambient occlusion (`bAmbientOcclusion`, radius, strength), bloom (threshold, softness, intensity), exposure, tonemapper (ACES, Reinhard, None) |
| Light2D | Point, Spot (along the entity's rotation) or Global; colour, intensity, radius, falloff, height (how low lights graze normal maps); shadows (`bCastShadows`, softness, strength) |
| ShadowCaster2D | The entity's sprite (by its alpha) or quad blocks lights that cast shadows, and darkens the ambient light around it |
| Material2D (`.opaaxdata`) | On a Sprite: lit or not, a normal map (tangent space, Y up, the sprite's layout), an emissive colour and strength |

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

C++ mirrors of these blocks are in `Renderer/Lighting/Lighting2D.h` and `Renderer/Post`; the constants
a shader repeats from C++ (light counts, slots, sizes) are checked by `EngineShaderTests`, so change
both sides together. The passes after the world is drawn (shadow and ambient occlusion maps, bloom,
the tonemap composite) are in `Renderer/Post/ScenePipeline2D`: a new post effect is a pass there, with
its settings on the Environment.

To see a change, play a level and take a screenshot: `TestWorld --exec TestWorld/Tests/Lighting.json`
writes `Lighting.png`, or `Sandbox --capture shot.png --capture-frame 30`.

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

    // A file type of the game in the Resource Browser (the runtime module registers the resource).
    InRegistrar.ResourceTypes().Register<WaveResource>().SetGlyph(Opaax::OpaaxString("[W]"));
}
```

Also available: `ViewportTools()` (buttons over the viewport), `EditWorldSystems()` (subsystems for
edit worlds only), `ConfigDrawers()` and `UIWidgetDrawers()`. `Sandbox/Editor/Source/SandboxEditor`
has a working command, drawer and resource type.

A command receives the `EditorContext`: the worlds, the selection, undo, the open documents, the
dialogs. Edits made through `EntityOps` (create, destroy, add or remove a component) and
`EntityComponentsEdit` (property changes) are undoable. The editor's own commands are in
`Editor/Commands/EditorNativeCommands.h`; any of them can be bound to a menu or run by tag.

## Automation

Every command a script or an agent can send is registered on an `AutomationRunner`: the engine's in
`Automation/EngineAutomationCommands.cpp`, the editor's in `Editor/Automation`. A game adds its own by
overriding `OnAutomationStarted(AutomationRunner&)` in its application class:

```cpp
void MyGameApp::OnAutomationStarted(Opaax::AutomationRunner& InRunner)
{
    InRunner.Register("game.score", "The current score: {score}.", [](const nlohmann::json&)
    {
        return Opaax::AutomationResult::Ok(nlohmann::json{ { "score", 42 } });
    });
}
```

See [Automation.md](Automation.md) for the protocol.

## Rules the engine follows

- Third-party APIs stay behind the code that wraps them: EnTT in `World`, GLFW in `Platform`, Box2D
  in `Physics/Box2D`, miniaudio in `Audio`, OpenGL in `RHI/OpenGL`, tinyfiledialogs in the editor's
  dialogs. Type ids come from `Core/Reflection/TypeInfo.h`.
- The engine is a static library linked whole into each executable; a game is declared by
  `opaax_add_game(<Name>)` (`CMake/OpaaxGame.cmake`).
- Code style and the rest: [AGENTS.md](../AGENTS.md).
