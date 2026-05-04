# M2 — Step 3 + Follow-ups + M2.5 (closure)

> Date: 2026-05-04
> Branch / commits: pushed on `main`
> Predecessors: `tasks/done/2026-05-01_m2-step1-step2.md`

This archive bundles everything that landed after M2 Steps 1+2 — the Save/Open dialog work, three follow-up bug fixes, and the M2.5 Editor Project Paths & Dialog UX polish. M2 is functionally done; only two PIE manual regression checks remain (see "Open" below) and they're not blocking new work.

---

## What landed

### M2 Step 3 — Save / Open scene dialogs (`tinyfiledialogs`)

- Vendored `tinyfiledialogs` under `Engine/Vendors/tinyfiledialogs/`. Editor-gated `add_library` block in `Engine/CMakeLists.txt`, Win32 link libs `comdlg32 ole32 user32 shell32`.
- `SceneManager` gained `m_CurrentScenePath` + `Save() / SaveAs(const char*) / Open(const char*) / NewScene() / HasCurrentScenePath() / GetCurrentScenePath()`. Open reuses the active scene (`World::Clear()` + `SceneSerializer::Deserialize`) so a derived `GameScene` survives the round-trip. `IsDirty()` deferred — modal always confirms.
- `MainMenuBar` File menu wired to `tinyfd_*` dialogs: Open Scene…, Save Scene (Ctrl+S), Save Scene As…, New Scene (with discard-confirm modal mirroring About-modal pattern). Ctrl+S via `ImGui::IsKeyChordPressed`.
- Removed the implicit shutdown auto-save: `CoreEngineApp::OnShutdown` no longer calls `SceneManager::SaveCurrentSave`; `GameScene::OnUnload`'s SaveScene gated with `#if !OPAAX_WITH_EDITOR`. **Editor mode never writes scenes implicitly** — captured in memory `feedback_editor_no_implicit_save`.

#### Recovery note
2026-05-04: user undid the Step 3 commit (`git revert` then `reset` away from it). Recovered cleanly via `git reset --hard 5c201c6` — commit was still in the reflog, parent matched HEAD, working tree was clean. No re-implementation required.

### F1 — Asset Browser auto-refresh after Save Scene As

- `AssetBrowserPanel::RunScan()` made public.
- New `EditorSubsystem::RefreshAssetBrowser()` shim — single-purpose, doesn't expose the panel.
- `MainMenuBar::DoSaveAs` calls it on a successful `SceneManager::SaveAs`. New `.scene.json` files appear in the browser without a manual Refresh click. `DoSaveOrSaveAs` inherits the refresh because it falls through to `DoSaveAs` when no current path is set.

### F2 — Current scene shown as "loaded" in browser

- `AssetBrowserPanel::Draw()` → `Draw(SceneManager*)`, mirroring the `HierarchyPanel::Draw(SceneManager*)` pattern (shadows `IEditorPanel::Draw()`, called directly by `EditorSubsystem::DrawPanels`).
- `DrawAssetList` / `DrawAssetEntry` plumb the SceneManager pointer through.
- New file-static `IsLoadedScene(...)` helper compares an entry's resolved abs path against `SceneManager::GetCurrentScenePath()` via `std::filesystem::equivalent` — handles slash/case differences without ad-hoc string normalisation. Bails fast on non-Scene types, missing files, and no-current-scene.
- `bLoaded` becomes `AssetRegistry::IsLoaded(InDesc.ID) || IsLoadedScene(...)` so the loaded scene reuses the existing texture-loaded green tint and "Loaded" tooltip status — no new visual style.

### F3 — Manifest poisoning fix

User redirected from "skip scenes from manifest" (the original `AssetScanner.h:27` doc-comment promise) to **treat scenes uniformly with other assets**. Concrete signals:
- "Don't assume scenes live in `Scenes/`."
- "Assets whatever the kind should be treated as same."

Implemented in `Engine/Source/Assets/AssetScanner.cpp`:
- `GenerateID` strips extensions iteratively (was: only the last). `Foo.scene.json` → `Foo` (was the broken `Foo.scene`). Single-extension files like `Player.png` are unaffected.
- `ResolveType` no longer keys off parent-folder name. Detection by extension alone: `.scene.json` → Scene, `.anim.json` → Animation, `.input.json` → InputMap, bare `.json` → Data. Saving a scene anywhere on disk now classifies correctly.
- Misleading `// Skip unknown types and scenes` comment replaced.
- Header doc-comment block rewritten — scenes flow into the manifest like every other asset; type discovery is filename-pattern based.

#### Notes for future
- `GameScene.json` (legacy single-extension) is now classified `Data`. Game still launches because Game-side load uses a hardcoded path (`Game/Source/Scene/GameScene.cpp:19`) independent of manifest type. Optional follow-up: rename to `GameScene.scene.json`.
- Animation / InputMap files don't exist on disk yet (verified via Glob). The new pattern detection is forward-compatible — drop them under `*.anim.json` / `*.input.json` and they'll classify without folder dependencies.

### M2.5 — Editor Project Paths & Dialog UX

Goal: dialogs default to source-tree paths instead of `build/.../bin/Debug/`, and remember the last-used dir within a session.

- **CMake**: `Engine/CMakeLists.txt` injects `OPAAX_PROJECT_ROOT="${CMAKE_SOURCE_DIR}"` as a PUBLIC compile def, gated on `OPAAX_EDITOR_SUPPORT`. Release binaries get nothing baked in.
- **`OpaaxPath`**: new `s_ProjectRoot` populated from the macro at `Init()` (with separator normalisation), `GetProjectRoot()` / `HasProjectRoot()` getters, and `ResolveFromProject(...)` that resolves against the project root in editor builds and falls back to `Resolve()` (exe-relative) when the macro is absent.
- **`EngineConfig`**: new `s_EditorDefaultScenePath` (default `"Game/Assets/Scenes"`) parsed/written under JSON key `editor.defaultScenePath`. Defensive parsing — missing key keeps the default.
- **`EditorSubsystem`**: volatile `m_LastDialogDir` member + `GetLastDialogDir()` / `SetLastDialogDir(...)` accessors. No persistence — session-only per the M2 scope decision.
- **`MainMenuBar`**:
  - `GetDialogDefaultDir(...)` helper returns last-used dir or, if empty, `OpaaxPath::ResolveFromProject(EngineConfig::EditorDefaultScenePath())`. Auto-creates the directory and appends a trailing slash for tinyfd's directory-hint semantics.
  - `RememberDialogDir(...)` stashes the parent of a chosen path back onto `EditorSubsystem`.
  - `DoOpen` / `DoSaveAs` use the helper; success path remembers the parent dir.
- **Untracked**: `Game/Assets/Scenes/` was created on demand by the helper (empty until used). User decides whether to add a `.gitkeep`.
- **Optional polish**: status-bar label showing the current scene path relative to project root — **deferred**.

---

## Files touched (across all of the above)

```
Engine/CMakeLists.txt
Engine/Vendors/tinyfiledialogs/...                    (vendored)
Engine/Source/Core/CoreEngineApp.cpp                  (no auto-save on shutdown)
Engine/Source/Core/OpaaxPath.{h,cpp}                  (project root + ResolveFromProject)
Engine/Source/Core/Config/EngineConfig.{h,cpp}        (editor.defaultScenePath)
Engine/Source/Editor/EditorSubsystem.{h,cpp}          (RefreshAssetBrowser, m_LastDialogDir)
Engine/Source/Editor/Toolbar/MainMenuBar.{h,cpp}      (dialog plumbing, helpers)
Engine/Source/Editor/Panels/AssetBrowserPanel.{h,cpp} (Draw(SceneManager*), IsLoadedScene, public RunScan)
Engine/Source/Assets/AssetScanner.{h,cpp}             (GenerateID strip-all, ResolveType by pattern)
Engine/Source/Scene/SceneManager.{h,cpp}              (Save/SaveAs/Open/NewScene API)
Game/Source/Scene/GameScene.cpp                       (gate auto-save with #if !OPAAX_WITH_EDITOR)
```

---

## Open at close

### Manual checks the user runs at their own cadence — non-blocking
- [ ] Play → Pause → Stop transitions log; viewport border tint changes (grey/green/amber).
- [ ] Gameplay-tick gating: a play-only system doesn't tick in Editing. Note: nothing production currently uses `IsPlayOnly()` so this is essentially a state-machine sanity check; physics in M5 will be the first real consumer.

### Deferred (out of M2 by design)
- Recent files persistence (config slot + LRU list).
- Multi-scene tabs.
- Drag-drop a `.scene.json` from the Asset Browser onto the menu bar (the FIXME at `AssetBrowserPanel.cpp:294` lives in M3 territory).
- Real Undo/Redo command system.
- Status-bar label showing current scene path (M2.5 stretch).

### New since M2 opened — captured for the next planning pass
- **Editor event system** — user wants `OnSceneSaved` / `OnEntitySelected` / `OnNewScene` to replace explicit cross-panel refresh calls (today's `EditorSubsystem::RefreshAssetBrowser()` shim being the canonical example). Memory: `project_editor_event_system`. Not scheduled.
- **Asset pipeline parity with Unreal/Unity** — user paused on the AssetBrowserPanel-doesn't-show-source-tree-files question to think it through. Right answer is a full asset-pipeline pass (cooked vs source assets, project-root manifest, source-of-truth in source tree, build copies cooked). Big — earns its own milestone, not a sub-fix. To revisit before M3 sprite UX if it blocks authoring, otherwise after.

---

## Lessons captured (already in `tasks/lessons.md`)

- L-001 — Audit existing persistence before adding new persistence.
- L-002 — Verification must cover cross-feature regressions, not just the new happy path.

---

## Plans archived

- `C:/Users/engue/.claude/plans/mighty-percolating-finch.md` (M2 Step 3 plan)
- `C:/Users/engue/.claude/plans/ok-test-work-i-indexed-boole.md` (F3 plan)
