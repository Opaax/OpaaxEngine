---
name: opaax-add-gameplay-type
description: Add a component, behaviour, world or game instance subsystem, asset class or resource type to the Opaax engine or to a game, with its registration, editor support, tests and a TestWorld check. Use when the task adds gameplay data or logic.
---

# Add a gameplay type

## Pick the kind

| Need | Kind | Registration |
|---|---|---|
| Data on entities, saved in maps | component | `OPAAX_REGISTER_COMPONENT(T)` |
| Logic on an entity (update, events, timers) | behaviour (`Opaax::Behaviour` subclass, `OPAAX_OBJECT(T, Opaax::Behaviour)` first in its body) | `OPAAX_REGISTER_BEHAVIOUR(T)` |
| Logic for a whole world | world subsystem (`Opaax::WorldSubsystem` + `OPAAX_OBJECT`) | `OPAAX_REGISTER_OBJECT(T)` |
| Logic across levels | game instance subsystem (`Opaax::GameInstanceSubsystem` + `OPAAX_OBJECT`) | `OPAAX_REGISTER_OBJECT(T)` |
| Code when the game starts or ends | the game's own class (`Opaax::GameInstance` + `OPAAX_OBJECT`, `OnStart`/`OnEnd`), named by `gameInstanceClass` in the `.opaaxproj` | `OPAAX_REGISTER_OBJECT(T)` |
| Shared tuning data in a file | asset class (`Opaax::Asset` + `OPAAX_OBJECT`, `.opaaxasset` files) | `OPAAX_REGISTER_OBJECT(T)` |
| Data in files of a format of its own | asset class read from them (`OPAAX_ASSET_SOURCE(".ext")` after `OPAAX_OBJECT`, `LoadSource`) | `OPAAX_REGISTER_OBJECT(T)` |
| A way of moving for the Mover | move mode (`Opaax::MoveMode` subclass: tuning as properties, a `const` `Tick`) | `OPAAX_REGISTER_OBJECT(T)` |
| A cached file type that is not an asset (rare: prefer an asset class read from its files) | resource | `OPAAX_REGISTER_RESOURCE(T)` |

Engine types go in their module (`Engine/Source/<Module>/`) and register in the module's
`<Module>Registration.cpp`; a game's types go anywhere under `<Game>/Source/<Game>/`.

## Write it

- Fields: defaults on every field; `OPAAX_PROPERTIES(T, OPAAX_PROP(Field)...)` for the editor and
  saving (`SetRange`, `SetDragStep`, `SetTooltip`). A component also gets
  `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(T, fields...)`, so maps saved before a new field still
  load. Behaviours are saved from `OPAAX_PROPERTIES` alone.
- Behaviours use the behaviour API (`World/Behaviour/Behaviour.h`): `GetTransform`, `SetPosition`,
  `Spawn`, `Destroy`, `Send`/`Listen`, `Broadcast`/`Subscribe`, `SetTimer`, `BindAction`, `PlaySound`,
  `SetVelocity`/`AddImpulse`, `RayCast`/`OverlapBox`. Never the EnTT registry.
- Subsystems: `OPAAX_OBJECT(T, WorldSubsystem)` (or `GameInstanceSubsystem`), no constructor work
  (the class default object is built with it), `GetContext()` from `Startup` to `Shutdown`; optional
  overrides `bool ShouldCreate(const World&) const` (Play-only systems check the world's mode) and
  `Int32 GetTickOrder() const` (`WorldSubsystemOrder::X`). Tests add one to a world with
  `World::GetSubsystems().Add<T>()` after `SetContext`, then `StartupAll()`.
- Asset classes (Docs/Objects.md): `class T final : public Opaax::Asset` with `OPAAX_OBJECT(T, Asset)`
  and `OPAAX_PROPERTIES`. A list field holds structs with `OPAAX_PROPERTIES` and
  `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT`. Another asset is a `TAssetRef<U>` field: include
  `U`'s header in an asset's header (a component forward-declares it, its registration file includes
  it). Users load with `LoadAsset(Resources, Paths, Ref)` and keep the `TAssetHandle`. Fixes after an
  edit go in `PostEditChange`, hidden fields in `IsPropertyVisible`; named entries pointing at other
  assets use `Resources/Asset/NamedAssetEntries.h`. It replaces an old file format: one line in
  `AssetMigration::GetLegacyFormats`, then `TestWorldEditor` and `SandboxEditor` with
  `--upgrade-assets --exec <a script that sends app.quit>`. A class read from files of its own format
  adds `OPAAX_ASSET_SOURCE(".ext")` after `OPAAX_OBJECT` and overrides `LoadSource` (any thread),
  `InitializeOnMainThread` (GPU or device work) and `GetResourceSize`; its files open read-only.
- An asset's editor look: `AssetTypes().Register<T>()` with `SetLabel`, `SetCategory`, `SetGlyph`
  (engine: `EditorService::RegisterNativeAssetTypes`; a game: its editor module's `OnRegister`). A
  custom editor (`IAssetEditor`, `SetEditor<E>()`) only when a field list is not enough: it edits the
  asset directly, the window does undo and Save; keep its logic in a pure header beside it
  (`Editor/Assets/Editors/*Editing.h`) and keep an ImGui item active during a drag.
- Style: AGENTS.md (prefixes, banners, short plain comments).

## Test it

1. Unit tests for its logic in `Engine/Tests/<Module>/` (a bare `World lWorld("Test")` gives entities
   without an engine; behaviour runtime tests use the fixture in `World/BehaviourRuntimeTests.cpp`).
   Include a JSON round trip and an old-file case (missing fields keep their defaults).
2. When a game can see it: a TestWorld probe or entities in a TestWorld map, and checks in the
   feature's script (`TestWorld/Tests`). Run `ctest -L feature`.
3. In the editor: create one through automation (`entity.create` with `components`), check
   `entity.get`, undo it; or open the Inspector by hand.
4. An asset class: a round trip through `AssetFile::Save` and `AssetFile::LoadAsset` with a local
   `ObjectClassRegistry` registering it, a file missing a property (default kept), a wrong type
   (refused), its hooks called directly, and an old format with `ConvertAndLoad`
   (`Engine/Tests/LegacyAssetFile.h`). Objects cannot be copied: build them in place. In the editor,
   steps in `TestWorld/EditorTests/Assets.json`: `asset.open` or `asset.new`, `asset.set` and
   `expect`, a screenshot of its window, `asset.close` with `discard`.
