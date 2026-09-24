# Plan — Editor M2d "ResourceTypes + ResourceBrowser"

> **Provenance:** fourth and final slice of the M2 program — see `.claude/plans/m2-panels.md` for the
> decomposition and cross-slice decisions (not repeated here). Base: `104f63c` (M2c complete).
> Structure follows the M1/M2a/M2b precedent. On approval this file is copied to
> `.claude/plans/m2d-resourcebrowser.md` and the checklist to `.claude/task/todo.md`.

## Context

M2a landed panel infrastructure (`PanelRegistry` + `EditorSelection`), M2b the drawer route
(`DrawerRegistry` + Inspector), M2c the engine `DebugDraw` + selection outline. Three of the five D10
routes are done; the fourth is still the counts-only `EditorRoute` from M0, with its `<int, int>`
placeholder sitting in `SandboxEditorModule.cpp`.

M2d graduates that route and gives it its first real consumer: a **ResourceBrowserPanel** listing the
edited project's content from disk. Per the overview's decision §3.5 (user, 2026-07-26) this is a **file
browser, not a catalog** — no manifest, no GUIDs, no `.meta` sidecars. The GUID-backed catalog is M3's
natural follow-on, when scene files need stable IDs.

Editor.md's M2 gate closed at M2b; this slice is content-row, not gate-row (overview F3).

**Four user steers shape this plan against the earlier draft:**
1. **"Resource", not "Asset"** — `Assets` is the *legacy* vocabulary (`Legacy/Assets`, the retired
   `IAsset`/`AssetRegistry` world). The live engine says `CResource` / `ResourceManager`, and
   `EditorContext` already names its member `Resources`. So the route becomes `ResourceTypes()`, the
   panel `ResourceBrowserPanel`, the data `ResourceFile`/`ResourceFolder`. The on-disk *directory* stays
   `Assets/` — that is `IPaths`' engine-side layout and D4 keeps the engine ignorant of the editor.
2. **Fix `IFileSystem` rather than route around it** — [[L18]]: a documented caveat is a bug with a
   comment on it. This slice needs directory enumeration; `IFileSystem` is the facility that should
   provide it and currently cannot. So the slice now **does touch engine files** (§1.1).
3. **More `OpaaxStringID`** — applied by a stated rule (§1.3).
4. **Browse the Editor assets too** — two roots, not one (§1.5).

The legacy `AssetBrowserPanel` (`Engine/Source/Legacy/Editor/Panels/`, X1 reference-only) is a rich
1060-line panel. What is worth taking — three view modes, breadcrumb, pinned toolbar — is in §1.5; what
is deliberately left behind is §4.

---

## 1. What lands

### 1.1 `IFileSystem` — repaired (MODIFIED, ENGINE — `Application/Services/Platforms/IFileSystem.{h,cpp}`)

Today the facility is unusable by construction, which is exactly why `EditorService.cpp:214` carries a
FIXME and calls `std::filesystem` directly:

- all three methods are **private** → zero callers are possible at all;
- they are **non-const**, while `IPlatform::GetFileSystem()` hands back a `const IFileSystem&` → even
  made public, a caller could not invoke them;
- `GetPathIfNCreate` guards `CreateDirectories` with try/catch but calls `IsPathExist` **outside** it —
  and `fs::exists(path)` throws on error, so the one hole left open is the one nothing guards;
- there is **no enumeration** at all, which is what a browser needs.

The repair, minimal and in that order:

```cpp
class OPAAX_API IFileSystem
{
public:
    struct Entry { OpaaxString Name; OpaaxString AbsPath; bool bIsDirectory = false; };

    bool        CreateDirectories(const OpaaxString& InPath)  const;
    bool        IsPathExist      (const OpaaxString& InPath)  const;
    OpaaxString GetPathIfNCreate (const OpaaxString& InPath)  const;

    /** One level, unsorted, no recursion — the caller composes the tree. @return false if InDirAbs is not a readable directory. */
    bool ListDirectory(const OpaaxString& InDirAbs, TDynArray<Entry>& OutEntries) const;
};
```

- **public + const.** Const is honest: the type is stateless (a `std::filesystem` façade), so
  `GetFileSystem()`'s `const&` needs no change and `IPlatform` is untouched.
- **`std::error_code` overloads throughout**, replacing the throwing calls + try/catch. That is the
  codebase's own idiom (`EditorService.cpp:220`, `Core/Config/ConfigIO.cpp`), and it closes the
  `IsPathExist` hole rather than documenting it.
- **`ListDirectory` stays primitive** — one level, no sort, no filter. Recursion, ordering and the tree
  live in the editor (§1.4). The engine gives a platform primitive; the editor builds the view.

**Then delete the FIXME by using it:** `EditorService::ResolveLayoutIniPath` swaps its direct
`fs::create_directories` for `GetPathIfNCreate`, and the `//TODO use FileSystem` comment goes with it.
That is the first real caller, so the facility ships proven rather than merely public.

> **Flagged, not fixed:** `IFileSystem` is not an interface — no virtuals beyond the dtor, held **by
> value** by `WindowsPlatform` and the null platform. The honest name is `FileSystem`. Renaming touches
> 5 files and 0 behavior; it is a cosmetic sweep that does not belong inside a panels slice, and I would
> rather hand you the observation than widen this diff. Say the word and it becomes S1b.

### 1.2 `ResourceTypeRegistry` (NEW — `Editor/Source/Editor/Extensions/ResourceTypeRegistry.h`)

Replaces `EditorRoute` behind `EditorExtensionRegistrar::ResourceTypes()`. Header-only, matching
`PanelRegistry.h` / `DrawerRegistry.h`.

```cpp
struct ResourceFile;     // fwd — Editor/Resources/ResourceScan.h
struct EditorContext;    // fwd — the activate closure receives it (D3: never the locator)

using FResourceActivate = TFunction<void(EditorContext&, const ResourceFile&)>;

struct ResourceTypeDesc
{
    OpaaxStringID     Extension;    // ".wave" — normalized then interned (§1.3)
    OpaaxStringID     Label;        // "Wave Definition" — displayed via ToString()
    OpaaxString       Icon;         // short text glyph, "[W]" (no icon font in the project)
    FResourceActivate OnActivate;   // optional; empty = the browser logs "no action registered"
};

class ResourceTypeRegistry
{
public:
    void                               Register(ResourceTypeDesc InDesc);      // normalizes, then stores
    const ResourceTypeDesc*            Find(OpaaxStringID InExt) const;        // nullptr = unregistered
    const TDynArray<ResourceTypeDesc>& Entries() const noexcept;
    Uint64                             Count()   const noexcept;   // keeps the seal log compiling
};
```

**Why a descriptor and not `Register<TResource, TActions>()`** (user decision): nothing here needs type
erasure. `DrawerRegistry`'s template exists because `TComponent` is a type the editor cannot name; a
resource "type" in a file browser is a key plus two labels and a closure. A template would be ceremony
that buys nothing. `Find` is a linear scan over a handful of entries, exactly as `DrawerRegistry` walks
its own.

Invariants: **I1** no static · **I5** owned by `EditorExtensionRegistrar` (a by-value `EditorService`
member) · **I2** the interning below crosses no DLL boundary, and could not break if it did — the
`OpaaxStringID` pool became a link-time singleton in `6e3cc4d` · **I6** plain class, no exported
template.

### 1.3 Where `OpaaxStringID` is used — and where it is not

The rule this slice applies, so it stays decidable later:
**`OpaaxStringID` for identities that are compared or keyed and come from a bounded set. `OpaaxString`
for free text and for paths.**

| Uses `OpaaxStringID` | Why |
|---|---|
| `ResourceTypeDesc::Extension`, `ResourceFile::Extension` | *the* lookup key — `Find` becomes an integer compare instead of a string compare per file per frame. One intern per distinct extension. |
| `ResourceTypeDesc::Label` | names the type; displayed via `ToString()`, so id and name live in one place — the `IEditorPanel::GetPanelID` convention. |
| `ResourceRoot::Label` ("Project" / "Editor") | compared when resolving the breadcrumb path; two values, ever. |
| the panel's own id (`OPAAX_ID("Resource Browser")`) | existing panel convention. |

| Stays `OpaaxString` | Why |
|---|---|
| `Name`, `RelPath`, `AbsPath` | **unbounded** — the pool is process-lifetime, so interning every path in a large project (and re-interning on every rescan) leaks monotonically for no gain. |
| `Icon` | a presentation glyph, not an identity; never compared. |

**Normalization happens before interning**, at both ends, or the ids cannot match: lower-case (Windows
paths are case-insensitive — `.PNG` and `.png` are one type) and a guaranteed leading dot, so a game
registering `"wave"` or `".WAVE"` still matches the scanner's `.wave`.

### 1.4 `ResourceScan` (NEW — `Editor/Source/Editor/Resources/ResourceScan.{h,cpp}`)

The directory walk, kept out of the panel so the panel is a pure view over data. **Includes no
`<filesystem>`** — it goes through `IFileSystem::ListDirectory` (§1.1).

```cpp
struct ResourceFile   { OpaaxString   Name;       // "Wave01.wave"
                        OpaaxString   RelPath;    // "Waves/Wave01.wave" (root-relative, '/')
                        OpaaxString   AbsPath;
                        OpaaxStringID Extension; };  // ".wave", normalized

struct ResourceFolder { OpaaxString Name; OpaaxString RelPath;
                        TDynArray<ResourceFolder> Folders;   // sorted by Name
                        TDynArray<ResourceFile>   Files; };  // sorted by Name

struct ResourceRoot   { OpaaxStringID  Label;     // "Project" | "Editor"
                        OpaaxString    AbsPath;
                        ResourceFolder Tree;
                        Uint64         FileCount = 0;
                        bool           bExists   = false; };

bool ScanRoot(const IFileSystem& InFS, ResourceRoot& InOutRoot);   // false = root dir absent
```

Recursion + sorting live here; sorted output gives a stable display order across rescans. Scanned **on
demand** — panel `Startup()` and the Refresh button — never per frame: all three views read this one
owned tree. No filesystem watcher (M2b §4 already sequenced "import → browser refresh" behind a future
editor bus).

### 1.5 `ResourceBrowserPanel` (NEW — `Editor/Source/Editor/Panels/ResourceBrowserPanel.{h,cpp}`)

A native `IEditorPanel`, registered in `EditorService::RegisterNativePanels()` through the **same**
`PanelRegistry` route a game panel uses — no privileged path (the property M2a exists to prove).
`Startup()` builds the root list and runs the first scan; `OnPreRender`/`Shutdown` are no-ops.

**Two roots** (user steer 4), built data-driven at `Startup()`:

| Label | Path | Source |
|---|---|---|
| `Project` | `<ProjectRoot>/Assets` | `IPaths::AssetsDir()` |
| `Editor` | `<ProjectRoot>/Editor/Assets` | `EditorPaths::EditorAssetsDir()` — skipped when the editor-paths pointer is null |

Layout — the legacy "freeze panes" shape, the part worth salvaging:

```
Begin("Resource Browser")
    DrawToolbar();                    // Refresh | Tiles/Tree/List toggle | text filter
    if (view == Tiles) DrawBreadcrumb();   //  Home > Project > Waves
    Separator();
    BeginChild("##Scroll")            // only the content scrolls; the toolbar stays pinned
        Tiles | Tree | List
    EndChild();
End();
```

**Three views over one tree** (the user's explicit ask):

| View | Shape | Filter behavior |
|---|---|---|
| **Tiles** | Explorer-style wrapped grid of the *current* folder; at Home the two roots are the tiles. Double-click a folder to enter, breadcrumb to walk back. A tile is a rounded card on the window draw list with the icon glyph centered + an ellipsis-truncated label. | filters files in the current folder |
| **Tree** | Both roots as top-level `TreeNodeEx`, open by default; recursive folders, `Selectable` file rows. | folders with no matching descendant are skipped; matching nodes force-open |
| **List** | Flat recursive list across both roots, each row showing `Root/RelPath` so it is unambiguous. | filters rows |

Tile column count comes from `GetContentRegionAvail().x` (legacy's arithmetic — correct and cheap).
The breadcrumb self-heals when a folder disappears between scans: walk as far as it resolves, truncate
the rest.

**Shared per-file behavior** (legacy's `ApplyAssetItemBehavior` — attached to the last-submitted item,
so one function serves both a tree `Selectable` and a tile `InvisibleButton`):
- single click → highlight (a local `m_SelectedRelPath`; it deliberately does **not** touch
  `EditorSelection`, which means "selected *entity*" and has an Inspector reading it)
- hover → tooltip: name, root-relative path, registered label (or "Unknown type")
- double-click → `ResourceTypeDesc::OnActivate(m_Context, file)` when registered

**Icon/label lookup is the only thing the panel asks the registry**: `Find(file.Extension)` → the
entry's `Icon`/`Label`, else the `"[ ? ]"` fallback. That is the whole coupling.

**Empty states are always explicit text** (L12), never a blank panel: missing root → "Folder not found:
`<abs>`"; nothing scanned → "No files under `<root>`."; filter matches nothing → "No resources match the
current filter."; empty folder in Tiles → "Empty folder."

**Discriminating logs** (L15 — the success branch, not just failures), all discrete, none per-frame:
- `Info` once per scan, per root: `Resource browser scanned '<label>' (<abs>): N files in M folders`
- `Warn` once per scan for a missing root
- `Info` on click: `Resource browser selected '<root>/<relpath>'`
- `Info` on double-click **with no registered type**: `'<name>' activated — no resource type registered
  for '<ext>'`. This is what makes S2 observable *before* any registration exists, and it is the line
  that flips to the game's own message in S3.

### 1.6 `EditorContext` + `EditorService` (MODIFIED)

```cpp
const IPaths&      Paths;                // M2d — AssetsDir() for the Project root
const EditorPaths* EditorPathsOrNull;    // M2d — EditorAssetsDir(); null when no edited project
```

The panel must never touch the locator (D3); `EditorService` is the composition root and already
resolves services there. The nullable pointer is honest: `ResolveLayoutIniPath`'s existing
`dynamic_cast<const EditorPaths*>` genuinely can fail (the editor falls back to plain `Paths` when no
edited project is declared).

**That cast is resolved once now**, into an `EditorService` member, and both `ResolveLayoutIniPath` and
the context read it — which removes the duplicate cast and satisfies your standing "cache the editor
path" TODO. *Flagging it because it is your TODO, in your file: say so and I will leave it alone and
cast twice instead.*

Also in `EditorService`: register `"Resource Browser"` in `RegisterNativePanels()`, and rename the seal
log's `assetTypes=` field to `resourceTypes=`.

### 1.7 Sandbox fixtures + dogfood

Fixtures (S2 — the browser must list something real; `Sandbox/Assets/` holds only a legacy
`AssetManifest.json`, and `Sandbox/Editor/Assets/` only a `.gitkeep`):

```
Sandbox/Assets/Waves/Wave01.wave        # tiny JSON-ish text; nothing parses these — browser fixtures
Sandbox/Assets/Waves/Wave02.wave
Sandbox/Assets/Textures/Readme.txt      # a second folder + an unregistered extension
Sandbox/Assets/AssetManifest.json       # already there — the unregistered-.json case
Sandbox/Editor/Assets/Icons/Notes.txt   # proves the second ROOT is live, not just configured
```

Dogfood (S3 — `SandboxEditorModule.cpp` only):

```cpp
InRegistrar.ResourceTypes().Register(Opaax::Editor::ResourceTypeDesc{
    .Extension  = OPAAX_ID(".wave"),
    .Label      = OPAAX_ID("Wave Definition"),
    .Icon       = "[W]",
    .OnActivate = [](Opaax::Editor::EditorContext&, const Opaax::Editor::ResourceFile& InFile)
    {
        OPAAX_LOG(LogSandboxEditorModule, Info, "Wave definition activated: {}", InFile.RelPath.CStr())
    }
});
```

---

## 2. Verified up front (not assumed — L16)

- **The M0 placeholder `AssetTypes().Register<int, int>()` cannot survive §1.2** — the route is renamed
  *and* `Register` becomes a non-template, so an explicit template argument list is a hard compile error
  ("not a function template"). Registry + placeholder removal are atomic, the F1 rule that forced the
  four-slice split. Handled in S2.
- **`EditorRoute` survives** — `Menus()` (M5) and `EditWorldSystems()` (M4) still use it. Its header
  comment names M2d for assets; that sentence is corrected in the same edit.
- **`Count()` keeps its signature**, so the five-count seal log compiles untouched, as `PanelRegistry`
  (M2a) and `DrawerRegistry` (M2b) both did.
- **Making `IFileSystem`'s methods public+const breaks no test.** `Engine/Tests/Core/Application/PathsTests.cpp:28`
  holds one `IFileSystem` **by value** inside its `StubPlatform` and only returns it from
  `GetFileSystem()` — it never calls a method, and a widened access specifier cannot break a
  by-value member.
- **`EditorContext` has exactly one construction site** (`EditorService::Initialize`, brace init), so two
  new members cost one line each.
- **`Engine/Assets/` exists with real content** (Fonts/Meshes/Shaders/Textures + legacy junk) — a third
  root is one line in a data-driven list. Deliberately not shipped: §4.

---

## 3. Steps (each builds green; commit at every boundary — L17)

**S0 — baseline (verify, do not assume).** `./build.bat debug-editor`, grep `OPAAX_BUILD_OK`; run the
test preset; smoke `Sandbox.exe`. Record test counts, the `Sandbox.exe` log-line count, and the seal-log
line. HEAD is a docs-only commit on top of M2c so this should be green — but a prior session's green is
not this session's truth ([[L14]]), and every later "unchanged" claim is measured against this.

**S1 — `IFileSystem` repaired (ENGINE).** §1.1 + `EditorService::ResolveLayoutIniPath` becomes its first
caller (FIXME + TODO deleted). New `Engine/Tests/Core/Application/FileSystemTests.cpp`: create-nested /
already-exists / empty-path / missing-dir-lists-false / list-separates-files-from-dirs, against a unique
`fs::temp_directory_path()` subdir cleaned up after. **Test count rises — the new number is recorded and
becomes the baseline for S2/S3.**
*Gate:* 3 presets green; new cases pass; `SandboxEditor.exe` still writes `Editor/Save/imgui.ini`
(the dock layout survives the swap — same path, different plumbing). **Commit.**

**S2 — registry + scan + panel + fixtures (editor-side; first browser).** `ResourceTypeRegistry.h`;
`ResourceScan.{h,cpp}`; `ResourceBrowserPanel.{h,cpp}`; `ResourceTypes()` returns the registry;
`EditorContext` grows `Paths` + `EditorPathsOrNull`; `EditorService` caches the cast, passes both,
registers the panel, renames the seal field; **remove** the `<int, int>` placeholder (§2); add the five
fixture files.
*Gate:* both roots list — `Project` shows `Waves/`, `Textures/`, `AssetManifest.json`; `Editor` shows
`Icons/Notes.txt`; all three views render; a folder tile double-click enters and the breadcrumb walks
back; the filter narrows all three; double-clicking `Wave01.wave` logs *"no resource type registered for
'.wave'"*; seal log reads `resourceTypes=0, panels=4`, `constructed: 4`.
**Commit** — S3's gate is a diff shape, so it must start from a clean tree.

**S3 — Sandbox resource type (dogfood).** One `Register(ResourceTypeDesc{...})` call in
`SandboxEditorModule.cpp`. **This step's diff must touch only `Sandbox/Editor/**`.**
*Gate:* both `.wave` files now show `[W] Wave Definition` in every view and in the tooltip; double-click
logs the *game's* line; `.json`/`.txt` still fall back to `[ ? ]`; seal log `resourceTypes=1`.
**Commit.**

**S4 — close-out.** `.claude/ARCHITECTURE.md` if the `IFileSystem` repair earns an invariant line;
`.claude/CLAUDE.local.md` ▶ NEXT rewritten (M2 complete → M3 Snapshot; the `EditorRoute` placeholder
tally drops to two; the IFileSystem entry leaves the open-TODO list); `.claude/plans/m2-panels.md` status
table; the slice plan filed at `.claude/plans/m2d-resourcebrowser.md`; `.claude/task/todo.md` review
section; lessons synthesis if any correction landed. **Pathspec commit** ([[L9]]) — never sweeping
`Docs/TODO.txt`.

**`Docs/Architectures/Editor.md` is yours, not mine.** D10's table row and example still read
`AssetTypes().Register<TAsset, TActions>()`, and the Asset→Resource rename widens that amendment. I will
hand you the exact replacement text in S4 rather than edit it — `Docs/` is your space. Say the word and
I will apply it instead.

---

## 4. Deliberately not in M2d (each independently addable later)

- **Folder colors + JSON persistence** (legacy's Unreal-style right-click picker) — ~120 lines and a
  file-IO surface. *(user decision)*
- **Type-filter buttons** — icon/label already prove the registry is consumed. *(user decision)*
- **Drag & drop source** — nothing consumes a resource reference yet, so it would ship untestable; it
  earns its place when a component drawer needs a resource field. *(user decision)*
- **An `Engine` root** — `Engine/Assets/` exists, but it mixes live shaders with legacy junk
  (`AssetManifest.json`, `Test.bin`) and nothing in the editor consumes engine content yet. One line in
  `Startup()`'s root list when you want it.
- **Thumbnails** — impossible today: they need a loaded `Texture2D`, and the `Texture` `CResource` is
  deferred (same reason `Renderer2D`'s sprite path is severed).
- **Load / Reload / Unload / "missing" status** — catalog concepts. A file browser's files exist by
  definition, and `ResourceManager` is load-by-path with no enumerate capability.
- **Renaming `IFileSystem` → `FileSystem`** — §1.1, cosmetic, 5 files, offered as S1b.
- **Details panel, "Create New", file watcher, rename/delete** — beyond a listing panel.

---

## 5. Verification

1. **3 presets** grep `OPAAX_BUILD_OK` (`debug-editor` / `release` / `release-editor`) — never the exit
   code ([[L8]]).
2. **Tests:** unchanged at S0's count through S2/S3; **S1 raises it** by the `FileSystemTests` cases and
   that new number is the baseline afterwards. A delta anywhere else means something leaked.
   *Known gap, stated plainly:* `ResourceScan` is pure logic worth testing and cannot be — the editor lib
   is not linked into `Engine/Tests`. Moving the `IFileSystem` primitive into the engine means the *file*
   layer is now covered; the tree-building layer is not. Not this slice.
3. **`Sandbox.exe` unchanged (D4)** at every step: same log-line count as S0, 3 quads, 1280x720,
   0 err/warn, zero editor/imgui/panel mentions. S1 changes the engine DLL, so this is the step where it
   matters most — the runtime calls none of the new code, and the gate proves it rather than assuming.
4. **`SandboxEditor.exe` observable gate per step** (§3), each pointing at specific UI *and* a log line —
   never "no errors" alone ([[L12]]).
5. **S3's diff touches only `Sandbox/Editor/**`** — changed + untracked paths filtered for anything
   outside it must come back empty (the M2a S3 / M2b S2 check).
6. Graceful window close → clean reverse teardown, 0 err/warn. The browser joins `m_Panels` and owns no
   GPU resource, so the existing reverse loop covers it; nothing new to order.
7. **The visual gate needs you** — I cannot drive a click from here. Every §3 gate is either a log line I
   can grep or an on-screen state I will hand you with the exact thing to look at.

---

## 6. Critical files

- **New:** `Editor/Source/Editor/Extensions/ResourceTypeRegistry.h`,
  `Editor/Source/Editor/Resources/ResourceScan.{h,cpp}`,
  `Editor/Source/Editor/Panels/ResourceBrowserPanel.{h,cpp}`,
  `Engine/Tests/Core/Application/FileSystemTests.cpp`,
  `Sandbox/Assets/Waves/Wave0{1,2}.wave`, `Sandbox/Assets/Textures/Readme.txt`,
  `Sandbox/Editor/Assets/Icons/Notes.txt`.
- **Modified (engine):** `Engine/Source/Application/Services/Platforms/IFileSystem.{h,cpp}` — the only
  engine change; `IPlatform` and every platform impl are untouched (§1.1).
- **Modified (editor):** `Editor/Source/Editor/Extensions/EditorExtensionRegistrar.h`,
  `Editor/Source/Editor/EditorContext.h`, `Editor/Source/Editor/EditorService.{h,cpp}`,
  `Sandbox/Editor/Source/SandboxEditor/SandboxEditorModule.cpp`.
- **CMake:** none — all three trees `GLOB_RECURSE ... CONFIGURE_DEPENDS`.
- **Reference only (X1 — do not depend on, do not migrate):**
  `Legacy/Editor/Panels/AssetBrowserPanel.{h,cpp}` (view modes, breadcrumb, tile drawing, frozen header —
  layout ideas), `Legacy/Editor/Assets/IAssetTypeActions.h` (the concept, now injected),
  `Legacy/Assets/AssetScanner.h` (manifest-coupled; shares only the exists-then-iterate shape).
