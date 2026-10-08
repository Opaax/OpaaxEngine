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
`script.out.json` (or `--exec-out <file>`), the exit code is 1 if a request failed. Run it from the
binary's folder or give absolute paths; a relative screenshot path is relative to the working folder.

## The editor

Same flag on `SandboxEditor` / `TestWorldEditor`, plus the editor's commands: `entity.create`
(with `components`), `entity.select`, `component.set` (undoable), `component.add`/`remove`,
`editor.undo`/`redo`, `editor.play`/`stop`, `level.open`, `level.play`, `map.save`,
`project.export`. A screenshot shows the whole editor window. Nothing is saved unless asked, so a
script can edit freely; the editor closes at the end of the script.

## A live session

`--automation <folder>` keeps the app open: write `001.request.json` (one request, written under
another name then renamed), wait for `001.response.json`, repeat; send `app.quit` at the end.

## Habits that pay

- Use `world.wait` (game time) rather than `frames.wait` in anything that checks timing.
- Find names first: `entity.list` (filter with `name` or `component`), then `entity.get` for values.
  Component fields have their saved names (`Transform/Position/x`, `Light2D/Intensity`).
- Check with `expect.value` / `expect.count` / `expect.entity`: a script of checks is a test.
- Open every screenshot you take before saying what it shows.
- A level made for a check can go in TestWorld (`TestWorld/Assets/Maps`, `Levels`) with its script in
  `TestWorld/Tests`: it then runs in CTest and CI.
