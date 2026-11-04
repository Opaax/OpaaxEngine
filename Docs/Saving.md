# Saving

What a game keeps for a player between launches (best times, unlocked levels, settings) and how it
reaches the disk: as objects saved by their properties, one file per slot in the player's own
folder, written whole or not at all, now or in the background.

## A save class

A save is an `Opaax::SaveGame` subclass. Its fields are properties, as an asset's or a config's, and
every field has a default: a file written before a field existed still loads.

```cpp
#include "Engine/Registries/AutoRegistration.h"
#include "Save/SaveGame.h"

struct LevelRecord
{
    Opaax::OpaaxString Level;
    float              BestTime = 0.f;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LevelRecord, Level, BestTime)
    OPAAX_PROPERTIES(LevelRecord, OPAAX_PROP(Level), OPAAX_PROP(BestTime))
};

class ProgressSave final : public Opaax::SaveGame
{
    OPAAX_OBJECT(ProgressSave, Opaax::SaveGame)

public:
    Opaax::TDynArray<LevelRecord> Records;
    Opaax::Int32                  Runs = 0;

    OPAAX_PROPERTIES(ProgressSave, OPAAX_PROP(Records), OPAAX_PROP(Runs))
};

OPAAX_REGISTER_OBJECT(ProgressSave);
```

The field types are the properties' ([Objects.md](Objects.md)): numbers, `bool`, strings, names,
enums with `OPAAX_ENUM_VALUES`, vectors, colours, and lists of structs like `LevelRecord`. A game may
have several save classes, one per slot that holds a different thing (the progress, the settings, a
level's ghost).

## Saving and loading

Every game has a `SaveGameSubsystem`. A behaviour finds it with
`GetGameSubsystem<Opaax::SaveGameSubsystem>()`, and a game's `GameInstance` class with
`GetSubsystems().GetSubsystem<Opaax::SaveGameSubsystem>()`. It is null while no game runs (the editor
outside Play).

```cpp
Opaax::SaveGameSubsystem* lSaves = GetGameSubsystem<Opaax::SaveGameSubsystem>();

Opaax::TStrongObjectPtr<ProgressSave> lSave;
const Opaax::ESaveResult lLoaded = lSaves->Load("Main", lSave);
if (lLoaded == Opaax::ESaveResult::Missing)
{
    lSave = Opaax::NewObject<ProgressSave>();     // a first launch
}

if (lSave)
{
    ++lSave->Runs;
    lSaves->Save("Main", *lSave);
}
```

`Load` names the save after its slot and leaves the pointer as it was when it fails. Each call says
how it ended, and logs the failures:

| Result | Means | What a game does |
|---|---|---|
| `Ok` | Done | |
| `Missing` | The slot has no save yet | Starts a new save: the only result that should |
| `Corrupt` | The file is not a save (cut short, edited by hand) | Keeps the file: the player's progress may be recovered from it. Starts with a save in another slot, or asks the player |
| `NewerVersion` | Written by a newer game or engine | Never saves over it: the newer game's data would be lost |
| `WrongClass` | The file holds another class, or one the game no longer registers | As `Corrupt` |
| `InvalidSlot` | The slot name is not a plain file name (below) | Fixes the name |
| `IoError` | The disk refused (no space, no rights) | Tells the player; the slot keeps its last save |

`Exists`, `Delete` and `ListSlots` work on the slots the same way.

## In the background

Writing a file can take longer than a frame. `SaveAsync` and `LoadAsync` do the work on a worker
thread and answer on the main thread:

```cpp
// The answer comes frames later: the behaviour may be gone by then.
const Opaax::TWeakObjectPtr<RunTracker> lSelf(this);

lSaves->LoadAsync<ProgressSave>("Main",
    [lSelf](Opaax::ESaveResult InResult, Opaax::TStrongObjectPtr<ProgressSave> InSave)
    {
        if (RunTracker* lTracker = lSelf.Get())
        {
            lTracker->OnProgressLoaded(InResult, InSave);
        }
    });

lSaves->SaveAsync("Main", *lSave, [](Opaax::ESaveResult InResult) { /* a "saved" icon */ });
```

- `SaveAsync` turns the save into text when it is called: the game may change the object at once.
  The worker only writes the file.
- The background saves and loads run one at a time, in the order asked: a load asked after a save
  reads it. `Load`, `Save`, `Exists`, `Delete` and `ListSlots` wait for the ones asked before them.
- A callback runs on the main thread at the subsystem's next `Update`, never inside the call that
  asked for it. Capture a `TWeakObjectPtr` to the object it reports to, as above, not `this`.
- `IsBusy()` is true from the call until its callback has run: a "saving..." icon. `Flush()` waits for
  everything asked and runs the callbacks.
- When the game ends (quitting, or stopping Play In Editor), everything asked is still written. The
  callbacks run while the game is whole; a request made while the game's subsystems are shutting down
  is written without its callback.
- A load makes the save on the worker: `PostLoad` and `UpgradeProperties` may run there and must touch
  nothing but the save. One that throws makes the load answer `Corrupt`.

## Slots

A slot is a file name: 1 to 64 letters, digits, `_` or `-` (`Main`, `Profile2`, `Ghost_Level3`), and
not a name Windows keeps for a device (`CON`, `NUL`, `COM1`...). A name a player types ("Zoé's game")
goes in the save as a field; the slot gets a name of the game's (`Profile2`).

`Settings` holds the player's settings ([Settings.md](Settings.md)), in any case: a game's save there is
refused (`InvalidSlot`), and `ListSlots` leaves it out.

## Versions

A field added with a default needs nothing: an older file loads with the default. A value of the wrong
type keeps its default too, and the log names the field, so one bad field does not cost a player the
rest of the save.

A change the defaults cannot absorb (a renamed field, a value in other units) raises the class's
version, and the class rewrites what older files hold before they are read:

```cpp
Opaax::Int32 GetSaveVersion() const override { return 2; }

void UpgradeProperties(Opaax::Int32 InFileVersion, nlohmann::json& InOutProperties) const override
{
    // Version 1 called Runs "Attempts".
    if (InFileVersion < 2 && InOutProperties.contains("Attempts"))
    {
        InOutProperties["Runs"] = InOutProperties["Attempts"];
        InOutProperties.erase("Attempts");
    }
}
```

A file of a higher version than the class's is refused (`NewerVersion`), so an older game never
overwrites what a newer one saved.

## Where the files go

| Build | Folder |
|---|---|
| Dev build (the editor, `build.bat`) | `<ProjectRoot>/Save/SaveGames`, ignored by git: a developer's runs stay with the project and never mix with an installed copy's |
| Ship build, Windows | `%APPDATA%/<Project>/SaveGames` |
| Ship build, Linux | `$XDG_DATA_HOME/<Project>/SaveGames`, or `~/.local/share/<Project>/SaveGames` |
| Ship build, macOS | `~/Library/Application Support/<Project>/SaveGames` |
| `--user-dir <folder>` | `<folder>/SaveGames`, in any build. The feature tests use `<build>/<Game>Results/User` |

A ship build's own folder is where it was installed, which the player may not be able to write to;
the folder above is the player's own (`IPaths::UserDir`, `ISaveStorage::GetSaveDirectory`). The log
(`Log/OpaaxEngine.log`) and the crash reports (`Crashes/`) go beside `SaveGames` in it, so in a dev
build they stay in `<ProjectRoot>/Save`. In the editor, **File > Open Save Folder** shows the saves,
and the log names the folder at startup (`user dir`).

A slot is `<Slot>.opaaxsave`, text a person can read, keys sorted:

```json
{
    "Class": "ProgressSave",
    "ClassVersion": 1,
    "Properties": {
        "Records": [ { "BestTime": 41.5, "Level": "Levels/Canyon.opaaxlevel" } ],
        "Runs": 12
    },
    "Version": 1
}
```

`Version` is the file format's, as in an asset file; `ClassVersion` is the save class's. A write goes
to `<Slot>.opaaxsave.tmp`, is flushed to the disk, then renamed over the slot in one step: after a
crash or a power cut the slot holds the last save or the new one, never half of one. What a crash
leaves in the `.tmp` is never read, and the next save replaces it.

## Testing

- A unit test starts the subsystem the way a game does, with its saves in a temp folder:
  `Engine/Tests/Save/SaveGameSubsystemTests.cpp`. `TestManualJobs` (`Engine/Tests/TestManualJobs.h`)
  holds the background work until the test runs it, to check what the game sees before and after.
- A feature test prepares and reads the slots with `save.set`, `save.get`, `save.list` and
  `save.clear` ([Automation.md](Automation.md)). `TestWorld/Tests/Saves.json` seeds a save, plays a
  level that loads and saves it in the background, and checks the file.

## How it is built

`ISaveStorage` (`Application/Services/ISaveStorage.h`) is an app service: the slot files and their
text, with no game needed, so the automation and the editor use it too. A platform with a save API of
its own (a console) would replace it. `SaveGameFile` (`Save/SaveGameFile.h`) turns a save into text
and back. `SaveGameSubsystem` (`Save/SaveGameSubsystem.h`) is the game's side: objects, versions, the
background requests, and their end with the game. [Architecture.md](Architecture.md#saves-in-the-players-folder-written-whole)
says why.

Not there yet: compression or encryption of a save, a copy of the last good save beside it, and slots
in sub-folders.
