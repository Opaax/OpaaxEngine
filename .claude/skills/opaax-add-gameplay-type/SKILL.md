---
name: opaax-add-gameplay-type
description: Add a component, behaviour, world or game instance subsystem, data asset or resource type to the Opaax engine or to a game, with its registration, editor support, tests and a TestWorld check. Use when the task adds gameplay data or logic.
---

# Add a gameplay type

## Pick the kind

| Need | Kind | Registration |
|---|---|---|
| Data on entities, saved in maps | component | `OPAAX_REGISTER_COMPONENT(T)` |
| Logic on an entity (update, events, timers) | behaviour (derives from `Opaax::Behaviour`) | `OPAAX_REGISTER_BEHAVIOUR(T)` |
| Logic for a whole world | world subsystem (`WorldSubsystemBase`) | `OPAAX_REGISTER_WORLD_SUBSYSTEM_ORDERED(T, WorldSubsystemOrder::X)` |
| Logic across levels | game instance subsystem | `OPAAX_REGISTER_GAME_INSTANCE_SUBSYSTEM(T)` |
| Shared tuning data in a file | data asset (`.opaaxdata`) | `OPAAX_REGISTER_DATA_ASSET(T)` |
| A file format of its own | resource | `OPAAX_REGISTER_RESOURCE(T)` |

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
- Subsystems: `OPAAX_SUBSYSTEM_TYPE(T)`, a constructor taking `WorldContext&`, `Startup`/`Shutdown`,
  optional `static bool ShouldCreate(const World&)` (Play-only systems check the world's mode).
- Style: AGENTS.md (prefixes, banners, short plain comments).

## Test it

1. Unit tests for its logic in `Engine/Tests/<Module>/` (a bare `World lWorld("Test")` gives entities
   without an engine; behaviour runtime tests use the fixture in `World/BehaviourRuntimeTests.cpp`).
   Include a JSON round trip and an old-file case (missing fields keep their defaults).
2. When a game can see it: a TestWorld probe or entities in a TestWorld map, and checks in the
   feature's script (`TestWorld/Tests`). Run `ctest -L feature`.
3. In the editor: create one through automation (`entity.create` with `components`), check
   `entity.get`, undo it; or open the Inspector by hand.
