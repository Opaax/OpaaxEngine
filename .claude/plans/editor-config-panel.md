# A Config panel — Unreal's Project Settings shape

## Context

The engine has a config registry (`IConfigSystem`, `Engine/Source/Application/Services/IConfigSystem.h`)
that nothing in the editor can see. Two configs are live today — `Config_Engine` (registered in
`OpaaxApplication::PreRegisterConfig`) and `Config_Renderer` (auto-registered by `RendererManager::Startup`
through `Get<Config_Renderer>()`) — and the only way to look at either is to open its `.config` file by hand.

The ask, after the fork: **one panel, Unreal Project Settings shaped.** A list of every registered config on
the left; clicking a title makes it current; the right pane draws it. A menu category listing configs is
*not* built — the panel is the listing, and it gets its menu entry for free from `BindPanelToggles`
(`PanelDesc::Menu`), which is the route every panel already travels.

Two facts shape the design:

- **The registry is not sealed and not ordered.** `Get<T>()` auto-registers on a miss, so a game system
  reading its config during `FinishStartup` — or on frame 500 — registers *after* the editor's extension
  seal. The panel therefore reads the registry **live, every frame**, and a config added later simply
  appears. Storage is also a `TUnorderedMap` keyed on a `uintptr_t` tag, so today's list order would be
  hash order — it needs to become registration order.
- **Nothing can enumerate the registry, and nothing can render an `IConfig`.** `IConfigSystem` is
  type-keyed (`Get<T>()`), and `IConfig` exposes only `FileName` / `Load` / `Save`. Both gaps are one small
  addition each, below.

## Engine — make the registry ordered, enumerable, and printable

**1. `IConfigSystem` owns the storage** (`Application/Services/IConfigSystem.{h,cpp}`)

The `TUnorderedMap<ConfigTypeID, TUniquePtr<IConfig>>` and the `FindOrCreate` body are currently duplicated
in `ConfigSystem` and in `NullConfigSystem`. Move both into the base:

- `TDynArray<TUniquePtr<IConfig>> m_Configs` in **registration order** — deterministic list order, and a
  linear scan beats hashing at this cardinality.
- `FindOrCreate` becomes the shared, **non-virtual** skeleton. The only difference between the two systems
  is what happens to a config that was just created, so that becomes one hook,
  `virtual void OnConfigRegistered(IConfig&)`: `ConfigSystem` resolves `ConfigsDir()/FileName()` and `Load`s
  it (today's behaviour, log included), `NullConfigSystem` does nothing.
- `SaveConfig` / `SaveAll` **stay virtual** — the null system must keep refusing to save, which the existing
  `CHECK_FALSE(lSys.Save<TestConfig>())` pins. `NullConfigSystem` collapses to three one-liners.
- New, public: `const TDynArray<TUniquePtr<IConfig>>& GetConfigs() const` (the `PanelRegistry::Entries()` /
  `DrawerRegistry::Entries()` idiom) and `IConfig* FindConfig(ConfigTypeID) const`.
- Copy/move `= delete` on `IConfigSystem`: it is `OPAAX_API` and now holds a move-only member — the **I6
  corollary** (C2280 on the implicit copy-assign even though nothing copies one).

**2. `IConfig` gains a name and a text form** (`Core/Config/IConfig.{h,cpp}`, `Core/Config/TConfig.hpp`)

- `OpaaxStringID GetName() const` — the file name's stem through `PathString::Stem`
  (`Core/String/OpaaxPathString.h`), so `"Engine.config"` → `Engine`. **I13's one stem rule**: a
  hand-written second display name is a second copy of a naming rule, and those drift. Interned rather than
  a view because `CStr()` is then a pool pointer valid for the life of the process (**I2**) — what ImGui is
  handed — while a `Stem` view is not null-terminated and would render `Engine.config`.
- `virtual OpaaxString ToText() const = 0`, implemented **once** in `TConfig<TData>` as
  `TConfigCodec<TData>::ToText(m_Data)` — the same codec that writes the file (both configs `dump(4)`, so it
  is already pretty-printed). This is what lets the panel draw a config it cannot name the type of.

## Editor — the panel

**3. `Editor/Source/Editor/Panels/ConfigPanel.{h,cpp}`** — two panes, `ImGui::BeginChild` left of a
`SameLine`d right:

- **Left:** one `ImGui::Selectable` per entry of `Context.Configs.GetConfigs()`, labelled
  `GetName().CStr()`, walked **every frame** (see Context). Clicking sets `m_Current` = that config's
  `ConfigTypeID`. Empty registry draws a disabled `(none)`.
- **Right:** the current config resolved through `FindConfig(m_Current)` each frame — null (nothing selected
  yet, first frame) falls back to the first entry, so the panel is never blank when configs exist. Draws its
  name, its `FileName()`, and its `ToText()` **read-only** in a scrolling child.
- `ToText()` is re-serialized per frame rather than cached: a cache here would need an invalidation nobody
  owns, and this costs a few µs only while the panel is open.
- **Read-only is the point of this slice.** Editing is the reflection system's job (next), and a JSON
  textbox would be a thing reflection has to delete. What the pane gives today is the first way to *see*
  live config values at all.

**4. `EditorContext` += `IConfigSystem& Configs`** (`Editor/EditorContext.h`), resolved once in
`EditorService::Initialize` beside `Paths` / `FileSystem` / `MainWindow` — the composition root resolves,
nothing downstream touches the locator (**D3**).

**5. Register it** in `EditorService::RegisterNativePanels`:
`lPanels.Register<ConfigPanel>(PanelDesc{ .Id = OPAAX_ID("Config") })`. That is the whole menu story — the
Window entry, its tick and its close button all come from `BindPanelToggles` + `EditorPanels`, no privileged
path (**D10**, **MR2c**). Visible by default like every panel but `Input`, so the change is visible on
launch and smoke-verifiable.

## Verification

- `Engine/Tests/Core/Application/IConfigSystemTests.cpp` — extend with: registration **order** preserved
  across two config types, `FindConfig` hit and miss, `GetName()` == the file stem, `ToText()` round-trips
  through the codec. The four existing cases must stay green, in particular the null system's "does not
  load" and "refuses to save".
- `build.bat` on all three presets; verdict by grepping `OPAAX_BUILD_OK`, never the exit code (**L8**). Run
  `OpaaxTests.exe` directly (CTest reports one test).
- Editor smoke: **back up `Sandbox/Editor/Save/imgui.ini` first** (a run flushes the smoke window's geometry
  into it), `ls -la` the exe against the DLL (**L24**), launch `SandboxEditor.exe`, and confirm in
  `Sandbox/Save/Log/OpaaxEngine.log` that panels went 7 → 8 and the editor booted clean. The two-pane
  content itself is a look-at-it gate: the panel must list **Engine** and **Renderer**, and clicking each
  must swap the right pane. Restore `imgui.ini` afterwards.
- Docs in the same change (CLAUDE.md §0): a line in `Docs/Architectures/Editor.md` for the panel, and the
  config registry's ordering guarantee into `.claude/ARCHITECTURE.md` if it proves load-bearing beyond this.

## Explicitly not in this slice

Editing a config, saving from the panel, and any per-field widget — that is the CryEngine-style reflection
system, which is next and which this panel is the shell for.

---

## Review — LANDED 2026-08-19, committed on `refresh_engine`

Built as planned, with one addition: **`ConfigPanel` logs the config it is showing**, once per change
(`Showing 'Engine' (Engine.config, 734 bytes)`). The right pane's failure mode is an empty rectangle,
which no error log discriminates from a clean run — the success branch had to say so (**L15**), and it is
also the instrument for the one leg a machine cannot check (clicking `Renderer` must print a second line).

**Gates.**
- Tests **341 / 6542 / 7 skipped**, 0 failed (up from 339 / 6532 — three new cases: registration order,
  `FindConfig` hit/miss, `GetName`/`ToText`). The four pre-existing cases stayed green, including the null
  system's "does not load" and "refuses to save", which is what kept `SaveConfig` virtual.
- **`OPAAX_BUILD_OK` on all three presets** (debug-editor, debug, release).
- Editor smoke: `panels 7 → 8`, `menus 20 → 21` (the free Window toggle), **0 err/warn**, and the ini the
  run wrote contained `[Window][Config]` + `[Window][Config/##list_…]` — the panel window *and* its list
  child drew. `imgui.ini` restored byte-identical afterwards.

**Still owed:** the click itself. Selecting `Renderer` swapping the right pane is a look-at-it gate.
