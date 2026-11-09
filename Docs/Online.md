# Online services

What a game gets from the player's store (Steam today, others later): the player and their friends'
names, leaderboards, the store's overlay. One set of calls whatever the store, and whether there is
one: a game started without a store, or on a store that lacks a feature, plays on and gets an answer
that says so.

## What a game gets

| Feature | Fake store | Steam |
|---|---|---|
| The player, other players' names, the friends list | Yes | Yes |
| Leaderboards: find or make, upload keeping the best, World / AroundPlayer / Friends | Yes | Yes |
| A file on a leaderboard entry (a ghost, a replay, a level, a picture) | Yes, except the `epic` profile | Yes, with the Steam Cloud ([below](#steam)) |
| Achievements | Yes | Not yet: `Unsupported` |
| The overlay, and its opening and closing | Yes (`online.overlay`) | Yes (Shift+Tab) |

`GetCapabilities()` says what the store behind the services can do, so a game hides a button rather
than learning it from an answer:

| Field | Means |
|---|---|
| `bIdentity`, `bLeaderboards`, `bAchievements`, `bOverlay` | The feature is there |
| `bLeaderboardsMadeAtRunTime` | The store makes a board the first time the game asks for it; otherwise it must exist in the store's portal |
| `bFriendsRange` | The store gives the friends range itself; otherwise the services build it from the friends list |
| `bLeaderboardFiles`, `MaxFileBytes` | A file can go on an entry, of this size at most. On Steam, off when the app or the player has the Steam Cloud off |
| `MaxDetails` | Detail numbers kept with a score (Steam: 64); more are cut, with a warning |

## The game's leaderboards

A game names its boards once, in `Configs/Online.config` (the Config panel edits it). The same names
for every store: a store that makes boards makes them from this, another finds them by name.

```json
{
    "Backends": [ "Steam" ],
    "Leaderboards": [
        { "Name": "Level1", "Sort": "Ascending", "Display": "TimeMilliseconds" }
    ],
    "Steam": { "AppId": 0 }
}
```

| Field | Means |
|---|---|
| `Backends` | The stores the game tries, in order; the first that starts is used. Empty: always offline |
| `Leaderboards` | `Name`, `Sort` (`Ascending`: the smallest is best, a time; `Descending`: points), `Display` (`Numeric`, `TimeSeconds`, `TimeMilliseconds`: how the store's own pages show it) |
| `Steam.AppId` | The game's Steam app id ([below](#steam)) |

A board the config does not list answers `NotFound`, and the log says to add it.

## Calling it

Every game has an `OnlineSubsystem`. A behaviour finds it with
`GetGameSubsystem<Opaax::OnlineSubsystem>()`; it is null while no game runs (the editor outside Play).
A request answers at a later frame, never inside the call, on the main thread. The subsystem lasts
across levels, so a behaviour that asks guards itself in its callback:

```cpp
Opaax::OnlineSubsystem* lOnline = GetGameSubsystem<Opaax::OnlineSubsystem>();
const Opaax::TWeakObjectPtr<ResultsScreen> lSelf(this);

lOnline->UploadScore("Level1", lTimeMs, {}, lGhostBytes,
    [lSelf](const Opaax::EOnlineResult InResult, const Opaax::ScoreUploadResult& InUpload)
    {
        if (ResultsScreen* lScreen = lSelf.Get())
        {
            lScreen->ShowUpload(InResult, InUpload.bScoreChanged, InUpload.Rank);
        }
    });

lOnline->DownloadEntries("Level1", Opaax::ELeaderboardRange::AroundPlayer, -5, 5,
    [lSelf](const Opaax::EOnlineResult InResult, const Opaax::TDynArray<Opaax::LeaderboardEntry>& InEntries)
    {
        if (ResultsScreen* lScreen = lSelf.Get())
        {
            lScreen->ShowLines(InResult, InEntries);
        }
    });
```

- **Upload** keeps the player's best by the board's sort. A file given with it goes on the entry only
  when the score was kept, so the best entry keeps its own file; on a store without files the upload
  answers `Unsupported`, so send the file only when `bLeaderboardFiles` is set. The bytes are the
  game's own (a ghost, a replay, a level a player made, a picture): the store only keeps them.
  The score is uploaded first: when the file is then refused (`QuotaExceeded`), the score is kept
  and its entry has no file, so check the size against `MaxFileBytes` before.
- **Ranges:** `World` takes ranks (1 to 10 is the top ten), `AroundPlayer` offsets from the player
  (-5 to 5 is eleven lines, none when the player has no entry), `Friends` ignores both.
- **Names** come with the entries. A name the store does not know yet is asked for; the answer waits
  a few seconds at most, then comes with an empty name for those still unknown: show the id there,
  never an empty line.
- **Files:** `DownloadFile(lEntry.File, ...)` reads a file on an entry.
- **Cancel** a request with `Cancel(id)`. When the game ends (Play stopped, the game quit), what it
  asked and is not answered is dropped: no callback runs into a game that is gone.

Each answer is an `EOnlineResult`:

| Result | Means | What a game does |
|---|---|---|
| `Ok` | Done | |
| `Unavailable` | No store runs: offline, a build without one, the editor | Hides the online screens, or shows them empty |
| `Unsupported` | The store runs but lacks the feature | Hides it (check the capabilities first) |
| `NotFound` | No such board, file or achievement | Fixes the config, or the store's portal |
| `Failed` | The store refused or the network failed; the log says why | Tells the player, offers to retry |
| `Cancelled` | Cancelled, or the game ended | Nothing |
| `TimedOut` | The store did not answer in time | As `Failed` |
| `QuotaExceeded` | The player's storage on the store is full | Uploads without the file |
| `NotSignedIn` | The store runs, but offline or signed out | As `Unavailable`, and says so |

### The overlay

When the store's overlay opens (Shift+Tab on Steam) the player is looking at the store, not the game:
pause it there.

```cpp
// OnStart; m_Online and m_Handle are members, removed again in OnDestroy.
m_Online = GetGameSubsystem<Opaax::OnlineSubsystem>();
m_Handle = m_Online->OnOverlayChanged().Add([lOnline = m_Online](const bool bInOpen)
{
    lOnline->GetContext().Worlds.SetPaused(bInOpen);
});
```

The overlay changes outside the world's tick, paused or not, so the callback runs while the game is
paused too.

`OpenOverlay(EOverlayPage::Leaderboards)` opens a page of it (friends, achievements, leaderboards, a
player's profile), false when the store has no overlay or cannot show it now.

## Which store runs

The store starts with the app, before its window (Steam's overlay needs that), and stays for its
lifetime. The first that applies chooses:

1. `--online none`, `--online fake[:profile]` or `--online steam` on the command line.
2. The editor: no store. Play In Editor answers `Unavailable` (`--online fake` gives it a fake one).
3. The game's `Backends`, the store that launched it first (Steam sets `SteamGameId` when it does).
   A store whose SDK the build lacks is left out, and the reason says so.

A store that fails to start (Steam not running, its library missing, no app id) is skipped, the log
says why, and the game plays offline. `online.info` answers the store and that reason
([Automation.md](Automation.md)).

## Steam

**The SDK** is never in the repository: Valve's agreement forbids publishing it, and the repository
is public. Each developer downloads the Steamworks SDK from partner.steamgames.com and either unzips
it to `Engine/Vendors/steamworks/sdk` (git-ignored: configuring stops when a file of it is tracked)
or points CMake at it, by the `OPAAX_STEAMWORKS_SDK` cache variable or the `STEAMWORKS_SDK`
environment variable. Without it the build leaves Steam out and games play offline; CI builds that way.

**Nothing links it.** The engine opens `steam_api64.dll` (`libsteam_api.so`, `libsteam_api.dylib`)
beside the executable while it runs: the build copies it beside each game and a ship build installs
it there. A game without the file starts and plays offline.

**DRM-free.** The engine never calls `SteamAPI_RestartAppIfNecessary`: started outside Steam, the
game runs, with Steam if it can and offline if not. Store DRM is a choice for the game's page, not
the engine's.

**The app id.** Started by Steam, Steam names the app. Started by hand, the game names it from
`Steam.AppId` (it sets `SteamAppId` in its own environment, as `steam_appid.txt` would). Keep it 0 in a
shipped config when the game should use Steam only when Steam starts it. For development without an
app of your own, Valve's test app is 480 (Spacewar): set it in the config, or for one run only:

```
set SteamAppId=480
Sandbox --online steam
```

**Leaderboards** are made the first time the game asks for them (`FindOrCreateLeaderboard`), with the
config's sort and display. A board made earlier keeps its own sort on Steam: a different one in the
config is logged as a warning, and Steam's wins. For a released game, make them in the Steamworks
portal (Stats & Achievements, Leaderboards) with the same names, and publish; a board set to be
written by a trusted server only refuses the game's uploads (`Failed`).

**Files on entries** are files of the player's [Steam Cloud](https://partner.steamgames.com/doc/features/cloud),
shared, their handle attached to the entry. In the Steamworks portal (Application, Steam Cloud):
enable the Cloud, set the byte quota per user and the number of files per user, and publish. No
Auto-Cloud path is needed: the engine writes through the API.

- One file per board, `leaderboards/<board>.bin` (a character of the board's name other than a
  letter, a digit, `_` or `-` is written `%XX`). A better score replaces it, and the handle of the
  file it replaced answers `NotFound`. So the quota holds at least the largest file of every board,
  and as many files as boards. A player sees them in Steam, under their account's Remote Storage.
- `MaxFileBytes` is the player's quota, 100 MB at most (one write's limit). Valve's test app 480
  gives 4096 bytes.
- A file over `MaxFileBytes`, or over the room left in the player's cloud, answers `QuotaExceeded`
  before Steam is asked.
- A player who turns the Steam Cloud off, for the game or for their account, has no files:
  `bLeaderboardFiles` is false and an upload with a file answers `Unsupported`.
- A new best score drops the file of the entry it replaces: an entry never shows a file of another
  score.

**Checking it** needs a Steam client signed in. CI never builds Steam, so a change to `Online/Steam`
is checked on a machine with the SDK and Steam running:

```
set SteamAppId=480
TestWorld --online steam --exec Scratch/steam_leaderboard.json
```

with `online.leaderboard.upload` and `online.leaderboard.entries` requests on a board of TestWorld's
`Online.config`. For files: an upload with a `file` and a better score than the entry's, the entry's
`file` from `online.leaderboard.entries`, then `online.file.get` on it, from the same account and
from another one. On Linux the overlay works only when Steam starts the game.

## Testing without a store

The fake store keeps players, boards, files and achievements in memory, and answers a frame later as
a store would. Its profiles behave as each store does, so a game's screens are tried against each
store's limits without it:

| Profile | Like | Differs |
|---|---|---|
| `full` | Every feature | |
| `steam` | Steam | As `full` today: Steam's limits as the fake store keeps them (64 details, files of 1 MB; on Steam the size is the app's Cloud quota) |
| `epic` | Epic Online Services | No files, no detail numbers, no boards made at run time, no friends range |
| `gog` | GOG Galaxy | No friends range (the services build it from the friends list) |

Feature tests run on it (`--online fake`, never a real store). The automation adds players and scores
(`online.fake.player`), switches the profile while the game runs (`online.use`) and opens the overlay
(`online.overlay`); `TestWorld/Tests/Online.json` plays a results screen against it. A unit test makes
an `OnlineServices` over a `FakeOnlineBackend` and pumps it.

## Adding a store

Each store is a backend behind `IOnlineBackend`, which gives the feature interfaces its store has
(`IOnlineIdentity`, `IOnlineLeaderboards`, `IOnlineFiles`, `IOnlineAchievements`, `IOnlineOverlay`) and
null for the others. The services do the rest: answers at the next pump, the board handles, names,
the friends range from the friends list, uploading then attaching a file.

1. A value in `EOnlineBackend`, its `ToString` and its `OPAAX_ENUM_VALUES`.
2. The backend in `Engine/Source/Online/<Store>/`, the only code that includes the store's SDK. It
   loads the store's library while running (`DynamicLibrary`), so a game without it starts, and runs
   the store's callbacks in `Pump`.
3. What it decides without the SDK (ids, results, limits) in `Online/<Store>Rules.h`, unit tested:
   CI builds without any store SDK.
4. An `opaax_add_online_sdk(<Store> ...)` line in `Engine/CMakeLists.txt` (`CMake/OpaaxOnline.cmake`):
   where the SDK is looked for, its include folder and its libraries. Not found, `Online/<Store>/` leaves
   the build.
5. In `OnlineBackendFactory.cpp`, the store in `GetBuiltInBackends` and its case in `CreateBackend`,
   under `#ifdef OPAAX_ONLINE_<STORE>`; its settings in `OnlineConfigData`; its `--online` name and its
   launch sign in `ChooseOnlineBackends`.
6. A fake store profile when its limits differ from those above, so games test against them.
