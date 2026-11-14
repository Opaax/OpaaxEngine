# The dev console

A line to type commands into a running game: the same commands scripts and agents send
([Automation.md](Automation.md)), typed as text. It is there in every dev build, in a game and in Play
In Editor, and gone from a ship build.

## Using it

The key left of 1 (`` ` `` on a US keyboard, `²` on a French one) opens it across the top of the screen,
and closes it again. While it is open the game gets no input: no action fires and the game's UI takes
no click or key. The key that closes it stays without effect until it is released, so the Escape that
closes the console does not also open the pause menu. Code that reads raw keys (`Input.IsKeyDown`)
still sees them.

| Key | Does |
|---|---|
| Enter | Runs the line |
| Escape | Closes the console (the line is kept for next time) |
| Left, Right, Home, End | Move the caret |
| Backspace, Delete | Delete the character before or after the caret; held, they repeat |
| Up, Down | Walk the lines run before, as a shell does; Down past the newest gives back what was being typed |
| Tab | Completes the word before the caret: a command's name, then a parameter's choices. Several matches are completed as far as they agree, and listed |
| Page Up, Page Down | Scroll the output |

`help` lists every command; `help time.scale` describes one, with its parameters and their choices
(`help` is a command of the registry, in every dev build: scripts can send it too).
A line is a command's name and its words:

```
time.scale 0.25
input.key Space tap
online.use fake profile=steam
level.play "Levels/My Level.opaaxlevel"
component.set Player Transform {"Position": {"x": 10}}
```

Words fill the declared parameters in order; `key=value` names one; a quoted word is text; a word
that reads as JSON (`5`, `true`, `[1, 2]`) is that value unless its parameter is text; a lone JSON
object is the whole params. A line that does not fit what the command declares is refused with what
is wrong (an unknown command, a missing parameter, a wrong type), and nothing runs.

Each answer is printed under the line that asked: text as it is, anything else as indented JSON.
Rows are not wrapped: a long one runs off the right edge. Log lines show too, from the level the
config names (warnings and errors by default). The lines run are kept between runs in
`<UserDir>/Console/History.txt`.

A command that waits (`frames.wait 60`, `world.wait 2`) answers when its wait ends; the console has
its own queue, so typing goes on meanwhile, and a script running at the same time is not held by it.

## In the editor

The editor's Log panel has the same line under its rows, in Edit and in Play: the same commands
(the editor's own too: `entity.create`, `asset.open`...), Tab, Up and Down. What it prints goes to the
log, in the `DevConsole` category: the line, the answer a row per line, and a refused line as a
warning. Its history is its own, in `<UserDir>/Console/EditorHistory.txt`.

## Settings

The `Console` section of `Engine.config`, read when a game starts (the Config panel draws it):

| Field | Default | Is |
|---|---|---|
| `ToggleKey` | `GraveAccent` | The key that opens and closes it |
| `bPauseWorld` | false | Pause the world while it is open |
| `Height` | 0.5 | The part of the screen it covers, from the top |
| `FontSize` | 22 | Its text, in pixels of a 1080-pixel-tall screen |
| `MaxLines` | 200 | Output rows kept |
| `HistorySize` | 100 | Lines kept for Up and Down |
| `LogLevel` | `Warn` | The lowest level of the log lines shown |
| `BackgroundColor`, `TextColor`, `InputColor`, `WarningColor`, `ErrorColor` | | Its colours |

## Adding a command

A game adds commands to the app's registry, for the whole run (`OnRegisterCommands` in its
application class) or for the life of a subsystem (`GetContext().Commands.Register`, keeping the
handle as a member): [Customizing.md](Customizing.md#automation). Declare the parameters, so the line
is checked and Tab can complete them:

```cpp
m_Teleport = GetContext().Commands.Register(
    Opaax::CommandSpec("game.teleport", "Moves the player: start, finish, or x y.")
        .Param("x", Opaax::ECommandParamType::Any, "start, finish, or the x position")
            .Choices({ "start", "finish" })
        .Param("y", Opaax::ECommandParamType::Float, "the y position").Optional()
        .Cheat()
        .DevOnly(),
    [this](const nlohmann::json& InParams) { return Teleport(InParams); });
```

- `Choices` are the only values a typed parameter takes, and hints for `Any` (so `game.teleport 120 40`
  and `game.teleport finish` both fit).
- `NamesCommand()` makes a parameter a command's name (as `help`'s): Tab completes it among the commands.
- `Cheat()`: the command changes the game in a way a player could not (below).
- `DevOnly()`: not registered in a ship build.

## Looking at the renderer

`renderer.info` prints what the renderer is using now: the GPU and its driver, the most samples a
multisampled target can have, the texture filter, and each world view's scene target (its size,
format and samples). A renderer setting is tried while the game runs with `config.set` (a cheat,
for this run only), and `frame.sample` counts the colours of a part of the frame: 2 across a hard
edge, more across a smoothed one.

```
renderer.info
config.set Renderer {"Multisampling": "X4", "bFXAA": true}
frame.sample 600 340 width=40
```

Both are described in [Automation.md](Automation.md#commands); the settings themselves in
[Customizing.md](Customizing.md#smooth-edges-which-setting).

## Cheats

A command marked `Cheat()` that answers ok, from the console, a script or anywhere, publishes
`CheatUsed{Command}` on the engine's event bus and sets a flag (`ICommandRegistry::HaveCheatsRun()`),
cleared when a game starts. A game that posts times or scores checks it: a run that used a cheat is
not posted. A refused cheat changed nothing and marks nothing. `help` marks cheats `[cheat]`; the
engine's are listed in [Automation.md](Automation.md#commands).

## Time scale

`time.scale 0.25` runs the game at a quarter of its speed: the world, its fixed steps (each of the
usual length, fewer of them) and its camera run on game time; the UI, the input and the console on
real time. Kept in [0.05, 4], and back to 1 when the game ends. A cheat, and dev only.

## Ship builds

A ship build (`build.bat release`, an export) has no console: the key does nothing, and `help`,
`console.info`, `console.run`, `time.scale` and every command a game marks `DevOnly()` are not there.
The engine's other cheats (`component.set`, `world.step`...) stay for the release feature tests, which
run scripts on the ship build; they still mark the run.

## For scripts

`console.info` answers whether the console is open, the line being typed and its caret, and its last
rows (`rows`, 20 by default) with their kind: what a test checks after typing with `input.key` and
`input.text` (`TestWorld/Tests/Dev/Console.json`). `console.run {line}` runs a line as the console
reads it, without the console.
