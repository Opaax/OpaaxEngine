# The player's settings

What a player chose in a settings menu: the window, the sound, the keys, a game's own options. They go
over the project's configs and contexts, the engine reads them at once, and they are kept between
launches in the player's own folder, beside the saves.

## From a settings menu

```cpp
Opaax::IUserSettings& lSettings = GetUserSettings();    // a behaviour's; GetContext().Settings in a game subsystem

lSettings.SetMasterVolume(0.5f);                        // heard at once
lSettings.SetBusVolume(OPAAX_ID("Music"), 0.25f);
lSettings.SetVSync(false);
lSettings.SetWindowMode(Opaax::EWindowMode::Fullscreen);
lSettings.SetResolution(1920, 1080);                    // the window follows at once
lSettings.SetMaxFrameRate(120);                         // 0 for no cap
lSettings.SetMultisampling(Opaax::EMultisampling::X4);  // the picture changes at once
lSettings.SetFXAA(true);
lSettings.SetRenderScale(0.75f);                        // 0.5 to 2
lSettings.SetConfigValues("Bhop", R"({"MouseSensitivity": 1.5})");    // a game's own config

lSettings.Save();                                       // kept for the next launch
```

What the engine reads, the project's values with the player's over them, is in the engine config:
`GetContext().Config.Audio.MasterVolume` is where a volume slider starts.

- A value the config does not have, or of the wrong type, changes nothing: the setter answers false,
  and `SetConfigValues(config, values, &lWhy)` says why (`Audio.MasterVolum: no such value`).
- A null takes a player's value away, and the project's is read again:
  `SetConfigValues("Engine", R"({"Window": {"Mode": null}})")`. `ResetConfig("Engine")` takes a
  config's away, `Reset()` every one.
- Nothing is kept until `Save()`; `HasUnsavedChanges()` tells a menu whether to ask.
- A game's own options are a config of its own ([Customizing.md](Customizing.md#a-config)): the project
  ships their defaults, the Config panel edits them, and the player's values go over them the same way.

## When they are read

- **A game** reads them as it starts, before its window opens: the window opens at the player's size
  and mode.
- **The editor** does not: its window and its Config panel keep the project's values. Each Play reads
  them from the file, and Stop takes them off.
- The Config panel and `config.set` change the project's values; a player's value still wins over them
  (`config.get` answers both).

Everything changes at once, except the window's title (read when it opens).

## The window and the frame rate

| Setting | What it does |
|---|---|
| `Window.Mode` `Windowed` | A window of `Width` x `Height`, back where it was before it left. |
| `Window.Mode` `Borderless` | A window without borders over the whole screen it is on, at the screen's own resolution: no mode change, and switching to another program leaves it where it is. |
| `Window.Mode` `Fullscreen` | The screen itself, at its mode closest to `Width` x `Height`, at that mode's highest refresh rate. |
| `Window.Width`, `Height` | The window's size; in fullscreen, the resolution asked. |
| `Window.bVSync` | Waits for the screen's refresh: no tearing. |
| `Window.MaxFrameRate` | The most frames a second; 0 for no cap. |

- **A game's window follows** the mode and size the moment they change, on the screen holding its
  centre. The editor's window never does: it keeps the size the person gave it (`window.info` answers
  `follows`).
- **The resolutions to offer**: `GetDisplayModes()` lists the fullscreen modes of the window's screen,
  smallest first, each with its refresh rate. A size the screen does not have gets the nearest it has.
- **The frame cap holds while a game runs**: in the editor, during Play only, and never in a paced run
  (`--frame-pacing`). With vsync, a cap that divides the screen's rate (120 or 80 on 240 Hz, 30 on
  60 Hz) shows each frame for the same whole refreshes, by the swap interval: no wait, no uneven
  frames. Any other cap (100 on 240 Hz) waits for a steady beat, and vsync shows each frame for 2 or 3
  refreshes around it; so does every cap when vsync is off. A late frame is shown at once, and the next
  counts from it: no burst of short frames to catch up.
- A driver may show frames at every refresh whatever the swap interval, and a variable-refresh screen
  (G-Sync, FreeSync) shows them at once: the cap sees its frames come early and waits for them
  instead, with an Info line in the log.

## The picture

Three of the renderer's settings are the player's to choose: they trade the GPU's time for smoother
edges. They go over `Renderer.config`, and the picture changes at once. What each one cures and costs
is in [Customizing.md](Customizing.md#multisampling).

| Setting | Setter | What it does |
|---|---|---|
| `Multisampling` `Off`, `X2`, `X4`, `X8` | `SetMultisampling` | Samples per pixel of the world: smooth edges on quads, tiles and shapes. A driver that has fewer uses its most. |
| `bFXAA` | `SetFXAA` | Smooths every edge of the finished world picture, inside textures too. The UI stays sharp. |
| `RenderScale` 0.5 to 2 | `SetRenderScale` | The world's pixels against the view's: below 1 for a slow GPU, above 1 for smoother edges at a high price. A scale outside the range is refused. |

- They change the world only: the UI and the overlays over the world keep the view's own pixels.
- `TextureFilter` has no setter: how the art is read is the project's choice (art direction), not
  a quality setting.
- `renderer.info` answers what the renderer uses (`sceneSettings`, and per world view `samples`),
  `config.get` with `config: Renderer` the project's and the player's values.

## The keys

A controls menu rebinds the keys of the game's mapping contexts ([AssetFormats.md](AssetFormats.md#input-inputactionasset-inputmappingcontextasset)).
A mapping is named by its context's asset, its action and the key the asset gives it (`InputKeySlot`):
the key the asset gives tells an action's keys apart (Move's W and S), and stays the same whatever the
player binds.

```cpp
Opaax::InputMappingSubsystem& lInput = *GetContext().Actions;
lInput.RegisterContextAsset("Input/Controls.opaaxasset");     // known before it is added: listed, rebindable

for (const Opaax::InputKeyMapping& lMapping : lInput.GetKeyMappings("Input/Controls.opaaxasset"))
{
    // lMapping.Slot.Action, lMapping.Key (the player's), lMapping.IsRebound()
}

const Opaax::InputKeySlot lJump{ "Input/Controls.opaaxasset", OPAAX_ID("Jump"), Opaax::EKeyCode::Space };
const Opaax::InputRebindResult lResult = lInput.Rebind(lJump, Opaax::EKeyCode::E, Opaax::EKeyConflictPolicy::Swap);
// lResult.Result: Rebound, Conflict (lResult.Others: who has the key), Reserved, Locked, RefusedByGame (lResult.Why)...

lInput.ResetKeys("Input/Controls.opaaxasset");               // the asset's keys again
GetUserSettings().Save();                                     // kept with the other settings
```

- **At once:** every context added from that asset takes the key the moment it is rebound, and one
  added later starts with it. `EKeyCode::None` clears a mapping: it drives nothing.
- **Conflicts** are within the mapping's own context: another context (a vehicle's) is active at
  other times, or above it. `Refuse` changes nothing and lists the mappings on the key, for the menu to
  ask; `Swap` gives the other mapping the key the rebound one had; `Replace` clears it.
- **Locked contexts:** an asset with `bRebindable` off (a menu's Escape) is not the player's to
  change (`Locked`), and once known its keys are never given to another mapping (`Reserved`),
  whatever the policy.
- **The game's say:** `SetKeyCheck` takes a function that answers false, with the reason, for a key
  the game keeps or an action must not have; it is asked for every key a rebind gives, the one swapped
  included.
- `OnKeysChanged` tells a menu to redraw: a rebind, a reset, or the settings read again.
- The keys are read from the settings as the game starts, and again whenever the section changes
  there (`Load`, `Reset`, `settings.reset`).
- `GetContextLayouts` lists every context asset known (added or registered) with its mappings and
  whether it is rebindable; `IsContextAssetAdded` says whether one is in use.
- **From outside**, a test or an agent does what a controls menu does: `input.keys` lists the keys,
  `input.rebind`, `input.resetKeys` and `input.capture` change them ([Automation.md](Automation.md#commands)).
  ```json
  { "command": "input.rebind", "params": { "action": "Jump", "key": "E", "policy": "Swap" } }
  ```
  A refused rebind fails with why and what to do instead: `Conflict: E is taken by Use on E in
  Input/Controls.opaaxasset: {"policy": "Swap"} gives it the key Jump had, "Replace" clears it`.

### Press a key

```cpp
lInput.OnKeyCaptured().AddMember(this, &ControlsMenu::HandleKeyCaptured);   // RemoveAll(this) when it goes
lInput.BeginKeyCapture();                                                    // "Press a key for Jump..."

void ControlsMenu::HandleKeyCaptured(const Opaax::EKeyCode InKey)
{
    if (InKey != Opaax::EKeyCode::None)                                      // None: cancelled
    {
        m_LastResult = GetContext().Actions->Rebind(m_Slot, InKey);          // Conflict: ask, then Swap
    }
}
```

- The next key or mouse button pressed is taken, a wheel notch included. A key held when the capture
  begins (the click or the Enter that asked) counts only once released and pressed again.
- **Escape cancels** (`EKeyCode::None`), and so does `CancelKeyCapture`.
- While it waits, no key drives an action, and **the UI gets no input**: the click that gives
  `Mouse_Left` presses no button. The key taken, or the Escape that cancelled, drives nothing until
  released, so binding Jump to Space does not jump.
- `IsCapturingKey` says it waits; `IsHoldingInput` also covers the key still held.
  `GetLastCapturedKey` is the key the last capture took, for a menu that reads it instead of listening.

## The file

`<UserDir>/SaveGames/Settings.opaaxsave` ([Saving.md](Saving.md#where-the-files-go)), laid out as a
save, holding only what the player changed, shaped like each config's file:

```json
{
    "Class": "UserSettings",
    "ClassVersion": 1,
    "Properties": {
        "Configs": {
            "Engine": {
                "Audio": { "Buses": { "Music": { "Volume": 0.25 } }, "MasterVolume": 0.5 },
                "Window": { "Mode": "Fullscreen" }
            },
            "Renderer": { "Multisampling": "X4", "RenderScale": 0.75 }
        },
        "Keys": {
            "Input/Controls.opaaxasset": { "Jump": { "Space": "E" }, "Move": { "W": "Up", "S": "None" } }
        }
    },
    "Version": 1
}
```

- A bus is set by its name, so the list of buses stays the project's: a bus the project adds later is
  there for this player too.
- `Keys` holds each rebound mapping: context, action, the asset's key, then the player's. A key kept
  for a mapping its asset no longer has, or for a locked context, is dropped with a warning once the
  asset is read; one for a context not read yet waits for it.
- A value the config does not have any more is dropped as the file is read, with a warning; the rest is
  read.
- A file of a newer game, or one that holds another class, is not read and never written over. A file
  that is not one at all (cut short) is replaced by the next save.
- The slot is the settings': a game's saves cannot use it (`InvalidSlot`), and
  `SaveGameSubsystem::ListSlots` leaves it out.

## Testing

- `Engine/Tests/Application/IUserSettingsTests.cpp`: a project's configs in a temp folder with the
  settings over them; `Relaunch` makes them again as the next launch does.
- A feature test sets them with `settings.set`, `settings.save`, `settings.load` and `settings.reset`,
  and reads them with `settings.get` and `config.get` ([Automation.md](Automation.md)).
  `TestWorld/Tests/Settings.json` sets the volume from gameplay, as a menu does, and checks the mixer
  plays at it; it sets the picture's three settings the same way (a render scale of 4 refused) and
  checks the renderer uses them (`renderer.info`), after a reset and a reload too. `TestWorld/Tests/Window.json` resizes the window, makes it borderless and back, and caps
  the frames at 30 (`window.info` answers the window, its screen's modes and the cap). The feature
  tests share one player's folder: a test that saves settings deletes them at its end (`save.clear`
  with `slot: Settings`).
- `Engine/Tests/Engine/FrameLimiterTests.cpp` plays the cap on simulated screens (240 Hz rows),
  one that ignores the swap interval included; `IWindowManagerTests.cpp` the display modes chosen.
- `Engine/Tests/Input/InputRebindingTests.cpp` checks the conflicts, the locked keys, the game's check
  and the section's JSON; `InputAssetTests.cpp` the subsystem over real files and settings, the next
  launch included. `TestWorld/Tests/Keys.json` rebinds Fire from gameplay (`RebindProbe`), presses
  the new key, saves, resets and reads the file again, then does it with the `input.*` commands, a
  capture included. `Engine/Tests/Automation/InputAutomationTests.cpp` checks those commands' answers
  and refusals (`InputTestGame.h` is the game the input suites share). `InputMappingTests.cpp` waits for keys;
  `TestWorld/Tests/UI.json` clicks a button while a capture waits: the click is the key, the button
  is not pressed, and Escape cancels the next one.

## How it is built

`IUserSettings` (`Application/Services/IUserSettings.h`) is an app service beside `ISaveStorage`: the
window opens before any game exists. Each `TConfig` keeps two layers, the project's values and the
player's (`Config::SetPlayerText`); `GetData` is the project's with the player's over them. The
settings put their values over each config, those registered later included
(`IConfigSystem::OnConfigAdded`). [Architecture.md](Architecture.md#the-players-settings-over-the-projects-configs)
says why.

The window follows the engine config through `WindowManager::FollowEngineConfig`, called by a game
app only, and moves only when the mode or the size changed: a window the player dragged to another
size is not snapped back by a volume change. The frame cap is `FrameLimiter` (`Engine/FrameLimiter.h`),
pure: told the time, it says how long to wait before the present, and which swap interval holds the
frames; the engine waits with `IPlatform::SleepPrecise` (a high-resolution timer on Windows, the last
millisecond spun, the end counted in whole nanoseconds of the steady clock), inside the profiler's
`FrameCap` scope.

The keys are `InputRebinds` (`Input/Mapping/InputRebinding.h`), pure: told the layouts of the
known contexts, it rebinds and finds the conflicts. `InputMappingSubsystem` reads the context assets,
rebuilds the bindings of every context added from one (`InputActionEvaluator::SetContextBindings`,
in place, so the actions' states carry on), and keeps the keys in the settings' `Keys` section
(`IUserSettings::OnSectionChanged` says when another hand changed it). As Unreal's Enhanced Input
keeps the player's mappable keys over the mapping contexts.

A capture runs in `InputMappingSubsystem::Update`, before the actions are read: it masks every key
while it waits (`ConsumeThisFrame`'s mask), and `UISubsystem` routes nothing while
`IsHoldingInput` holds, as if in `GameOnly` mode.
