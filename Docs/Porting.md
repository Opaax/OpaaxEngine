# Porting game code to the object model

Every engine type is now an `Object` class (`Object/Object.h`) or an `Asset` class, components
excepted. Game code written before needs the changes below. Content files do not: maps, levels,
prefabs, canvases, `.config` files and `.opaaxasset` files load as they are.

## Registration

| Before | Now |
|---|---|
| `OPAAX_REGISTER_WORLD_SUBSYSTEM(T)`, `..._ORDERED(T, Order)`, `OPAAX_REGISTER_NAMED_WORLD_SUBSYSTEM` | `OPAAX_OBJECT(T, Opaax::WorldSubsystem)` first in the class, then `OPAAX_REGISTER_OBJECT(T)`; the order is `GetTickOrder()` |
| `OPAAX_REGISTER_GAME_INSTANCE_SUBSYSTEM(T)`, `OPAAX_REGISTER_NAMED_GAME_INSTANCE_SUBSYSTEM` | `OPAAX_OBJECT(T, Opaax::GameInstanceSubsystem)`, then `OPAAX_REGISTER_OBJECT(T)` |
| `OPAAX_REGISTER_UI_WIDGET(T)`, `OPAAX_REGISTER_NAMED_UI_WIDGET` | `OPAAX_OBJECT(T, Opaax::UIWidget)`, then `OPAAX_REGISTER_OBJECT(T)` |
| `OPAAX_REGISTER_MOVER_MODE(T, Name)` on an `IMoverMode` | a `MoveMode` subclass (an asset class) with `OPAAX_OBJECT`, then `OPAAX_REGISTER_OBJECT(T)` |
| `OPAAX_REGISTER_RESOURCE(T)`, `OPAAX_REGISTER_NAMED_RESOURCE` | an asset class read from its own files: `OPAAX_ASSET_SOURCE(".ext")` after `OPAAX_OBJECT`, `LoadSource`, then `OPAAX_REGISTER_OBJECT(T)` ([Objects.md](Objects.md)) |
| `IMPL_T_CONFIG(Name)` | `OPAAX_REGISTER_OBJECT(Config_Name)`; `DECLARE_T_CONFIG` stays |

`OPAAX_REGISTER_BEHAVIOUR(T)` stays, but the behaviour declares its class first in its body:
`OPAAX_OBJECT(T, Opaax::Behaviour)`.

## Classes

- **Subsystems:** the constructor takes nothing and does nothing (the class default object is built
  with it). Services come from `GetContext()`, valid from `Startup`. Which worlds get one, and when it
  ticks: override `ShouldCreate` and `GetTickOrder`.
- **UI widgets:** saved fields go in `OPAAX_PROPERTIES`; `SaveFields` and `LoadFields` go. Make one
  with `NewObject<T>()`; its parent owns it (`AddChild`).
- **Move modes:** the tuning is the subclass's properties and the step its `const` `Tick`; a Mover's
  mode asset names the subclass.
- **The game:** a `GameInstance` is made by its class, without arguments. A game class of its own is a
  `GameInstance` subclass with `OnStart`/`OnEnd`, named in the project file:
  `"gameInstanceClass": "MyGame"`.
- **Worlds:** `World::GetName()` and `CreateWorld` take an `OpaaxStringID` (`.CStr()` for text).
- **Configs:** `IConfig` is `Config`, an Object class found by its class; a hand-written config
  declares `OPAAX_OBJECT(T, Opaax::Config)` (`DECLARE_T_CONFIG` does it). `GetData()` is unchanged.

## Fields and loading

| Before | Now |
|---|---|
| `TResourcePath<TextureResource>` | `TAssetRef<Texture2D>` (`Renderer/Textures/Texture2D.h`) |
| `TResourcePath<FontFaceResource>` | `TAssetRef<FontFace>` (`Renderer/Text/FontFace.h`) |
| `TResourcePath<AudioClipResource>` | `TAssetRef<AudioClip>` (`Audio/AudioClip.h`) |
| `TResourcePath<PrefabResource>`, `THardResourcePath<PrefabResource>` | `TAssetRef<PrefabAsset>`, `THardAssetRef<PrefabAsset>` (`World/Prefab/PrefabAsset.h`) |
| `TResourcePath<T>` of a game's own type | `TAssetRef<T>` of its asset class |
| `Resources.Load<TextureResource>(path)` and the like | `LoadAsset<Texture2D>(Resources, Paths, path)`, keeping the `TAssetHandle` |
| `MapResource`, `LevelResource`, `UICanvasResource` | `MapAsset`, `LevelAsset`, `UICanvasAsset` |
| `AudioManager::Play` with a clip resource | `Play` with an `AudioClip` or a `TAssetHandle<AudioClip>` |

The saved text of a field is the same path string, so files need no change. A component that names
an asset forward-declares the class; the file that registers it includes the class's header.

## Editor modules

| Before | Now |
|---|---|
| `ResourceTypes().Register<T>().SetGlyph(...).SetActivate(...)` | `AssetTypes().Register<T>()` with `SetLabel`, `SetGlyph`, `SetIcon`, and `SetActivate` only when its files open a document of their own |
| `ConfigDrawers().Register<T>()` | nothing: the Config panel draws any config from its properties |
| `UIWidgetDrawers` | nothing: the UI panel draws any widget from its properties |

A registration macro in editor code never runs (the editor library is not linked whole): register
editor-side things through the module's `EditorExtensionRegistrar`.
