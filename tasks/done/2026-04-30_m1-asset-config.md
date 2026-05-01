# Done — M1 Asset Unification + Config Foundation

> Source plan: `C:\Users\engue\.claude\plans\silly-petting-charm.md`
> Verified working & pushed by user (2026-04-30).

## Implementation
- [x] New `Engine/Source/Core/Config/EngineConfig.{h,cpp}` — JSON load/save via `nlohmann/json`, defaults match historical hardcoded constants, auto-generates a default file on first launch.
- [x] `CoreEngineApp` ctor: calls `EngineConfig::Load(OpaaxPath::Resolve("engine.config.json"))` after `OpaaxPath::Init()`; window now created from `EngineConfig::WindowTitle/Width/Height()`.
- [x] `CoreEngineApp::Initialize`: manifest paths now sourced from `EngineConfig::EngineManifestRelPath()` / `GameManifestRelPath()`.
- [x] `SpriteComponent::Serialize`: reverse-lookup via `AssetManifest::FindByPath(MakeRelative(absPath))`; writes logical ID when manifest knows the file, falls back to relative path otherwise.
- [x] `SpriteComponent::DeserializeImplementation`: stops pre-resolving; passes raw stored reference to `AssetRegistry::Load`, letting `ResolveToAbsPath` route the three cases (absolute / manifest logical ID / relative path).
- [x] Removed orphan `World m_World;` from `CoreEngineApp.h`. `GetWorld()` already routes through `SceneManager`.

## Verification
- [x] Editor build clean (debug-editor preset).
- [x] First launch generates `engine.config.json` with the documented JSON shape.
- [x] Config drives window size on restart.
- [x] Editor flow: scan → drag texture → save → reopen → load resolves the texture correctly. Saved JSON uses logical ID once the manifest is populated.
- [x] Backward compat: pre-M1 scenes (relative-path texture) still load.
- [x] `m_World` removal compiles and `GetWorld()` continues to work.

## Out of scope (deferred)
- `AssetHandle` keyed on logical ID — revisit in M4 alongside the RHI seam so asset and renderer cleanups can ship together.
- Auto-running `AssetScanner` from engine init in non-editor builds — current pipeline already correct (editor scans → manifest committed → game ships manifest).
- `OpaaxLog::Init` consuming `EngineConfig::LogLevel()` — wire-up deferred to M8.
