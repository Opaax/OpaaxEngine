# Quick start: a first game

From a new project to a build that runs on another machine. Commands are given for Windows; on Linux
and macOS use `./MakeOpaax.sh` and `./build.sh` the same way.

## 1. Create the project

```
MakeOpaax MyGame
build
```

`MakeOpaax` copies the templates in `OpaaxCreator/Templates` into `MyGame/` and adds
`add_subdirectory(MyGame)` to the root `CMakeLists.txt`. `build` makes the debug build with the editor:
`MyGameEditor` is in `build/debug-editor/bin/Debug/`.

```
MyGame/
  MyGame.opaaxproj              the project: its startup level
  Assets/                       levels, maps, textures, sounds, prefabs, data assets
  Configs/                      window, renderer and editor settings
  Source/MyGame/                the game's code: components, behaviours, subsystems
  Source/MyGameRuntime/         the game's executable (rarely touched)
  Editor/Source/MyGameEditor/   the game's editor extensions
  Tests/                        automation scripts (optional, see step 5)
```

Every `.cpp` and `.h` under `Source/MyGame/` is compiled, and every type registers itself with one
line: there is no list to keep up to date.

## 2. A behaviour

A behaviour is gameplay code on an entity: fields that are saved and edited like a component's, and
hooks the engine calls. `Source/MyGame/Spinner.h`:

```cpp
#pragma once

#include "Engine/Registries/AutoRegistration.h"
#include "World/Behaviour/Behaviour.h"

namespace MyGame
{
    // Turns its entity, and jumps when Space is pressed (with a Collider and a Dynamic Rigidbody).
    class Spinner : public Opaax::Behaviour
    {
    public:
        float DegreesPerSecond = 90.f;
        float JumpSpeed        = 500.f;

        OPAAX_PROPERTIES(Spinner, OPAAX_PROP(DegreesPerSecond), OPAAX_PROP(JumpSpeed))

        void OnUpdate(float InDeltaTime) override
        {
            SetRotation(GetRotation() + DegreesPerSecond * InDeltaTime);

            if (WasKeyPressed(Opaax::EKeyCode::Space))
            {
                SetVelocity({ 0.f, JumpSpeed });
            }
        }
    };

    OPAAX_REGISTER_BEHAVIOUR(Spinner);
}
```

Beside `OnUpdate`: `OnStart`, `OnFixedUpdate` (before each physics step) and `OnDestroy`. A behaviour
can create, spawn (prefabs) and destroy entities, send and listen to events, set timers, play sounds,
read input and actions, and push its body: `World/Behaviour/Behaviour.h` lists it all.

Plain data, with no code of its own, is a component:

```cpp
struct Health
{
    float Points = 100.f;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Health, Points)
    OPAAX_PROPERTIES(Health, OPAAX_PROP(Points).SetRange(0.f, 1000.f))
};

OPAAX_REGISTER_COMPONENT(Health);
```

## 3. Play it

Build again and start `MyGameEditor`. In the Hierarchy, create an entity; in the Inspector, add a
**Sprite** (pick a texture), a **Collider**, a **Rigidbody** and your **Spinner**. Add a second entity
with a Collider and a Rigidbody set to Static under it: the ground. Press **Play** (F5): the sprite
turns, falls on the ground and jumps with Space. **Stop** (F8) gives the level back as it was. Save
with Ctrl+S.

## 4. Light it

Add an entity with an **Environment**: the level is now drawn in HDR. Lower its Ambient Intensity, add
a **Light2D** on another entity, and a **ShadowCaster2D** to the sprite: it casts a shadow when the
light's `bCastShadows` is on. The Environment also turns on ambient occlusion and bloom; a **Material2D**
data asset gives a sprite a normal map or a glow. TestWorld's Lighting level (`TestWorld/Assets/Maps`)
uses all of it.

## 5. Test it

A script plays the level and checks what happened; it fails (exit code 1) when a check fails.
`MyGame/Tests/Spinner.json`:

```json
[
    { "command": "level.play", "params": { "path": "Levels/Main.opaaxlevel" } },
    { "command": "world.wait", "params": { "seconds": 1.0 } },
    { "command": "expect.value", "params": { "entity": "Entity", "path": "Transform/Rotation", "greater": 45 } },
    { "command": "screenshot", "params": { "path": "spinner.png" } }
]
```

```
build\debug-editor\bin\Debug\MyGame.exe --exec MyGame\Tests\Spinner.json
```

The answers are written to `Spinner.out.json` beside the script. To run such scripts with the tests,
register them like `TestWorld/CMakeLists.txt` does. Every command is in
[Automation.md](Automation.md).

## 6. Export it

In the editor, **File > Export Game...** and pick a folder; or from a shell:

```
build export MyGame C:\Exports\MyGame
```

The folder holds the release build (`MyGame.exe`), the game's content (`MyGame_Data/`) and the
engine's (`Engine/Assets/`): copy it anywhere and run it. The export builds from the files on disk, so
save first.

## Next

[Customizing.md](Customizing.md) covers subsystems, data assets, the renderer and the editor's own
extension points.
