# Gameplay tags

A gameplay tag is a dotted name, from the most general: `UI.Layer.Menu`, `Audio.Bus.SFX.Footsteps`,
`Faction.Player`. Systems name their data by tags (the UI's layers, the sound buses, an entity's
`TagsComponent`), and a tag matches its parents: `Faction.Player` has `Faction`. As in Unreal, the tags
a project uses are registered: code declares the engine's and a game's fixed ones, and the project lists
its own in `Configs/Tags.config`. The editor's pickers offer the registered tags, so a typo shows.

```cpp
const Opaax::OpaaxTag lPause("UI.Layer.Menu.Pause");    // interns its text: keep one used every frame
lPause.MatchesTag(Opaax::OpaaxTag("UI.Layer.Menu"));    // true: a parent
lPause == Opaax::OpaaxTag("UI.Layer.Menu");             // false: the same tag only
```

`OpaaxTag` (`Core/Tag/OpaaxTag.h`) is one tag; `OpaaxTagContainer` a set of them, saved as an array of
strings ([AssetFormats.md](AssetFormats.md)). A text that is not a tag (a space, an empty name between
dots) reads as no tag.

## Registering a tag

**In code**, one line in any source file of a module, as a component or a class is registered:

```cpp
OPAAX_REGISTER_TAG("UI.Layer.Popup", "Pop-ups, over the menus");
```

The engine declares its own in `Engine/Tags/EngineTags.cpp`:

| Tag | For |
|---|---|
| `UI.Layer.Hud`, `UI.Layer.Menu`, `UI.Layer.Debug`, `UI.Layer.Loading` | The UI's default layers ([UI.md](UI.md#layers)) |
| `Audio.Bus` | The master bus |
| `Audio.Bus.Music`, `.SFX`, `.Dialogue`, `.Ambience`, `.UI` | The default buses under it ([Customizing.md](Customizing.md#sound)) |

**In the project**, `Configs/Tags.config` lists a game's tags, each with what it is for. The editor's
Config panel edits it (Tags), and the engine reads it again whenever it changes:

```json
{ "Tags": [ { "Tag": "UI.Layer.Menu.Pause", "Comment": "The pause menu, over the menus' own screens" },
            { "Tag": "Audio.Bus.SFX.Footsteps", "Comment": "Steps, turned down apart from the other sounds" } ] }
```

Registering a tag registers its parents: `UI.Layer.Menu.Pause` brings `UI.Layer.Menu`, `UI.Layer` and
`UI`. A tag listed in both places stays the code's.

## A tag that is not registered

It still works: a layer, a bus or an entity's tag named by an unregistered tag behaves as a registered
one would. Only the warnings differ:

- The UI warns once per listed layer that is not registered, the audio once per short bus name
  ("SFX" plays in `Audio.Bus.SFX`, with a warning to resave the file).
- The editor draws an unregistered tag in a tag field with a warning and a **Register in Tags.config**
  button, which adds it to the project's list.

## Tag fields in the editor

A tag field (`OpaaxTag`, or an `OpaaxTagContainer` of them) is a combo of the registered tags, each with
its comment. A field that only takes tags under one root says so with `SetTagRoot`, and its combo then
lists only those:

```cpp
OPAAX_PROPERTIES(MyAmbience, OPAAX_PROP(Bus).SetTagRoot("Audio.Bus"))
```

Set on a group or a list, the root reaches the fields inside (the engine config's `UI.Layers` and
`Audio.Buses` lists do so).

## Names written before tags

Layers and buses were short names before they were tags. Files that name them so still load:

- A UI layer `"Menu"` (in `Engine.config` or a `MountAsset` call) reads as `UI.Layer.Menu`.
- A bus `"Music"` reads as `Audio.Bus.Music`, `"Master"` as `Audio.Bus`, and a bus listed with a
  `Parent` is folded into its name: `"Footsteps"` under `"SFX"` is `Audio.Bus.SFX.Footsteps`.
- A player's settings file that sets `"Music"` sets `Audio.Bus.Music`: an item of a named list is found
  by its exact name, else by its last names ([Settings.md](Settings.md)).

`WithRoot(root, tag)` (`OpaaxTag.h`) is how: a tag already under the root is kept, any other is put under
it. Resave a file to write its full tags.

## From automation

`tags.list` answers the registered tags, sorted, each with its `source` (`Native`, `Config` or
`Implied`, a parent) and `comment`; `{"root": "UI.Layer"}` keeps those under a root, and
`{"tag": "UI.Layer.Popup"}` answers whether that one is `registered` ([Automation.md](Automation.md)).
