---
name: opaax-drive-game-and-editor
description: Drive an Opaax game or the Opaax editor with JSON automation requests to see a change working - play levels, press keys, create and edit entities, undo, take screenshots, check values. Use to verify gameplay, rendering or editor changes, or to inspect a running game.
---

# Drive a game or the editor

Every Opaax app accepts JSON requests (Docs/Automation.md has the full list; `commands.list` asks the
app itself).

## A script (one shot)

Write the requests to a file and run the app with `--exec`:

```json
[
    { "command": "level.play", "params": { "path": "Levels/Physics.opaaxlevel" } },
    { "command": "world.wait", "params": { "seconds": 2.0 } },
    { "id": "rests", "command": "expect.value", "params": { "entity": "Crate1", "path": "Transform/Position/y", "near": -205, "tolerance": 3 } },
    { "command": "input.key", "params": { "key": "Space", "action": "tap" } },
    { "command": "screenshot", "params": { "path": "after.png" } }
]
```

`build/debug-editor/bin/Debug/TestWorld.exe --exec script.json` runs it and closes; the answers are in
`script.out.json` (or `--exec-out <file>`), the exit code is 1 if a request failed or never ran (the
app closed first). Run it from the
binary's folder or give absolute paths; a relative screenshot path is relative to the working folder.

## The editor

Same flag on `SandboxEditor` / `TestWorldEditor`, plus the editor's commands: `entity.create`
(with `components`), `entity.select`, `entity.parent`, `entity.rename`, `component.set` (undoable),
`component.add`/`remove`, `prefab.create`/`place`/`variant`, `editor.undo`/`redo`, `editor.play`/`stop`,
`level.new`, `map.new`, `map.add`, `level.open`, `level.play`, `level.save`, `map.save` (with `path`: Save
As), `ui.new`, `project.export`, and for assets (`.opaaxasset`: sheets, clips, input actions, a game's
data) `asset.classes`, `asset.new`, `asset.open`, `asset.get`, `asset.set`, `asset.undo`/`redo`,
`asset.save`, `asset.close`, `asset.list`; an engine asset is named `/Engine/...`. A game
starts from `level.new` (a level and its first map), entities, then `level.save`. A screenshot shows the
whole editor window. Nothing is saved unless asked, so a script can edit freely; the editor closes at
the end of the script.

No dialog ever opens for a script: a request that would ask (a file picker, "lose unsaved changes?")
fails, saying what was asked. Save first (`level.save`), or pass `"discard": true` to `level.open`,
`level.play` or `map.open`. `level.info` tells the open level, its maps, the focused one and what is
unsaved.

## A whole game

The order that works for a game made without help (a Tetris was made this way):

1. `MakeOpaax <Game>`, then `build`: the project with its runtime and its editor.
2. Code in `<Game>/Source/<Game>/`: the rules as plain classes, and a behaviour that drives them (input
   actions, prefabs it spawns, sounds, the HUD's bindings). Make its randomness seedable from a field,
   and keep what a test must read in fields (`expect.value` reads `<Behaviour>/<Field>`).
3. Content from the editor (`level.open`, `entity.create` with components, `prefab.create`, `ui.new`,
   `level.save`); input actions, mapping contexts and other assets with `asset.new` then `asset.set`
   and `asset.save`; the HUD's widgets written in the canvas file (Docs/AssetFormats.md).
4. A test script in `<Game>/Tests/`: play the level, set the seed with `component.set`, start a new
   game from the game's own input, press keys, check fields, `ui.list` for what the HUD shows, and open
   the screenshots.
5. `project.export`, then the same script on the exported game.

## Timing and the camera

Anything that moves is checked at the screen rates it will meet, not at this machine's:

- `time.pace` (or `--frame-pacing`) gives the frames a screen's time: `"144"`, `"60,jitter=0.25"`
  (timer noise), `"144,hitch=0.1@0.5"` (stalls), `"off"`. The same frames every run, without vsync.
- `config.set` changes a setting for the run: `{"config": "Engine", "values": {"Time": {"FixedRate": 30}}}`
  (`Window.bVSync`, `Render.bInterpolation` too); `config.get` reads one.
- `camera.info` says how the world is framed: the live camera, every camera and whether it is drawn
  between its steps, the last frame's alpha, steps and game time.
- `camera.trace.start` (`name`, `target`: an entity) then `camera.trace.stop` (`limits`) measure every
  frame in pixels (`targetDrift`, `motionJitter`, `largestMove`, `timeReversals`, `stillMoves`,
  `zoomReversals`) and write `<name>.camera.csv` to open; `camera.trace.compare` (`reference`, `trace`)
  checks a run at another rate draws the same path as a steady one.
- `world.pause` / `world.step` pause and step a game's world.
- `{"repeat": [{"tag": "60", "pacing": "60"}, ...], "requests": [...]}` runs requests once per entry,
  `{tag}` replaced: `TestWorld/Tests/CameraFollowing.json` is a whole matrix this way.

## A live session

`--automation <folder>` keeps the app open: write `001.request.json` (one request, written under
another name then renamed), wait for `001.response.json`, repeat; send `app.quit` at the end.

## Habits that pay

- Use `world.wait` (game time) rather than `frames.wait` in anything that checks timing.
- Find names first: `entity.list` (filter with `name` or `component`), then `entity.get` for values.
  Component fields have their saved names (`Transform/Position/x`, `Light2D/Intensity`).
- Menus and HUD: `ui.list` names the game UI's widgets; `ui.click` clicks one by name (no pixel
  guessing).
- Check with `expect.value` / `expect.count` / `expect.entity`: a script of checks is a test.
  `expect.quit` waits for the game to quit by itself (`QuitGame`). `"expectFailure": true` on a
  request checks a refusal: it passes when the command fails. `"expect": {...}` checks any answer
  (`{"command": "world.info", "expect": {"mode": "Play"}}`).
- Open every screenshot you take before saying what it shows.
- Requests cannot click inside an editor window (`input.*` feeds the game, not ImGui): check an
  asset's values with `asset.get`, and a custom editor's selected view with a local build that starts
  with a selection, reverted afterwards. A window restored from `imgui.ini` draws on its second frame:
  `frames.wait` before a screenshot.
- A level made for a check can go in TestWorld (`TestWorld/Assets/Maps`, `Levels`) with its script in
  `TestWorld/Tests` (`TestWorld/EditorTests` for an editor script): it then runs in CTest and CI.
