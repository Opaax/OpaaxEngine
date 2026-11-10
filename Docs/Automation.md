# Driving a game or the editor from outside

Every Opaax app (a game, its editor) can be driven by JSON requests: from a script in CI, from a
test, or by an AI agent. The commands are the same everywhere; the editor adds its own.

## Starting

| Flag | What happens |
|---|---|
| `--exec <script.json>` | Runs the script's requests in order, then closes the app. Each answer is written to `<script>.out.json` as it comes (`--exec-out <file>` to choose). The exit code is 1 when a request failed, or never ran because the app closed first. |
| `--automation <folder>` | Keeps the app open and runs every `<name>.request.json` dropped in the folder (in name order), answering in `<name>.response.json`. `app.quit` closes it. |
| `--upgrade-assets` | The editor only: before it opens, converts the asset files of older formats (`.opaaxsheet`, `.opaaxclip`...) in the project and the engine's content into `.opaaxasset` files, and rewrites the paths that named them. The log lists what changed. With `--exec` and a script that sends `app.quit`, it converts and closes ([Objects.md](Objects.md)). |
| `--frame-pacing <pacing>` | The frames get the time of a screen rate instead of the clock's, from the first frame: `144`, `60,jitter=0.25` (each frame up to a quarter of a frame early or late, never drifting off the rate), `60,hitch=0.1@2` (a 0.1 s stall every 2 s), `60,seed=3` (another jitter), `deltas=0.016/0.033` (repeated). The same pacing gives the same frames every run, on any machine, and VSync is off so the run goes as fast as it can. A pacing that does not read fails the run. |
| `--user-dir <folder>` | The player's own folder, where the saves, the log and the crash reports go (`IPaths::UserDir`), instead of `<ProjectRoot>/Save` in a dev build or the OS's user folder in a ship build. A relative folder is from the workspace. The feature tests point it at `<build>/<Game>Results/User`, so their saves never touch yours and their log is beside their answers. |

The two can be combined. For the inbox, write a request under another name first and rename it to
`.request.json`, so the app never reads half a file; number the names (`001`, `002`...) to keep the
order.

A script owns the input: while it runs, the window's own keys, mouse and focus changes are ignored,
so a pointer resting on the window cannot move the scripted one. An inbox session shares the input
with whoever uses the window.

## Requests and answers

```json
{ "id": "jump", "command": "input.key", "params": { "key": "Space", "action": "tap" } }
```

```json
{ "id": "jump", "command": "input.key", "ok": true, "result": {} }
{ "id": "hero", "command": "entity.get", "ok": false, "error": "no entity named 'Hero'" }
```

A script is an array of requests, or `{"requests": [...]}`. A request without an id gets its
position. In the inbox, the file's name is the id.

To run the same requests several times, write them once in a repeat:
`{ "repeat": [{ "tag": "60", "pacing": "60" }, { "tag": "144", "pacing": "144" }], "requests": [...] }`
stands for its requests once per entry, each `{tag}` in their strings replaced by the entry's value;
a string that is only `{name}` takes the value itself, so a number stays a number. Repeats can nest.
`TestWorld/Tests/CameraFollowing.json` plays one level at eleven frame rates this way.

Frame rates and timing are driven too: `time.pace` (or `--frame-pacing`) gives the frames the time of
a screen rate, steady, noisy or stalling, the same frames every run, and `config.set` changes a
setting for the run (`Time.FixedRate`, `Window.bVSync`, `Render.bInterpolation`). Prefer pacing in
tests of anything that moves: a test then sees what a 144 Hz screen shows on any machine.

To check that a command refuses something, add `"expectFailure": true`: the request passes when the
command fails (its answer is `{"refused": "why"}`) and fails when the command succeeds. An unknown
command, or a request the app closed before running, still fails.

To check an answer, add `"expect"` with what it should hold:
`{ "command": "world.info", "expect": { "mode": "Play" } }`. Objects match by the keys given (others
are ignored), arrays item by item, numbers within 1e-4; the request fails, saying where the answer
differs, when it does not hold them.

## Frames

Requests run at the start of a frame, one after another, until one holds the queue:
`frames.wait` lets frames run, `world.wait` lets game time pass, `screenshot` waits until the frame
is drawn, a tapped key stays down for its frames. Everything else is immediate: let the game run
before checking what a change did. Prefer `world.wait` in tests: frames go by faster or slower
depending on the machine, game time does not.

## Commands

`commands.list` lists every command with its help. An `entity` is an entity's id (from
`entity.list`) or its name when no other entity has it. Component types and fields are the ones
saved in `.opaaxmap` files.

| Command | Params | Does |
|---|---|---|
| `app.info` | | Frame number and the folders: workspace, engine, project, assets, save, user (where the saves go) |
| `app.quit` | | Closes the app at the end of the frame |
| `frames.wait` | `count` | Lets frames run before the next request |
| `world.wait` | `seconds` | Lets the playing world's clock advance (game time, whatever the frame rate) |
| `config.get` | `config` | A config's values (`Engine`, `Renderer`...): `values` what the engine reads, `project` its file's values and `player` the player's settings over them |
| `config.set` | `config`, `values` | Merges `values`, shaped like the file (`{"Time": {"FixedRate": 30}}`), into the config's project values for this run, without saving, and applies them as the Config panel does. A value the config does not have, or of the wrong type, is refused with the names it has there; one the player set still wins (see `player` in the answer) |
| `camera.info` | | How the active world is framed: its view (`x`, `y`, `size`, `rotation` in degrees), the fixed rate, the pacing, the trace recording, the frame clock (`refreshHz` its frames are snapped to, `snapping`, `maxSteps`, `droppedSteps`), every camera (`name`, `id`, `priority`, position, `size`, `blendIn`, `blendCurve`, `framed`: the view it gives on that frame, `recorded`: framed between its steps, `rhythm` over the last 60 steps: `still`, `steps`, `update`, or `mixed` when one entity moves it both in the steps and in Update; `unknown` before 60 steps; `stages`: its rig in the order it runs, each `class` and `stage` (`Body`, `Size`, `Constraint`) with what it holds: a follow's `target`, `inDeadZone` and `lead`, a confiner's or a group's `bounds` (`center`, `halfExtent`), a zone stage's `zone` and `blend`, a speed zoom's `size`), and in a Play world the last frame: the camera that framed it (`live`: `name`, `id`, `rhythm`, `stages`, `count`, `tied`, `blends` under way and the newest `blend`: `from`, the camera the view leaves, null for a view held where a destroyed camera was, and its `weight` toward the live camera, and `unmodified`, the view before the modifiers), the `modifiers` that change it (`class`, `priority`, in the order they run), the `shakes` under way (`id`, `shake` its asset, `scale`, `seed`, `start` and `stopped` in game time, `source` its entity's name, a trauma source's `trauma` at the frame) and its `delta`, `steps`, `alpha`, `interpolated`, `stepDelta`, `gameTime` |
| `camera.shake` | `path`, `scale`, `seed`, `stop`, `stopAll` | Plays a camera shake asset on the active Play world from the next step, times `scale` (1), its noise picked by `seed` (0): answers its `id`. `stop` (an id) fades one out from the next step, `stopAll` every one |
| `camera.trauma` | `entity`, `amount` | Raises a trauma source's trauma (a CameraShakeSource with `bUseTrauma`) by `amount` (negative lowers it), kept in 0 to 1, from the next step; answers its `trauma` |
| `camera.trace.start` | `name`, `target`, `heightPx` | Records every frame of the playing world from the next one: its delta, steps and alpha, the game time drawn, the camera that framed it, the view, and where `target` (an entity's name) is drawn. Pixels are of a view `heightPx` tall (the window's by default) |
| `camera.trace.stop` | `limits` | Ends the trace and writes `<name>.camera.json` and `<name>.camera.csv` in the working folder (a spreadsheet opens the CSV). Answers the summary: `targetDrift` (the target's largest move on screen between two frames, 0 for a camera locked on it), `motionJitter` (the largest change of the view's move between two frames, 0 for a steady pan at a steady rate), `largestMove`, `timeReversals`, `stillMoves` (the view moving while game time stands still), `zoomReversals`, `cameraSwitches`, `switchJump` (how much a switch changed the view's move from the frame before: the distance between the cameras for a cut, near 0 for a blend), `cuts` (frames after `CameraManager::Cut`; a switch's or a cut's jump is not counted as a move), each with its frame, and `mostSteps`, `fewestSteps` and `stepsSpread` (the fixed steps a frame took: at a screen rate the fixed rate divides, one spread of 0). `limits` (`{"targetDrift": 0.01}`) fails the request past any of them |
| `camera.trace.compare` | `reference`, `trace`, `limits` | How far `trace` is from where `reference` says its camera was at each of its game times: `position` and `size` in world units, `rotation` in degrees. With a reference at a steady rate of the fixed rate or a multiple, one level played at any pacing must match it: the screen's rate only chooses which moments of the camera's path are shown. A frame the reference did not draw is placed between its two frames around it: at the fixed rate, a step's ends, right for anything drawn between its steps; a view curving within a step (a blend, a shake) needs a reference drawing within it (720 Hz places 60, 144 and 240 exactly) |
| `window.info` | | The main window ([Settings.md](Settings.md#the-window-and-the-frame-rate)): `mode`, `width`, `height`, `refreshRate` (its screen's), `decorated`, `follows` (whether it follows `Window.Mode`, `Width` and `Height` at once: a game's does, the editor's not), `modes` (its screen's fullscreen modes, smallest first: `width`, `height`, `refreshRate`) and `frameCap`: `maxFrameRate` (the cap holding the frames: 0 with none, no game running or paced), `heldRefreshes` (each frame's by the swap interval; 0 on the beat) and `unheld` (the screen ignored the swap interval) |
| `time.pace` | `pacing` | From the next frame, the frames get the time of `pacing`, written as `--frame-pacing` takes it; `off` gives the clock back. Answers the pacing as read |
| `screenshot` | `path` | Saves the frame as a PNG (in the editor: the whole editor) |
| `input.key` | `key`, `action`, `frames` | `press`, `release` or `tap` a key (`Space`, `A`, `Mouse_Left`...) as if from the keyboard; the wheel's codes are refused (`input.scroll`) |
| `input.mouse` | `x`, `y` | Moves the game's pointer (pixels of the game view) |
| `input.scroll` | `y`, `x` | Turns the mouse wheel by `y` notches (1 up, -1 down) and `x` sideways in one frame; the next request runs after that frame |
| `input.keys` | `context` | The running game's keys, as a controls menu lists them ([Settings.md](Settings.md#the-keys)): each context asset it added or registered (`context`, `rebindable`, `added`: in use) with its `keys` (`action`, `default` the asset's key, `key` the player's, `rebound`), and the capture: `capturing`, `captured` (the key the last one took, `None` when cancelled, null before any). `context` lists only that one |
| `input.rebind` | `action`, `context`, `default`, `key`, `policy` | Gives a mapping the `key` (`None` clears it) as a controls menu does: kept in the player's settings, not saved. `action` alone names a mapping only one known context has; `context` and `default` (the asset's key: `Move`'s `W` or `S`) tell several apart. `policy` settles a conflict within the context: `Refuse` (the default), `Swap` or `Replace`. Answers `result`, the mapping, its `key` and the `others` a swap or a replace changed. A refusal fails, its message starting with why (`Conflict`, `Reserved`, `Locked`, `InvalidKey`, `RefusedByGame`) |
| `input.resetKeys` | `context`, `action`, `default` | Gives every mapping its asset's key again; `context` only that context's, `action` (named as `input.rebind` takes it) only that mapping. Answers as `input.keys` |
| `input.capture` | `cancel` | Waits for the next key the player presses, as a menu's "press a key" does: `input.key` presses one, `input.keys` says what it took, and it rebinds nothing. `cancel: true` ends the one under way. Answers `capturing`, `captured` |
| `ui.list` | `name` | The game UI's widgets: name, type, visible, centre and size in game-view pixels |
| `ui.click` | `widget`, `frames` | Clicks a widget of the game's UI by name: the pointer moves to its centre, the left button is tapped |
| `world.info` | | The active world: name, mode, paused, entities, camera (its view) |
| `world.pause` | `paused` | Pauses the playing world (`true`) or resumes it (`false`) |
| `world.step` | | Runs one frame of the paused world, with the fixed steps it owes |
| `entity.list` | `name`, `component` | The entities (optionally filtered) with their components |
| `entity.get` | `entity` | One entity with its components' values |
| `component.set` | `entity`, `type`, `value` | Merges `value` into the component (adds it when missing). A field of the wrong type changes nothing |
| `level.play` | `path` | Opens a level (relative to the project's assets) in a new play world |
| `expect.value` | `entity`, `path`, check | Fails unless the value at `path` (`Transform/Position/x`) passes the check |
| `expect.count` | `name`, `component`, check | Fails unless the number of matching entities passes the check |
| `expect.entity` | `entity`, `exists` | Fails unless an entity with that name or id exists (or, with `exists: false`, none does) |
| `expect.quit` | `seconds` | Waits for the game to close the app itself (`QuitGame`); fails if it is still open after `seconds` (10) |
| `save.list` | | The player's save slots: `folder`, `slots` (sorted) |
| `save.get` | `slot` | A slot's save as its file holds it: `slot`, `path`, `class`, `classVersion`, `properties`. Fails for a slot without a save, or a file that is not a save this engine reads |
| `save.set` | `slot`, `class`, `properties` | Writes a slot as the game writes a save of `class`: its defaults, `properties` merged. An unknown class, a property the class does not have (the error lists its properties) or a value of the wrong type writes nothing. With `slot`, `text` instead, the file is written as given, unchecked: an old or broken file, for a test |
| `save.clear` | `slot` or `all` | Deletes a slot's save, or every slot's with `all: true`; answers `deleted`. A slot without a save is no failure |
| `settings.get` | | The player's settings ([Settings.md](Settings.md)): `active` (over the configs: in a game always, in the editor while Play runs), `unsaved`, `path` (their file) and `properties`, their values as the file holds them (`{"Configs": {"Engine": ...}}`). What the engine reads is in `config.get` |
| `settings.set` | `config`, `values` | Sets `values`, shaped like the config's file (`{"Audio": {"MasterVolume": 0.5}}`; a bus by its name, `{"Audio": {"Buses": {"Music": {"Volume": 0.5}}}}`), as the player's, as a settings menu does; a null takes one away. Read at once while active; not saved. A value the config does not have, or of the wrong type, refuses them all. Answers as `settings.get` |
| `settings.save` | | Writes the player's settings to their file (the `Settings` slot). Answers as `settings.get` |
| `settings.load` | | Reads them from their file again, in place of the values in memory, as a game does when it starts. Answers as `settings.get` |
| `settings.reset` | `config` | Takes the player's values for `config` away, or every value without it: the project's are read again. Not saved. Answers as `settings.get` |
| `online.info` | | The player's store ([Online.md](Online.md)): `backend`, `reason` (why it runs, or why none does), `available`, `capabilities`, `player`, `overlay`, `pending` requests, the game's `leaderboards`; `profile` for the fake store |
| `online.use` | `backend`, `profile` | Runs another store: `none`, or `fake` with a `profile` (`full`, `steam`, `epic`, `gog`). What was asked is dropped. A real store is chosen at launch only (`--online steam`). Answers as `online.info` |
| `online.leaderboard.upload` | `board`, `score`, `details`, `file` | Uploads a score, keeping the player's best; `file` (text) is attached only when the score was kept. Answers `result`, `scoreChanged`, `rank`, `previousRank` |
| `online.leaderboard.entries` | `board`, `range`, `first`, `last` | A board's entries: `World` (ranks `first` to `last`, 1 to 10), `AroundPlayer` (offsets, -5 to 5), `Friends`. Answers `result`, `entries` (`rank`, `score`, `name`, `id`, `details`, `file`) |
| `online.file.get` | `file` | A file attached to an entry: `result`, `text`, `size` |
| `online.achievements` | `unlock` or `clear` | The player's achievements, after unlocking or clearing one if asked: `result`, `achievements` (`name`, `unlocked`) |
| `online.overlay` | `active` | Opens or closes the fake store's overlay, as the player's key does. Fails on any other store |
| `online.fake.player` | `name`, `friend`, `nameKnown`, `board`, `score` | Adds a player to the fake store, with a score on a board if given: `id`. `nameKnown: false` keeps the name from the store until asked |

A check is any of `equals` (numbers within 1e-4; for an object, the fields given), `near` with a
`tolerance` (0.01 by default), `greater`, `less`, `between: [min, max]`. A failed expectation fails
its request, so a script of expectations is a test: its exit code is 1 when one fails. When the app
closes before the script's end, the requests left fail too: a game that quits early cannot pass a
test it never finished.

While a game runs, the `save.*` commands first wait for its background saves
(`SaveGameSubsystem::Flush`), so they read what it asked to write. `TestWorld/Tests/Saves.json` seeds
slots, plays a level that loads and saves in the background, and checks the files.

The `online.*` requests to the store answer once the store has, 10 s at most. What the store said is
a success carrying it (`"result": "Unsupported"`), so a script expects it like any value. Feature
tests run on the fake store (`--online fake`), never a real one; `TestWorld/Tests/Online.json` posts a
run with its ghost, reads the board around the player, pauses on the overlay, then plays the level
again as Epic (`online.use`), where the ghost is not sent.

The editor adds:

| Command | Params | Does |
|---|---|---|
| `editor.play`, `editor.stop`, `editor.pause`, `editor.step` | | Play In Editor |
| `level.play` | `path`, `discard` | Opens a level, then plays it in the editor |
| `editor.undo`, `editor.redo` | | Undo and redo, as the Edit menu does |
| `editor.command` | `tag` | Any editor command without arguments, by tag (`Editor.Command.FocusSelected`) |
| `editor.panel` | `panel`, `visible` | Shows a panel by its window title (`Camera Preview`), or hides it with `visible: false`; an unknown title fails with the list |
| `editor.camera` | `x`, `y` | The editor's camera, which frames the edit world: `x`, `y`, `size`, the `viewport` in pixels, `framing` (false while playing); with a viewport pixel `x`, `y` (from the top left), the world point `under` it |
| `editor.camera.set` | `x`, `y`, `size` | Places the editor's camera (a value left out is kept) |
| `editor.camera.pan` | `dx`, `dy` | Pans as a drag of that many viewport pixels: the content follows the drag |
| `editor.camera.zoom` | `wheel`, `x`, `y` | Zooms by wheel steps (positive zooms in) at a viewport pixel (the centre by default): the world point there stays there; answers it (`under`) and how far it moved (`shift`, 0) |
| `editor.viewport` | | The level viewport's size, the world's `view`, and the selection `outline` drawn on the last frame (its centre and size, world units; null when nothing is outlined) with `fromView`, its centre from the view's: constant for an entity the camera follows |
| `editor.camera.focus` | | Frames the selection, as Focus Selected does; fails while playing or with nothing selected |
| `editor.camera.preview` | | The camera the Camera Preview shows (select one to preview it): `camera` (`name`, `id`), `drawn` (on the last frame: the panel is open), `view` as drawn, the world's view that frame, `live` (it frames the playing world) and `gap` between the two views (0 for the live camera) |
| `level.open`, `map.open` | `path`, `discard` | Opens a level or a map (relative to the project's assets) |
| `level.new` | `path`, `map`, `discard` | Creates a level with one new, empty map (`map`; default `Maps/<level name>.opaaxmap`) and opens it; refuses files that exist |
| `map.new` | `path` | Creates an empty map, adds it to the open level and focuses it: entities created next go there |
| `level.info` | | The open level's file, its maps (`id`, `path`, `persistent`, `focused`, `unsaved`), whether anything is unsaved, and whether its edits can be undone or redone (`canUndo`, `canRedo`) |
| `level.save` | `all` | Saves the level's changed maps; with `all`, rewrites every map of the level and its file in the current format |
| `map.save` | `path` | Saves the focused map; with `path`, writes it to that new file instead, which the level names from then on (the old file stays as last saved) |
| `map.add` | `path` | Adds an existing map to the open level: its entities load. Answers the map's `id` |
| `project.export` | `path` | Exports the game into a folder (its release build and content); answered when done, minutes later |
| `entity.create` | `name`, `map`, `components` | Creates an entity in the focused map, with components `{type: values}`, as one undo step. Every type and value is checked first: a request that fails creates nothing |
| `entity.destroy` | `entity` | Deletes an entity |
| `entity.parent` | `entity`, `parent` | Puts an entity (and its children) under another, keeping its place in the world; no `parent` detaches it |
| `entity.rename` | `entity`, `name` | Renames an entity |
| `entity.select` | `entities` | Selects entities (an empty list clears the selection) |
| `entity.duplicate` | `entities`, `prefab` | Copies entities (default: the selection) and their children beside the originals, as Ctrl+D does: same parents, same maps, the copies named anew (`Crate 1`) and selected. Answers `count` created and the selected copies (`id`, `name`, `parent`, `map`) |
| `entity.copy`, `entity.cut` | `entities`, `prefab` | Puts entities (default: the selection) and their children on the clipboard; `cut` then deletes them. Copying works while playing. Answers `count` |
| `entity.paste` | `map`, `prefab` | Makes new entities of the clipboard's copy in `map` (default: the focused one), as roots where they were copied, named anew and selected. Answers as `entity.duplicate`; fails when the clipboard holds no copied entities |
| `clipboard.get`, `clipboard.set` | `text` | The clipboard's `text` and the `kind` of editor copy it holds (`Entities`, null for other text); `set` puts `text` there as it is |
| `component.add`, `component.remove` | `entity`, `type` | Adds or removes a component |
| `component.set` | `entity`, `type`, `value` | As the engine's, recorded for undo in the edit world |
| `component.copy` | `entity`, `type` | Puts one component's values on the clipboard, as the Inspector's Copy Component does |
| `component.paste` | `entity` | Pastes the clipboard's component on an entity: over its own of that type, or added with the copied values. One undo step; a value the type refuses changes nothing |
| `prefab.create` | `entities`, `path` | Writes entities (with their children) as a prefab, then replaces them with an instance of it |
| `prefab.place` | `path`, `position` | Places an instance of a prefab in the focused map, its first entity at `position` |
| `prefab.variant` | `prefab`, `path`, `entity`, `components` | Writes a variant of a prefab: it inherits everything, except `components` changed on its `entity`. A type or value it refuses writes nothing and leaves the base prefab as it was |
| `ui.new` | `path` | Creates an empty UI canvas (a root panel at the project's reference height); its widgets are written in the file |
| `ui.widget.add` | `type`, `parent` | Adds a widget of `type` (`UIPanel`, `UIText`...) to the canvas open in the UI panel, under the widget named `parent` (the top level by default), named after its type and numbered when taken. Answers its `name` |
| `ui.widget.select` | `name` | Selects a widget of the UI panel's canvas by name (an empty name clears): what `Editor.Command.CopyUIWidget`, `CutUIWidget`, `PasteUIWidget` and `DuplicateUIWidget` act on |
| `ui.widget.list` | | The canvas's widgets in tree order (`name`, `type`, `parent`) and the `selected` one's name |
| `asset.classes` | | The asset classes a project can make: name, label, category, and properties at their defaults |
| `asset.new` | `path`, `class`, `properties` | Creates an asset of a class, every field at its default, then sets `properties`, saves and opens it; refuses a file that exists |
| `asset.open` | `path` | Opens an asset in its window: `path`, `class`, `properties`, `unsaved`, `canUndo`, `canRedo`, `fromSourceFile` (an image and the like: read-only). A class with a document of its own opens there, as a double-click does (a prefab in the Prefab panel): `path`, `class`, `document` |
| `asset.get` | `path` | An asset's class and values: its window's when open (unsaved changes included), else its file's; an image, a font or a sound answers its class (`Texture2D`, `FontFace`, `AudioClip`) |
| `asset.set` | `path`, `properties` | Merges values into an asset (opened first if needed) as one undo step, then its class's fixes apply; not saved. A value of the wrong type changes nothing |
| `asset.undo`, `asset.redo` | `path` | Steps an open asset's own history |
| `asset.duplicate` | `path` | Copies a content file, as saved, beside it under the next free name (`Grunt 1.opaaxasset`), as the Resource Browser's Duplicate does. A map gets its own map id and new entity guids, so it can join a level beside its original. Answers the copy's `path` |
| `asset.copy`, `asset.paste` | `path` | Puts an asset's values on the clipboard, with its class; `paste` gives them to another asset of the same class as one undo step, not saved (another class is refused) |
| `asset.save` | `path` | Writes an open asset and reloads it, so a playing game reads the new values |
| `asset.close` | `path`, `discard` | Closes an asset's window; unsaved changes are refused unless `discard` is true |
| `asset.list` | | The open assets, in the order they were opened: `path`, `class`, `unsaved` |

An asset `path` is relative to the project's assets, `/Engine/...` for the engine's, or absolute.

Edits made by the editor's commands are the same as a user's: undoable, and saved only when asked.

With `"prefab": true`, the entity copy commands act on the prefab open in the Prefab panel (`asset.open`
a `.opaaxprefab`), on its own selection and history (`editor.command` with
`Editor.Command.UndoPrefab` undoes there). A paste that would make the prefab place itself is refused.
A copy is JSON on the system clipboard, `{"opaaxClipboard": "Entities", "version": 1, "data": ...}`:
whole prefab placements stay placements, part of one is copied as plain entities. Every copy gets a
name no other entity has (`Finger 1`), children included, and its `EntityRef` fields that named a
copied entity name the copy. Running these commands replaces whatever was on your clipboard.

An `EntityRef` field is set like any value, with the guid of the entity it names
(`entity.get` answers it as `id`): `{"Target": {"entityRef": "c480b8587bc8547b838377ccc3789645"}}`.
A rename onto a name another entity has gets the next free one (`Arm 1`).

A script never meets a dialog: while a request runs, the editor's questions and file pickers stay
closed, and a request that met one did nothing and fails, saying what was asked. Opening a level
over unsaved changes is refused that way: save them first (`level.save`), or pass `"discard": true`
to `level.open`, `level.play` or `map.open` to lose them. Every editor action that asks for a file has
an automation command that takes the path instead (`level.new`, `map.save`, `map.add`, `ui.new`,
`prefab.variant`, `project.export`...).

## Example: does Space jump?

```json
[
    { "command": "level.play", "params": { "path": "Levels/Main.opaaxlevel" } },
    { "command": "frames.wait", "params": { "count": 30 } },
    { "command": "expect.value", "params": { "entity": "Player", "path": "Transform/Position/y", "near": 0, "tolerance": 1 } },
    { "command": "input.key", "params": { "key": "Space", "action": "tap" } },
    { "command": "frames.wait", "params": { "count": 10 } },
    { "command": "expect.value", "params": { "entity": "Player", "path": "Transform/Position/y", "greater": 20 } },
    { "command": "screenshot", "params": { "path": "jump.png" } }
]
```

`Sandbox --exec jump.json` runs it and closes with exit code 0 when the player rose, 1 otherwise;
`jump.out.json` holds every answer and `jump.png` what the screen showed. The TestWorld project
runs scripts like this one for every engine feature (see `TestWorld/Tests`).

## Example: an agent makes a level

The editor builds a level the way a user would: a new level, entities with their components, saved,
then played to check it.

```json
[
    { "command": "level.new", "params": { "path": "Levels/Arena.opaaxlevel" } },
    { "command": "entity.create", "params": { "name": "Camera", "components": { "Camera": { "OrthoSize": 300 } } } },
    { "command": "entity.create", "params": { "name": "Ground", "components": {
        "Transform": { "Position": { "x": 0, "y": -200 } },
        "Quad": { "Size": { "x": 800, "y": 40 } },
        "Collider": { "Size": { "x": 800, "y": 40 } },
        "Rigidbody": { "Type": "Static" } } } },
    { "command": "entity.create", "params": { "name": "Crate", "components": {
        "Quad": { "Size": { "x": 40, "y": 40 }, "Color": { "x": 0.9, "y": 0.6, "z": 0.2, "w": 1 } },
        "Collider": { "Size": { "x": 40, "y": 40 } },
        "Rigidbody": { "Type": "Dynamic" } } } },
    { "command": "level.save" },
    { "command": "editor.play" },
    { "command": "world.wait", "params": { "seconds": 2 } },
    { "command": "expect.value", "params": { "entity": "Crate", "path": "Transform/Position/y", "near": -160, "tolerance": 2 } },
    { "command": "editor.stop" }
]
```

`MyGameEditor --exec arena.json` writes `Levels/Arena.opaaxlevel` and `Maps/Arena.opaaxmap`, and
passes when the crate rests on the ground. The game's own behaviours are components like any other
(`"Spinner": { "DegreesPerSecond": 90 }`): write them in C++, build, then place them. Make it the
project's startup level (`startupLevel` in `<Game>.opaaxproj`), and `project.export` makes the build
to ship.
