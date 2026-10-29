# Cameras

How a level is framed, the rules a camera follows so it is smooth on any screen, and how to check one.
The camera is drawn the same at 30, 60, 144 or 240 frames a second, steady or not: the screen's rate
only chooses which moments of its path are shown.

## A camera in a level

A level is framed by a **Camera** entity: its world position and `OrthoSize` (half the visible
height, in world units). A camera parented to the player follows it. With several cameras, the
highest `Priority` frames the level: raise another's (a map view, a cutscene) to switch to it. The
view stays upright unless the camera's `bRotateView` is on: then it rolls with the entity's world
rotation (counter-clockwise, so the world is drawn turned the other way), and a camera under a
turning body turns with it.

The world is simulated in fixed steps (`Time.FixedRate`, 60 a second by default) and drawn once a
frame, at the screen's rate. Everything gameplay moves in the steps is drawn between its last two
steps, at how far the frame is into the next one (the alpha): bodies, entities the mover moves, and
every camera, its pose and its `OrthoSize`. So a camera following a moving body keeps it on the same
spot of the screen, and a zoom made in the steps is smooth on screens faster than the steps. Another
entity that gameplay moves in a fixed step is drawn between its steps once it asks:
`TransformInterpolation::RecordSteps(GetEntity())` in `OnStart` (`World/Entity/TransformInterpolation.h`).

Every camera is recorded, so one that takes the view over keeps what it frames still from its first
frame. A paused world is drawn as it was on its last frame; a step moves it by one step.

### Switching cameras

When another camera takes over (its `Priority` raised above the live one's), the view cuts to it,
or blends to it over its `BlendIn` seconds, eased by its `BlendCurve` (`Linear`, `EaseIn`,
`EaseOut`, `EaseInOut`, or `Cut`). The position blends along a line, the size in log space (so the
zoom feels even) and the roll the short way round. The camera left keeps moving while the view
leaves it; another switch during a blend blends from where the view is, and
`CameraManager::Cut(world)` ends a blend on the live camera. A world's `CameraDirector` does this.

A blend runs on game time, like everything drawn: raised in a fixed step (`OnFixedUpdate`), it starts
at that step and is drawn the same at any frame rate; raised in `Update` or by a tool, it starts at
the next step. A paused world holds a blend where it is.

## The rules

1. **Move a camera in the fixed steps** (a behaviour's `OnFixedUpdate`, a world subsystem's
   `FixedUpdate`) when it follows anything the steps move. The engine draws it between its steps. A
   camera moved in `Update` is drawn where it was put, every frame.
2. **Move it in one place.** One moved both in the steps and in `Update` is drawn between its steps
   on some frames and where `Update` put it on others: it jitters on screens faster than the steps,
   though it looks fine at 60 frames a second. The engine warns once, naming the entity moved both
   ways (which can be a parent of the camera), and `camera.info` gives each camera's `rhythm`
   (`steps`, `update`, `mixed`). The same holds for `OrthoSize`.
3. **Smooth with time, not frames.** `Maths::ExpDecay(current, target, decay, dt)` and
   `Maths::SmoothDamp(current, target, velocity, smoothTime, dt)` (`Core/Maths/Maths.h`) give the same
   result at any frame rate: one call over `dt` is two over `dt / 2`. `Lerp(current, target, k)` once
   a frame does not: at 144 frames a second it covers 77 units of 100 in a second where 60 frames
   cover 45 (k = 0.01).
4. **Say a jump.** After putting an entity somewhere else (a respawn, a door),
   `TransformInterpolation::Teleport(entity)` draws it there at once instead of sliding it across the
   level over a step; after placing the cameras, `CameraManager::Cut(world)` frames them where they
   are and marks the frame as a cut. A body that physics or the mover moves, placed in `Update`, is
   drawn where it was put without either. The world is drawn a step behind, so on a frame with no
   step after a cut the new place shows again.
5. **Only the camera manager sets a playing world's view.** Read it with `World::GetCameraView`.
   Something that shows another camera (the editor's Camera Preview) names it
   (`IEngine::SubmitCameraView`): the renderer frames it as the game would.
6. **Keep the view a number.** A position or `OrthoSize` that is not a number (a division by zero in
   a follow script) is never drawn: the last good view is, with an error naming the camera. An
   `OrthoSize` outside 1 to 100000 is drawn clamped, with a warning.
7. **Make it checkable.** A camera feature reports its state in `camera.info`, and adds a level and
   its rows to the camera matrix (below), set up from the level's first step: a path is compared only
   for a run that is the same from the start.

## Recipes

### A camera that eases after a target

In the steps, with `SmoothDamp` (or `ExpDecay` for a plain catch-up). `FollowProbe` in
`TestWorld/Source/TestWorld/Probes` does this with its `SmoothTime`; `TestWorld/Tests/CameraSmooth.json`
checks it draws one path at every frame rate.

```cpp
#include "Core/Maths/Maths.h"
#include "Engine/Registries/AutoRegistration.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Components/TransformComponent.h"

class CameraFollow : public Opaax::Behaviour
{
    OPAAX_OBJECT(CameraFollow, Opaax::Behaviour)

public:
    Opaax::OpaaxString Target;
    Opaax::Vector2F    Offset     = { 0.f, 40.f };
    float              SmoothTime = 0.3f;   // seconds to settle on the target

    OPAAX_PROPERTIES(CameraFollow, OPAAX_PROP(Target), OPAAX_PROP(Offset), OPAAX_PROP(SmoothTime))

    void OnStart() override { m_Target = FindEntity(Target); }

    void OnFixedUpdate(const float InFixedDeltaTime) override
    {
        if (!m_Target.IsValid())
        {
            return;
        }

        Opaax::Vector2F&      lAt   = Get<Opaax::TransformComponent>().Position;
        const Opaax::Vector2F lGoal = m_Target.Get<Opaax::TransformComponent>().Position + Offset;
        lAt = Opaax::Maths::SmoothDamp(lAt, lGoal, m_Velocity, SmoothTime, InFixedDeltaTime);
    }

private:
    Opaax::Entity   m_Target;
    Opaax::Vector2F m_Velocity = { 0.f, 0.f };
};
OPAAX_REGISTER_BEHAVIOUR(CameraFollow);
```

A zoom is the same: change `OrthoSize` in the steps, toward a target size with `ExpDecay`.

### A shake

A shake is made every frame, so it goes in `Update`, and rule 2 keeps it off an entity moved in the
steps. Split the camera: a parent entity follows in the steps (`CameraFollow` above, without a
`Camera`), and the camera under it is offset in `Update`. Each entity moves one way, so neither
jitters, and the offset is drawn as it is, on top of the follow drawn between its steps. Make the
offset a function of time (the trauma it decays by `InDeltaTime`, a noise or a wave of the time
elapsed), never of the frame count.

### A respawn

```cpp
Runner.Get<Opaax::TransformComponent>().Position = SpawnPoint;
Opaax::TransformInterpolation::Teleport(Runner);
Camera.Get<Opaax::TransformComponent>().Position = SpawnPoint + Offset;
Opaax::CameraManager::Cut(GetWorld());
```

### Zooming at the cursor

`ZoomAround(view, newSize, viewportPx, anchorPx)` (`Renderer/CameraView.h`) gives the view zoomed
with the world point under the anchor kept there. The editor's wheel zoom is built on it.

## Settings

In `Engine.config` (the Config panel):

| Setting | Does |
|---|---|
| `Time.FixedRate` | Fixed steps a second (60). The camera is drawn between them at any screen rate |
| `Time.bSnapToRefresh` | With vsync, frame times are counted in whole refreshes of the screen, so a steady screen gives steady motion (on by default) |
| `Time.MaxStepsPerFrame` | Steps a frame may run (8): a longer stall slows the game for that frame instead of making every frame longer |
| `Render.bInterpolation` | Draw between the steps (on). Off, everything is drawn where it is, a step at a time |
| `Window.bVSync` | Wait for the screen's refresh |

## Seeing it

- **`camera.info`** ([Automation.md](Automation.md)): the view, the frame clock (the refresh frames
  are snapped to, dropped steps), each camera's framed view, blend and rhythm, the blends under way,
  and the last frame's steps and alpha.
- **Traces:** `camera.trace.start` and `camera.trace.stop` record every frame's time, view and
  followed entity into a JSON and a CSV file and measure them: how far the followed entity moves on
  screen (`targetDrift`), how unevenly the view moves (`motionJitter`), time running back, moves
  while paused, zoom turning back, how much a camera switch jumped (`switchJump`: a cut's distance,
  nothing for a blend). `camera.trace.compare` checks that a run at another frame rate draws the
  same path; a view that curves within a step (a blend) is placed against a 720 Hz run.
- **Frame pacing:** `--frame-pacing 144` (or `time.pace`) plays a level with the frame times of a
  144 Hz screen on any machine, steady (`144`), noisy (`60,jitter=0.25`) or stalling
  (`144,hitch=0.1@2`), the same frames every run.
- **The camera matrix:** `TestWorld/Tests/Camera*.json` play the Following levels at 30 to 240 frames
  a second, steady, noisy and stalling, at fixed rates of 30, 60 and 120, far from the origin,
  smoothed, through a camera switch, a blended hand-over, a pause and a respawn.
  `TestWorld/EditorTests/Camera*.json` play them in the editor at 60, 144 and 240.
- **In the editor:** **Panels > Camera Stats** shows the live camera and how it moves, the blend
  under way, the frame's time, steps and alpha, the frame clock, and a graph of the selected entity's
  drift on screen; **Camera Preview** shows what the selected camera frames; `editor.camera*`,
  `editor.viewport` and `editor.camera.preview` drive and read them from scripts.
