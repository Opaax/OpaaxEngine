# M0 — Hygiene Sweep

---

## Identification

- **ID:** M0
- **Name:** Hygiene Sweep
- **Status:** Active
- **Plan file:** `milestone/M0_Hygiene_Sweep.md`
- **Started:** 2026-05-05
- **Closed:** —

---

## Goal

Clean the engine surface so subsequent milestones land on solid ground: fix typos, make `OpaaxPath` project-root-relative, sweep `OpaaxTypes` usage, extract a shared registry template, and decide the naming / prefix policy.

---

## Why

Every later phase depends on names being stable, paths being meaningful, and `OpaaxTypes` being consistent. Doing this mechanically *before* architectural changes means later refactors don't fight typos and inconsistent integer types in the diff.

---

## Scope

### In scope
- Rename typos: `Registery → Registry`, `Minifest → Manifest`, `OpanGL → OpenGL` (folders + class names + every reference).
- Fix `OpaaxPath` to be project-root-relative; add `ToAbsolute()` and `ToProjectRelative()` helpers.
- Sweep engine code for bare `int`, `unsigned`, `size_t`, `std::string`, `std::unordered_map<std::string, ...>` etc. and replace with the canonical `OpaaxTypes` aliases.
- Extract `TRegistry<TKey, TValue>` template; have the four existing registry classes reuse it (type alias or thin wrapper).
- **Decide and document** the naming policy: namespace `opaax::` vs `Opaax*` prefix. Decision only — execution lives here only if the decision is "keep prefix and finish enforcement"; if "switch to namespace", a follow-up M0.5 milestone runs the rename.

### Out of scope
- Any architectural change (no new abstractions, no new systems).
- Any asset / world / renderer / camera work — those are M1+.
- A full namespace rename (only happens if M0.5 spawns).

---

## Dependencies

- None. M0 starts the program.

---

## Verifiable Checklist

### Step 1 — Typo sweep
- [ ] `AssetRegistery` → `AssetRegistry` (class + file + every reference)
- [ ] `AssetLoaderRegistery` → `AssetLoaderRegistry`
- [ ] `AssetTypeRegistery` → `AssetTypeRegistry`
- [ ] `ComponentDrawerRegistery` → `ComponentDrawerRegistry`
- [ ] `AssetMinifest` → `AssetManifest` (class + file + JSON file paths if any)
- [ ] `Engine/Source/RHI/OpanGL/` → `Engine/Source/RHI/OpenGL/` (folder rename + every `#include`)
- [ ] Build green in `build/debug-editor/` and `build/release/`
- [ ] Editor launches, opens existing scene, no missing-class errors

### Step 2 — `OpaaxPath` project-root-relative
- [ ] `OpaaxPath` stores a project-root-relative path internally
- [ ] `ToAbsolute()` resolves against the engine's known project root
- [ ] `ToProjectRelative(absolutePath)` static helper exists
- [ ] Every existing call site uses one of the two helpers — no raw concatenation
- [ ] Asset browser still finds existing `Engine/Assets/` and `Game/Assets/` content
- [ ] Existing scene files (`*.scene.json`) still load correctly after the path change

### Step 3 — `OpaaxTypes` consistency sweep
- [ ] No bare `int` / `unsigned int` / `unsigned` / `size_t` in engine `.h/.cpp` (except in third-party-facing wrappers)
- [ ] No bare `std::string` in engine code where `OpaaxString` applies
- [ ] `OpenGLShader` no longer uses `std::unordered_map<std::string, ...>` for uniforms (per `engine-planning.md` note)
- [ ] Doc comment block at the top of `OpaaxTypes.h` listing the canonical alias for each common primitive
- [ ] Build green

### Step 4 — `TRegistry<TKey, TValue>` extraction
- [ ] `TRegistry<TKey, TValue>` template lives in `Engine/Source/Core/Container/TRegistry.hpp`
- [ ] `AssetLoaderRegistry`, `AssetRegistry`, `AssetTypeRegistry`, `ComponentDrawerRegistry` are either type aliases or thin subclasses over `TRegistry`
- [ ] Public API of each registry preserved (no caller breakage)
- [ ] Build green; editor still loads textures, draws components, applies asset-type actions

### Step 5 — Naming / prefix policy decision
- [ ] Decision recorded in `Refactor_Program.md` Decision Log: namespace `opaax::` OR `Opaax*` prefix
- [ ] Rationale recorded (1 paragraph)
- [ ] If "namespace": create M0.5 follow-up milestone for the rename, do **not** execute it inside M0
- [ ] If "prefix": list every offending class without the prefix; mechanical rename in this milestone

---

## Verification Steps

- [ ] `build/debug-editor/` builds clean
- [ ] `build/release/` builds clean
- [ ] Launch editor → opens to default state, no console errors
- [ ] Open `Game/Assets/Scenes/NewScene.scene.json` → loads without errors
- [ ] Open `Game/Assets/Scenes/TestGameFolder.scene.json` → loads without errors
- [ ] Drag a `Texture2D` into the inspector → still binds correctly
- [ ] Save scene → reload editor → scene state preserved (no path corruption)

---

## Files Touched (planned)

- `Engine/Source/Assets/AssetRegistery.h.cpp` → `AssetRegistry.h.cpp`
- `Engine/Source/Assets/AssetMinifest.h.cpp` → `AssetManifest.h.cpp`
- `Engine/Source/Assets/Loader/AssetLoaderRegistery.h.cpp` → `AssetLoaderRegistry.h.cpp`
- `Engine/Source/Editor/Assets/AssetTypeRegistery.h.cpp` → `AssetTypeRegistry.h.cpp`
- `Engine/Source/Editor/Inspector/ComponentDrawerRegistery.h.cpp` → `ComponentDrawerRegistry.h.cpp`
- `Engine/Source/RHI/OpanGL/` → `Engine/Source/RHI/OpenGL/` (folder rename)
- `Engine/Source/Core/OpaaxPath.h.cpp`
- `Engine/Source/Core/OpaaxTypes.h` (likely +1 doc-comment block)
- `Engine/Source/Core/Container/TRegistry.hpp` (NEW)
- `Engine/Source/RHI/OpenGL/OpenGLShader.h.cpp` (uniform map types)
- Every `#include` site that referenced renamed files
- `.claude/data/engine-structure.md` (reflect renames)

---

## Risks

- **Path change breaks existing serialized scenes.** Mitigate: Step 2 ships with a one-shot migration that converts old paths in any `*.scene.json` / `AssetManifest.json` on first load — or run a manual migration script before the change, committed alongside.
- **Rename PR is large and fights every other branch.** Mitigate: do M0 in one focused session, no parallel branches.
- **`OpaaxTypes` sweep flushes out latent bugs** (e.g. signed / unsigned mismatches). Allocate slack. Treat each surfaced bug as a separate small commit, not a hidden fix inside the sweep diff.
- **`TRegistry` template surface choice (alias vs subclass)** affects what callers compile against. Pick one early and stick with it.

---

## Estimate

`Low` — ~1 week of focused evenings.

---

## Notes / Lessons (fill at close)

-
