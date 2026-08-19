# Editor panels — one host, one identity, one toggle command

## Context

Panels are the last D10 route that never grew up. `Panels()` stores `{OpaaxStringID, lambda}`; everything
else about a panel — its name, whether it is visible, where it appears in the bar — is either duplicated
inside the panel class or does not exist.

**Five defects, verified in the code:**

1. **Identity is stated twice.** `Panels().Register("Hierarchy", …)` interns one id
   ([PanelRegistry.h:23](Editor/Source/Editor/Extensions/PanelRegistry.h:23)); `HierarchyPanel` interns
   another as `m_PanelID{OPAAX_ID("Hierarchy")}`
   ([HierarchyPanel.h:156](Editor/Source/Editor/Panels/HierarchyPanel.h:156)). Grepped: `GetPanelID()`
   has **7 overrides and 0 consumers** — the panel-side copy buys nothing.
2. **The Viewport is outside the collection**, so every panel loop is written twice —
   [EditorService.cpp:317](Editor/Source/Editor/Application/Services/EditorService.cpp:317), `:333`,
   `:493`, `:681`.
3. **Every panel repeats the window boilerplate** — `m_PanelID` + `m_Title` + `SetNextWindowSize` +
   `Begin`/`End`. Two carry a mid-function `ImGui::End(); return;`
   ([HierarchyPanel.cpp:75](Editor/Source/Editor/Panels/HierarchyPanel.cpp:75),
   [InspectorPanel.cpp:44](Editor/Source/Editor/Panels/InspectorPanel.cpp:44)).
4. **No visibility state at all** — no bool, no `p_open`, so no close button and nothing to tick.
5. **`FPanelFactory` is `MakeUnique<T>(ctx)` 7 times out of 7**, while every other registrar route is
   already `Register<T>(…)`.

**Outcome:** one registration line per panel carries identity, menu home and default visibility; one host
owns instances *and* visibility and is the only place that speaks ImGui window; a `Window` menu (or
`Tools`, per panel) toggles them.

**Decisions (user):** category `Window` · no cross-session persistence yet · Viewport joins the collection
· `EPanelVisibility { Visible, Hidden }` · the toggle is **one command taking the panel id as a param**.

---

## Design

### `PanelDesc` — NEW `Editor/Panels/PanelDesc.h`

Data only, no ImGui type — a game module includes it.

```cpp
enum class EPanelVisibility : Uint8 { Visible, Hidden };

struct PanelDesc
{
    OpaaxStringID    Id;                                          // identity + ImGui window label + dock key
    OpaaxStringID    Menu              = OPAAX_ID("Window");      // which root category holds the toggle
    EPanelVisibility DefaultVisibility = EPanelVisibility::Visible;
};
```

No per-panel tag: the id **is** the param, so one `Editor.Command.TogglePanel` serves every panel. That
also sidesteps `IsValidTagText` (**I14** refuses any byte `<= ' '`, so `"Play Controls"` could never have
been a tag segment).

`Menu` answers "a tool as a panel belongs under Tools" — `EditorMenu::Category` is already get-or-create
by interned id ([EditorMenuCategory.cpp:31](Editor/Source/Editor/Menus/EditorMenuCategory.cpp:31)), so a
panel toggle under `Tools` merges with the `Tools > Debug > Validate Sandbox` a game module registers.

### `PanelRegistry` v2 — MODIFY `Editor/Extensions/PanelRegistry.h`

```cpp
template<typename T>
concept CEditorPanel = std::derived_from<T, IEditorPanel> && std::constructible_from<T, EditorContext&>;

template<CEditorPanel T> void Register(PanelDesc InDesc);   // factory built internally
```

Verified: all 7 panels are `explicit T(EditorContext&)`. `Count()` unchanged — the seal log still reads it.

### `IEditorPanel` reshaped — MODIFY `Editor/Panels/IEditorPanel.h`

`Draw()` → `DrawContents()` (widgets only). **`GetPanelID()` deleted.** One new virtual carrying the only
two things that vary today (7 different `FirstUseEver` sizes; one `WindowPadding` push at
[ViewportPanel.cpp:134](Editor/Source/Editor/Panels/ViewportPanel.cpp:134)):

```cpp
struct PanelWindowStyle { Vector2F DefaultSize = {320.f, 400.f}; bool bNoPadding = false; };
virtual PanelWindowStyle GetWindowStyle() const { return {}; }
```

### `EditorPanels` — NEW `Editor/Panels/EditorPanels.{h,cpp}` — the host

Owns instances **and** visibility; the only place that speaks ImGui window.

```cpp
struct LivePanel { PanelDesc Desc; TUniquePtr<IEditorPanel> Panel; bool bVisible; };

void Build(const PanelRegistry&, EditorContext&);  // construct + Startup, registration order
void OnPreRender();                                // EVERY panel, visible or not
void Draw();                                       // Begin/End + visibility, ONCE
void Shutdown();                                   // reverse construction order (LC3)
void OnActiveWorldChanged(World*, World*);
bool IsVisible(OpaaxStringID) const noexcept;   void SetVisible(OpaaxStringID, bool);
```

`Draw()` is the collapse:

```cpp
for (LivePanel& lLive : m_Panels)
{
    if (!lLive.bVisible) { continue; }

    const PanelWindowStyle lStyle = lLive.Panel->GetWindowStyle();
    ImGui::SetNextWindowSize(ImVec2(lStyle.DefaultSize.x, lStyle.DefaultSize.y), ImGuiCond_FirstUseEver);
    if (lStyle.bNoPadding) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f)); }

    const bool lOpen = ImGui::Begin(lLive.Desc.Id.CStr(), &lLive.bVisible);
    if (lStyle.bNoPadding) { ImGui::PopStyleVar(); }

    if (lOpen) { lLive.Panel->DrawContents(); }
    ImGui::End();                                  // unconditional — ImGui's contract
}
```

Close button and menu tick read the **same bool**, by construction. `Id.CStr()` points into the intern
pool, valid for the process lifetime (**I2**) — what ImGui needs of a label it keeps for the ini.

`EditorContext` gains `EditorPanels& Panels`.

### `SetParams` — a third facet — MODIFY `EditorMenuCommandNode`; NEW `Editor/Commands/EditorCommandParams.h`

`EditorCommandRegistry::Execute<TParams>` is a template, so the node must hold its payload with the static
type intact. ~20 lines, deliberately placed in `Commands/` rather than `Menus/` because a future key→tag
table needs exactly the same box:

```cpp
struct IEditorCommandParams
{
    virtual ~IEditorCommandParams() = default;
    virtual void Dispatch(const EditorCommandRegistry&, const OpaaxTag&, EditorContext&) const = 0;
};

template<typename T>
struct EditorCommandParamsBox final : IEditorCommandParams
{
    T Value;
    void Dispatch(const EditorCommandRegistry& InReg, const OpaaxTag& InTag, EditorContext& InCtx) const override
    { InReg.Execute(InTag, InCtx, Value); }
};
```

`EditorMenuCommandNode` gains `TUniquePtr<IEditorCommandParams> m_Params` and a **third facet** beside
`SetEnabled` / `SetChecked`, chaining the same way. `AddCommand` and `EditorMenuCategory` are **not
touched** — the payload is a property of the entry, discovered from which facets were set, which is the
doctrine the node's own header already states:

```cpp
template<typename TParams>
EditorMenuCommandNode& SetParams(TParams InParams)
{
    m_Params = MakeUnique<EditorCommandParamsBox<TParams>>(Move(InParams));
    return *this;
}
```

Null `m_Params` means `NoParams`, so every existing entry's `Draw` path is unchanged.

**This amends MR2b, which rejected params on a node** — and the amendment is the point, not a workaround.
That rejection was reasoned from `QuitCommand`'s `Window*`: *a payload only the composition root can
supply is a payload a key binding cannot carry*. `PanelIdParams{OpaaxStringID}` is the opposite — plain
interned data, expressible verbatim in a binding table. The invariant needs the **class of payload**
named, not a blanket ban. `Window*` remains forbidden and `EditorContext::MainWindow` stays the answer.

### `TogglePanelCommand` — in the existing `EditorNativeCommands.{h,cpp}`

```cpp
struct PanelIdParams { OpaaxStringID PanelId; };

struct TogglePanelCommand
{
    using Params = PanelIdParams;
    void Execute(EditorContext&, const Params&);   // -> Panels.SetVisible(id, !IsVisible(id))
};
```

Stateless and default-constructible like every other command; registered **once** in
`RegisterNativeEditorCommand` under `Tags::EDITOR_COMMAND_TOGGLE_PANEL`.

### Viewport joins; hover/focus is PUSHED — MODIFY `ViewportPanel`, `InputRoute`

The only thing keeping the Viewport out is `EditorService` reaching in for `IsHovered()`/`IsFocused()` —
grepped: exactly two consumers, [EditorService.cpp:306](Editor/Source/Editor/Application/Services/EditorService.cpp:306)
and `:364` (the `Legacy/Editor` hits are a different, unlinked class). Invert it: `InputRoute` gains
`SetViewportFocus(bool,bool)` + `IsViewportHovered()`, `Evaluate()` loses its two parameters,
`ViewportPanel::DrawContents` pushes the measured values, and `IsHovered()`/`IsFocused()` are deleted.
Sourcing, not mirroring — [EditorContext.h:49](Editor/Source/Editor/EditorContext.h:49) already calls
`InputRoute` *"the one place that answers is the engine being fed"*.

**A real bug this exposes.** A hidden panel does not draw, so `m_bHovered` keeps last frame's `true`
forever and the route stays open with the Viewport closed — [[L28]]'s shape, invisible until a panel *can*
be hidden. Fix: `OnPreRender` (unconditional) pushes `false`; `DrawContents` pushes the measured values.
`BeginFrame`'s order stays correct — `Evaluate` reads last frame's values *before* `OnPreRender` clears.

**Teardown order flips and that is safe.** Registered first ⇒ reverse-order shutdown (LC3) destroys it
last. It is the only GPU owner; the UI backend and `ImGui::DestroyContext` still run after every panel, so
the FBO is released while the device and GL context are alive (**F2a**). Startup order is unchanged —
natives register before any module (**MR2**).

### `BindPanelToggles` — NEW in `EditorService`, ~8 lines

In `RegisterExtensions`, after `InCollect` (game panels are in) and before `Seal()`:

```cpp
for (const PanelEntry& lEntry : m_Extensions.Panels().Entries())
{
    const PanelDesc& lDesc = lEntry.Desc;
    m_Extensions.Menus().Category(lDesc.Menu)
                .AddCommand(lDesc.Id, Tags::EDITOR_COMMAND_TOGGLE_PANEL)
                .SetParams(PanelIdParams{ lDesc.Id })
                .SetChecked([lId = lDesc.Id](const EditorContext& InContext)
                            { return InContext.Panels.IsVisible(lId); });
}
```

`SetChecked` already exists and already works (`Play > Pause` uses it). **A game panel gets its toggle
with zero `OpaaxEditorLib` changes** — the M2a diff-gate property, preserved. `DrawDockspace` runs before
the panel loop in `EndFrame`, so a click takes effect the same frame.

### `EditorService` shrinks

`m_ViewportPanel` and `m_Panels` both go; one `TUniquePtr<EditorPanels> m_PanelHost` replaces them, built
empty **before** the context (so the context can hold the reference) and populated after — the ordering
`m_Selection` / `m_PIE` / `m_InputRoute` already use. Four mirrored loops become four delegations.

---

## Steps (four commits)

S4's gate is a **diff shape**, so it must start from a clean tree ([[L17]]).

1. **S1 — the host.** `PanelDesc`, `PanelRegistry` v2, `IEditorPanel` reshape, all 7 panels converted,
   `EditorPanels`, `EditorService` delegating. Viewport still a named member; `Begin` gets **`nullptr`**
   for `p_open` and every panel is `Visible` — today's behaviour, none of the duplication. Deliberate: an
   intermediate that can hide a panel with no way back is a state I would not ship.
2. **S2 — the Viewport joins.** Registered first; hover/focus pushed into `InputRoute`.
3. **S3 — the toggles.** `EditorCommandParams.h`, the `SetParams` facet, `TogglePanelCommand` + tag,
   `BindPanelToggles`, `Begin` switched to `&bVisible`, Input panel defaults `Hidden`.
4. **S4 — dogfood.** `SandboxPanel` → `.Menu = OPAAX_ID("Tools")`, `.DefaultVisibility = Hidden`.

## Verification

`OpaaxTests` cannot reach editor code (standing M2a gap), and per [[L22]] a test constructing the subject
directly could not verify this wiring anyway — correctness here is *where in boot the toggles are bound*
and *whether two front-ends share one bool*. Gates are the real editor plus an ordered log, per step.

| Step | Gate |
|------|------|
| S1 | 3 presets `OPAAX_BUILD_OK`; tests **339 / 6532 / 7** unchanged; editor boots 0 err/warn; all 7 panels present at the same sizes; **existing `imgui.ini` dock layout preserved** (window labels are unchanged strings) |
| S2 | The existing `RouteInput:` Trace lines are **identical** to S1's for the same interaction — over the UI `CONSUMED`, over the viewport `passed to engine`. That log already discriminates ([[L12]]) |
| S3 | `Window` menu lists every panel with a live tick; clicking toggles; **the X button flips the same tick**; Input starts hidden and the menu brings it back; restart returns to defaults. Seal log gains one line per panel (`id / menu / default`), the success branch per [[L15]]. `commands=` rises by exactly **1** |
| S4 | `git diff --name-only` touches **nothing outside `Sandbox/Editor/`**; `Tools` holds both `Debug > Validate Sandbox` and the panel toggle |

Operational: `OPAAX_NO_PAUSE=1`; grep `OPAAX_BUILD_OK`, never the exit code ([[L8]]); `ls -la` the exe
against the DLL before smoke-testing ([[L24]]); read the whole log ([[L27]]); graceful `taskkill` so the
layout flushes. **`imgui.ini` is the user's own gitignored, untracked file and is never deleted** ([[L20]]).

### Docs to fix in the same change (CLAUDE.md §0)

- **MR2b amended** — params are allowed when the payload is plain data a binding could carry; a
  composition-root-only payload (`Window*`) stays forbidden. This is the one contract change here.
- `PanelRegistry.h`'s comment naming the deleted `IEditorPanel::GetPanelID`.
- `Docs/Architectures/Editor.md` D10 row (line 273) **and its example call site** (line 285), still
  showing `Panels().Register("Wave Designer", …)`.
- New **MR2c** for the `PanelDesc` contract; SE table's `OnModulesRegistered` row for `BindPanelToggles`.
- On approval, sync to `.claude/plans/editor-panels.md` (project convention; an earlier draft is there).

## Deliberately NOT built

- **Persistence of visibility** — user's call. Trigger: the first hidden panel that must be reopened every
  launch.
- **A `Closable = false` flag.** Closing the Viewport leaves the render target bound and the world drawing
  into an FBO nobody samples — wasteful, never wrong — and the menu is the way back.
- **Multi-level `.Menu`** (a path of ids). One id today; `Category()`/`SubCategory()` already express more.
- **Shortcuts.** [[L37]]/[[L38]]'s answer stands: a key→tag table is its own system. It will reuse
  `EditorCommandParams.h`, which is why that box is in `Commands/`.
- **A panel instantiated twice** (two Inspectors). Needs an instance id and a different dock-label scheme.

## Risks

- **`ImGui::Begin` must be paired with `End` even when it returns false** — the loop does this
  unconditionally. It is also the hazard the current per-panel code carries twice; centralising removes it.
- **The params box is checked at DISPATCH, not compile time** — same trade `EditorCommandRegistry` already
  makes, and the typeid gate turns a mismatch into a logged refusal. A wrong params type on a menu entry
  is therefore a Warn at click time, not a crash.
- **`PanelDesc::Menu`'s default member initializer calls `OPAAX_ID`**, which interns. Safe: descs are
  constructed at registration, well past static init.
