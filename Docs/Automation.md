# Driving a game or the editor from outside

Every Opaax app (a game, its editor) can be driven by JSON requests: from a script in CI, from a
test, or by an AI agent. The commands are the same everywhere; the editor adds its own.

## Starting

| Flag | What happens |
|---|---|
| `--exec <script.json>` | Runs the script's requests in order, then closes the app. Each answer is written to `<script>.out.json` as it comes (`--exec-out <file>` to choose). The exit code is 1 when a request failed, or never ran because the app closed first. |
| `--automation <folder>` | Keeps the app open and runs every `<name>.request.json` dropped in the folder (in name order), answering in `<name>.response.json`. `app.quit` closes it. |

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
| `app.info` | | Frame number and the project's folders |
| `app.quit` | | Closes the app at the end of the frame |
| `frames.wait` | `count` | Lets frames run before the next request |
| `world.wait` | `seconds` | Lets the playing world's clock advance (game time, whatever the frame rate) |
| `screenshot` | `path` | Saves the frame as a PNG (in the editor: the whole editor) |
| `input.key` | `key`, `action`, `frames` | `press`, `release` or `tap` a key (`Space`, `A`, `Mouse_Left`...) as if from the keyboard |
| `input.mouse` | `x`, `y` | Moves the game's pointer (pixels of the game view) |
| `ui.list` | `name` | The game UI's widgets: name, type, visible, centre and size in game-view pixels |
| `ui.click` | `widget`, `frames` | Clicks a widget of the game's UI by name: the pointer moves to its centre, the left button is tapped |
| `world.info` | | The active world: name, mode, paused, entities, camera |
| `entity.list` | `name`, `component` | The entities (optionally filtered) with their components |
| `entity.get` | `entity` | One entity with its components' values |
| `component.set` | `entity`, `type`, `value` | Merges `value` into the component (adds it when missing) |
| `level.play` | `path` | Opens a level (relative to the project's assets) in a new play world |
| `expect.value` | `entity`, `path`, check | Fails unless the value at `path` (`Transform/Position/x`) passes the check |
| `expect.count` | `name`, `component`, check | Fails unless the number of matching entities passes the check |
| `expect.entity` | `entity`, `exists` | Fails unless an entity with that name or id exists (or, with `exists: false`, none does) |
| `expect.quit` | `seconds` | Waits for the game to close the app itself (`QuitGame`); fails if it is still open after `seconds` (10) |

A check is any of `equals` (numbers within 1e-4; for an object, the fields given), `near` with a
`tolerance` (0.01 by default), `greater`, `less`, `between: [min, max]`. A failed expectation fails
its request, so a script of expectations is a test: its exit code is 1 when one fails. When the app
closes before the script's end, the requests left fail too: a game that quits early cannot pass a
test it never finished.

The editor adds:

| Command | Params | Does |
|---|---|---|
| `editor.play`, `editor.stop`, `editor.pause`, `editor.step` | | Play In Editor |
| `level.play` | `path` | Opens a level, then plays it in the editor |
| `editor.undo`, `editor.redo` | | Undo and redo, as the Edit menu does |
| `editor.command` | `tag` | Any editor command without arguments, by tag (`Editor.Command.FocusSelected`) |
| `level.open`, `map.open` | `path` | Opens a level or a map (relative to the project's assets) |
| `level.save`, `map.save` | | Saves (a map must already have a file) |
| `project.export` | `path` | Exports the game into a folder (its release build and content); answered when done, minutes later |
| `entity.create` | `name`, `map`, `components` | Creates an entity in the focused map, with components `{type: values}` |
| `entity.destroy` | `entity` | Deletes an entity |
| `entity.select` | `entities` | Selects entities (an empty list clears the selection) |
| `component.add`, `component.remove` | `entity`, `type` | Adds or removes a component |
| `component.set` | `entity`, `type`, `value` | As the engine's, recorded for undo in the edit world |

Edits made by the editor's commands are the same as a user's: undoable, and saved only when asked.

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
