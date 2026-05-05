# M1 — Asset Foundation

---

## Identification

- **ID:** M1
- **Name:** Asset Foundation
- **Status:** Planned
- **Plan file:** `milestone/M1_Asset_Foundation.md`
- **Started:** —
- **Closed:** —

---

## Goal

Introduce a unified asset model: `IAsset` base, `EAssetState`, `AssetType`, and a typed `TAssetHandle<T>`. Promote `Texture2D`, shader, and **scene** to first-class assets. Eliminate the `AssetRegistry` vs `AssetManifest` ambiguity flagged in `engine-planning.md`.

---

## Why

Without a base class, every asset is its own pipeline: editor-side and engine-side speak different vocabularies, and the bug already surfaced ("sprite serialized with manifest ID, loaded with registry → load failed"). Promoting scenes to assets is the keystone that unifies the rest of the engine and editor — `IAssetTypeAction` then naturally extends to scenes, the asset browser speaks one language, the inspector speaks one language.

---

## Scope

### In scope
- `IAsset` interface (ID, type, state, source path).
- `EAssetState` enum (`Unloaded`, `Loading`, `Loaded`, `Failed`).
- `AssetType` (initial set: `Texture2D`, `Shader`, `Scene`).
- `TAssetHandle<T>` — type-safe handle keyed by `StringID`, resolves through `AssetRegistry`.
- Promote `Texture2D` to `IAsset`.
- Promote shader to `IAsset` (`ShaderAsset`).
- Promote `Scene` to `IAsset` (`SceneAsset`) — scenes become discoverable through the asset registry.
- Reconcile `AssetRegistry` and `AssetManifest`: pick one as authoritative for runtime lookup; the other (likely the manifest) becomes the *on-disk index* the registry loads from at startup.
- Add `SceneTypeActions : IAssetTypeAction` so scenes participate in the editor's asset-type-action system.
- Replace raw asset pointers in components and serializers with `TAssetHandle<T>`.

### Out of scope
- Async / streaming asset loading (later, possibly its own milestone).
- Hot-reload (later).
- Asset cooking / packaging pipeline (later).
- Asset browser folder UI — that's M7.

---

## Dependencies

- **M0 done.** Specifically: typo fixes (we're touching these registries), `OpaaxPath` project-root-relative (asset paths use it), `TRegistry` extracted (asset type registry can reuse it).

---

## Verifiable Checklist

### Step 1 — Core types
- [ ] `IAsset` defined in `Engine/Source/Assets/IAsset.hpp`
- [ ] `EAssetState`, `AssetType` defined alongside
- [ ] `IAsset` exposes: `GetAssetID() : StringID`, `GetType() : AssetType`, `GetState() : EAssetState`, `GetSourcePath() : OpaaxPath`
- [ ] Every existing asset class compiles unchanged (no inheritance yet)
- [ ] Build green

### Step 2 — `TAssetHandle<T>`
- [ ] `TAssetHandle<T>` in `Engine/Source/Assets/AssetHandle.hpp` (replaces or extends current `AssetHandle.hpp`)
- [ ] Stores `StringID`, **not** raw pointer
- [ ] `Get() : T*` resolves through `AssetRegistry` (returns null if unloaded / wrong type)
- [ ] `IsValid() : bool` for non-null + correct-type check
- [ ] Implicit conversion semantics decided and documented (recommend: explicit `Get()` only, no `operator T*`)

### Step 3 — Promote concrete assets to `IAsset`
- [ ] `Texture2D : public IAsset` — split logical asset from GPU resource (`Texture2D` owns `UniquePtr<RHITexture>`)
- [ ] `ShaderAsset : public IAsset` — may require splitting current shader code from RHI shader object
- [ ] `SceneAsset : public IAsset`
- [ ] Each subclass loads its own bytes in a `Load(OpaaxPath)` method (or via `IAssetLoader` if cleaner)
- [ ] Build green; existing texture loading still works end-to-end

### Step 4 — Registry / manifest reconciliation
- [ ] One source-of-truth decision recorded in milestone notes: `AssetRegistry` is the runtime authority; `AssetManifest` is the serialized index file
- [ ] `AssetRegistry` loads itself from the manifest at engine init
- [ ] Asset IDs are stable: a sprite serialized today loads tomorrow without manifest / registry divergence
- [ ] Existing scenes still resolve their texture refs after the change

### Step 5 — Components and serializers use `TAssetHandle`
- [ ] `SpriteComponent` holds `TAssetHandle<Texture2D>` (not raw `Texture2D*`)
- [ ] `SceneSerializer` reads / writes `StringID` (handle ID), not file paths
- [ ] All existing scenes load correctly after migration (one-shot migration if the on-disk format changes)

### Step 6 — Scenes as assets in the editor
- [ ] `SceneTypeActions : IAssetTypeAction` registered on editor startup
- [ ] Asset browser shows `*.scene.json` files as scene assets
- [ ] Double-click a scene asset → opens it in the editor
- [ ] Right-click a scene asset → standard actions (open, rename, delete) via `IAssetTypeAction`

---

## Verification Steps

- [ ] Build green for editor and release
- [ ] Open `TestGameFolder.scene.json` → entities and sprite refs resolve
- [ ] Save scene → reload editor → no asset reference loss
- [ ] Drag a texture from asset browser onto a sprite → handle binds, texture renders
- [ ] Delete a texture from disk while editor is open → handle reports invalid (no crash)
- [ ] Create a new scene asset from the editor → appears in asset browser → can be re-opened
- [ ] Migration of existing scenes from old format runs once on first load and never again

---

## Files Touched (planned)

- `Engine/Source/Assets/IAsset.hpp` (NEW)
- `Engine/Source/Assets/AssetHandle.hpp` (REWRITE → `TAssetHandle<T>`)
- `Engine/Source/Assets/AssetRegistry.h.cpp` (renamed in M0; refactored here)
- `Engine/Source/Assets/AssetManifest.h.cpp` (renamed in M0; role narrowed here)
- `Engine/Source/RHI/OpenGL/OpenGLTexture.h.cpp` (logical-asset / GPU-resource split)
- `Engine/Source/Renderer/Texture2D.*` (NEW logical asset, if not already split)
- `Engine/Source/Renderer/ShaderAsset.h.cpp` (NEW or refactor of current shader logical side)
- `Engine/Source/Scene/SceneAsset.h.cpp` (NEW or rename of `Scene.h.cpp`)
- `Engine/Source/ECS/Components/SpriteComponent.h.cpp`
- `Engine/Source/Scene/SceneSerializer.h.cpp`
- `Engine/Source/Editor/Assets/Types/SceneTypeActions.h.cpp` (NEW)
- `.claude/data/engine-structure.md`

---

## Risks

- **Splitting "asset" from "GPU resource".** A `Texture2D` is both data on disk and an OpenGL handle — the asset is the data, the GPU resource is owned by the asset. Pattern: `class Texture2D : public IAsset { UniquePtr<RHITexture> m_RhiTexture; ... }`. Decide this early, document it, do not let it silently shape later code.
- **Scenes-as-assets breaks existing scene loading flow.** Mitigate: ship a migration in Step 5 + extensive scene-load testing in Verification.
- **`TAssetHandle` adoption is viral.** Once introduced, every component, every serializer, every editor inspector touches it. Estimate is `Medium` specifically because of this surface area.
- **`StringID` collisions across asset types.** Decide: are IDs globally unique, or scoped by `AssetType`? Recommend globally unique — simpler resolution, harder to bug.

---

## Estimate

`Medium` — 2–3 weeks of focused evenings.

---

## Notes / Lessons (fill at close)

-
