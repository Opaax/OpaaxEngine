# The player's settings

What a player chose in a settings menu: the window, the sound, a game's own options. They go over the
project's configs, the engine reads them at once, and they are kept between launches in the player's
own folder, beside the saves.

## From a settings menu

```cpp
Opaax::IUserSettings& lSettings = GetUserSettings();    // a behaviour's; GetContext().Settings in a game subsystem

lSettings.SetMasterVolume(0.5f);                        // heard at once
lSettings.SetBusVolume(OPAAX_ID("Music"), 0.25f);
lSettings.SetVSync(false);
lSettings.SetWindowMode(Opaax::EWindowMode::Fullscreen);
lSettings.SetResolution(1920, 1080);
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

The volumes and vsync change at once. The window's size and mode are read when it opens.

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
            }
        }
    },
    "Version": 1
}
```

- A bus is set by its name, so the list of buses stays the project's: a bus the project adds later is
  there for this player too.
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
  plays at it. The feature tests share one player's folder: a test that saves settings deletes them at
  its end (`save.clear` with `slot: Settings`).

## How it is built

`IUserSettings` (`Application/Services/IUserSettings.h`) is an app service beside `ISaveStorage`: the
window opens before any game exists. Each `TConfig` keeps two layers, the project's values and the
player's (`Config::SetPlayerText`); `GetData` is the project's with the player's over them. The
settings put their values over each config, those registered later included
(`IConfigSystem::OnConfigAdded`). [Architecture.md](Architecture.md#the-players-settings-over-the-projects-configs)
says why.

Not there yet: changing the window's mode and size while a game runs, a frame-rate cap, and the
player's keys.
