# EditorGui — wrapping ImGui behind one editor class

## Context

`EditorService` is the editor's composition root (D3), and it had also become — by accretion — the
editor's ImGui integration file: 18 raw ImGui calls across five unrelated methods (context
create/config/destroy, `NewFrame`/`Render`, the dockspace, `GetIO().WantCapture*`, `GetTime()`, six
`ImGui::Shortcut` chords), plus ownership of the `IEditorUIBackend` and of the borrowed
`io.IniFilename` string whose lifetime rule was a comment on a service member.

`EditorGui` finishes the wrap the user had started. **The service now delegates to it exactly as it
delegates the menu bar to `EditorMenu`**, and names no ImGui symbol of its own.

Two boundary calls, both the user's:
- **Total wrap**, shortcuts included — they speak `EKeyCode`, the vocabulary `HandleReservedKeys`
  already used, so both shortcut paths speak one language.
- `EditorGui` owns the **UI backend and the layout-ini string**.

## What landed

**`Editor/Source/Editor/Imgui/EditorGui.{h,cpp}`** — the boundary.

- **Lifecycle** — `Init(Window&, OpaaxString iniPath)` / `Shutdown()` / `IsReady()`. `Init` takes a
  `Window&`, so the `GetNativeWindow()` → `GLFWwindow*` cast lives beside the backend construction,
  the only place that already knows GLFW. The native handle is checked **before** `CreateContext`, so
  a failure has nothing to unwind.
- **Frame** — `BeginFrame()` (backend → ImGui → `ImGuizmo::BeginFrame`), `EndFrame()` (`Render` →
  `RenderDrawData` → platform windows), `DrawDockspace()`.
- **Query** — `GetTime()`, `IsPointerOverUI()`, `IsKeyboardOwnedByUI()`, `Shortcut(mod, key)` /
  `Shortcut(key)`.
- **`m_LayoutIniPath` moved in.** ImGui *borrows* `io.IniFilename` and writes through it at
  `DestroyContext`, so the string now lives on the class that does the borrowing and the destroying.
  Resolution stays in `EditorService::ResolveLayoutIniPath()` (it needs `EditorPaths` +
  `IFileSystem`); only the storage moved, along with the `Dock layout: {}` log under a new
  `LogEditorGui` category.

**The two capture predicates were RENAMED, not just relocated.** `WantCaptureMouse` answers *"the
pointer is over some ImGui window"* — and in this editor one of those windows is the game. That
mismatch is [[L29]]/**SEL8**, the theme behind three of ②'s four interactive bugs. `IsPointerOverUI()`
says what the flag actually answers, and its doc states that a caller who means *"should the UI own
this click"* still owes its own viewport carve-out — which `RouteInput` still has, unchanged.

**Shortcuts speak `EKeyCode`.** `ToImGuiKey` is private to `EditorGui.cpp`, deliberately: publishing
it would hand every caller a way around the boundary this class exists to create (the one place this
departs from the standing "generic helpers go in a findable folder" rule — `Editor/ImguiLibrary/`
serves panels, which legitimately call ImGui). It is **total over the keyboard**, not just the six
keys in use — a partial table answering "no such key" for the next shortcut anyone adds would fail
silently ([[L15]]). `EKeyCode` follows GLFW's numbering, so the contiguous runs (`A–Z`, `0–9`,
`F1–F12`, `Numpad_Zero–Nine`) are walked by arithmetic exactly as `imgui_impl_glfw.cpp` does it; the
rest is by name. A non-keyboard code answers `ImGuiKey_None`, which `Shortcut` asserts on. Modifiers
are keys (`InputManager`'s own rule), and Left/Right fold together because ImGui's chord mods are
side-agnostic.

**`EditorService`** lost `m_UIBackend` and `m_LayoutIniPath` for one `EditorGui m_Gui` (by value,
like `m_Extensions`), and its `<imgui.h>` / `<ImGuizmo.h>` / `OpenGLEditorUIBackend.h` includes.
Every guard became `m_Gui.IsReady()`; `DrawDockspace()` survives as the "chrome outside a panel"
grouping. `EditorContext.UIBackend` is now `m_Gui.Backend()` — the struct and every panel reading it
are untouched.

## Verified

| Gate | Result |
|---|---|
| Baseline `debug-editor` **before** any edit ([[L13]]) | `OPAAX_BUILD_OK` |
| `debug-editor` after | `OPAAX_BUILD_OK`, **zero warnings** |
| No ImGui symbol in `EditorService.h`/`.cpp` | clean (`ImGui::`, `ImGuizmo::`, `ImGuiKey`, `ImGuiIO`, `#include <imgui`) |
| `OpaaxTests.exe` | **427 / 7004 / 7 skipped**, 0 failed |
| Exe newer than DLL ([[L24]]) | exe 15:50, DLL 15:29 (engine untouched) |
| Editor smoke, whole log read | 185 lines, `[EditorGui] Dock layout: …`, extensions sealed, panels **8/8**, `Viewport displaying world FBO`, clean teardown through `EditorService shutdown`. The one warning is the pre-existing `Main.opaaxmap` re-serialization churn from the user's own uncommitted edits. |
| `Sandbox/Editor/Save/imgui.ini` | backed up before, **byte-identical** after |

The user drove the window during the smoke run and closed it themselves — the log carries their
`Viewport marquee took 1 entity(ies)` and the renamed Trace fields
(`PointerOverUI=true, KeyboardOwnedByUI=false`), so the route, the capture predicates and the
viewport carve-out were all exercised for real rather than merely booted.

**Still owed — the eye gates a smoke run cannot reach** ([[L53]]): Ctrl+S clearing the `*`, F and
Delete from the *Hierarchy*, W/E/R switching gizmo mode, a bare letter doing nothing while a name
field has focus, and the dock layout surviving a full restart.

## Found in passing, NOT fixed

`Engine/Source/Engine/Subsystems/Input/InputCodes.h` claims *"Values follow the GLFW/SDL numbering …
so backend translation is a plain cast"*, and for the numpad **operators** it does not:

| EKeyCode | value | GLFW at that value |
|---|---|---|
| `Numpad_Add` | 330 | `KP_DECIMAL` |
| `Numpad_Subtract` | 331 | `KP_DIVIDE` |
| `Numpad_Multiply` | 332 | `KP_MULTIPLY` ✅ (by luck) |
| `Numpad_Divide` | 333 | `KP_SUBTRACT` |
| `Numpad_Enter` | 334 | `KP_ADD` |
| `Numpad_Decimal` | 335 | `KP_ENTER` |

So a real numpad `+` arrives from GLFW as 334 and the engine labels it `Numpad_Enter`. `ToImGuiKey`
maps by *enumerator*, so it is correct regardless — the defect is upstream, in engine input, and its
own change. `Numpad_Zero–Nine` (320–329) are correct.
