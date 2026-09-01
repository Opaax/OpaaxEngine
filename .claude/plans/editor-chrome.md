# Editor Chrome — record (2026-09-01)

A block inserted before ⑤ Undo/Redo, at the user's request: *"Custom TitleBar (our currently Menus
in registar). Panels, menus, dialogs with windows etc.... Should be GUI."*

**Five commits.** `cf66820` (the user's own, unsigned) · `f490e47` S1 · `42a8b95` S2 · `1746465` S3 ·
`19d26f4` S4. Durable in **MR2d** (amended), **MR2e**, **MR2f**.
**Gates: 494 / 7248 / 7, three presets, zero warnings tree-wide.**

---

## What it was

**MR2d** had landed the `IEditorGui` seam — one UI pass, ~20 chrome calls behind one interface — and
left three things:

1. the gui **looked up** what it drew (`InContext.Extensions.Menus()`, `InContext.Panels`) instead of
   being handed it; the user's own WIP TODOs said so;
2. there was **no title bar** — the menus sat in an `ImGui::BeginMainMenuBar` below the OS caption;
3. **dialogs bypassed the seam entirely** — six `tinyfd_*` calls inside command bodies.

## Three forks, decided with the user before any code

| Fork | Chosen | Why |
|---|---|---|
| Frame depth | **GLFW-only first, Win32 `WM_NCCALCSIZE` later** | The editor side is written against `Window` verbs, so the upgrade touches `WindowsWindow` only. Aero Snap, shadow and rounded corners are the stated price. |
| Dialogs | **Native, behind a seam, callback-shaped** | Cheap now; an in-editor implementation becomes a class swap rather than rewriting six call sites. |
| Bar contents | **The whole `Menus()` tree + Min/Max/Close** | The menu registry IS the extension point. No title-bar registry, no PIE transport. |

The user's own framing on the third: *"I wasnt aware that in windows the bar the 'Quit' 'Minimize'
'Maximize' was called 'TitleBar'. So its another 'refacto' on this but its worth to have a common
name with others apps."* — the vocabulary was the point as much as the feature.

## What the exploration turned up before planning

- **`WindowManager::CreateMainWindow` builds `WindowProps` from `EngineConfigData` alone** — no host
  seam — which is what forced decoration to be a *runtime* verb rather than a props field.
- **`SetWindowed()` hard-set `GLFW_DECORATED = TRUE`**, so decoration was welded to `EWindowMode`.
- **`WindowData::PosX/PosY` were `Uint32`** while `SaveWindowedState` writes `glfwGetWindowPos` into
  them. Latent until something dragged the window.
- **`ImguiLayout.h` calls itself the testable half but includes `imgui.h`**, which `OpaaxTests` may
  not pull — so the frame geometry needed its own header to be testable at all.

## The five steps

- **S1** — `IEditorGui` gains protected `m_Menu`/`m_Panels` + non-virtual `SetMenu`/`SetPanels`;
  bound at `PostInitialized`. Closes both user TODOs.
- **S2** — decoration as its own axis; `Window` grows position/size/minimize/maximize/restore;
  `PosX/PosY` → `Int32`.
- **S3** — the title bar. `ImGuiTitleBar` + the ImGui-free `WindowFrameGeometry.h` + 8 tests.
- **S4** — `IEditorDialogs` / `TinyFdEditorDialogs`; six call sites rewritten.
- **S5** — this record and the contract.

## Two things worth remembering

**Replacing `DockSpaceOverViewport` nearly destroyed the user's dock layout.** The dockspace id is
`GetID("DockSpace")` *seeded by a window literally labelled* `WindowOverViewport_%08X`. Open-coding
the helper without copying both would have silently orphaned every saved dock position — regenerable
in principle, but not the layout they had ([[L20]]). Caught by reading imgui's implementation before
writing the replacement, and verified afterwards by diffing the ini's `[Docking]` section: same
`0x08BD597D`, identical node tree. → [[L69]]

**A green test suite proved nothing about the tests I had just written.** `Engine/Tests/CMakeLists.txt`
lists sources **explicitly**, so the new file was never compiled; and `build.bat test` targets the
**debug-editor** preset while I was running `build/debug`'s binary. Both were caught by the same
instrument — the **case count**, not the pass/fail. → [[L70]]

## Deviation from the approved plan

The plan said rename `BeginMainMenuBar` → `BeginMenuBar`. Building it showed the rename is
impossible-in-spirit: if `EditorMenu` both opens and closes the bar, the window buttons can never
share the row. The two calls **left the seam** instead. Smaller seam, and `EditorMenu` stopped
knowing where its bar is. Recorded in **MR2e**.

## Owed — the user's eyes, and a smoke run reaches none of it

Confirmed in passing (*"seems to work"*) but **never ticked one-by-one**: drag across a second
monitor and to a negative X · double-click maximize/restore · all 8 resize regions with corner
priority and the min-size clamp · maximized not covering the taskbar · undock/redock a panel · every
File dialog opening where it used to, Cancel a no-op, the Open Level confirm still blocking · whether
the OS caption flashes at boot.

## Named, not built

- **Win32 `WM_NCCALCSIZE`** — the chosen follow-up; expect to want it for snap and edge-resize feel.
- `OpaaxApplication::OnConfigureWindow(WindowProps&)` — only if the boot caption flash is visible.
- A three-button `EDialogAnswer` (Save / Don't Save / Cancel) — one enumerator plus `"yesnocancel"`.
- Dragging a *maximized* window does not restore-and-follow the way Windows does.
- The ~360 widget call sites stay direct (**MR2d** already ruled).
