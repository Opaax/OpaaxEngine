# Plan — Editor M0 "Shell"

> **Provenance:** RECONSTRUCTED 2026-07-18 from the surviving sources after the original plan-mode mirror
> was lost with `.claude/plans/`. Faithful to `Docs/Architectures/Editor.md` (design, v3 frozen) +
> `.claude/task/todo.md` (per-step outcomes). If any step disagrees with those, they win.
>
> **Role of this file:** the *execution strategy* for M0 — why the work is decomposed S0→S12 in this
> order and what each gate proves. It is NOT the design (→ `Editor.md`) and NOT the live checklist
> (→ `todo.md`). Invariants it leans on live in `.claude/ARCHITECTURE.md`.
>
> **Branch:** `refresh_engine` · **Design source:** `Docs/Architectures/Editor.md` §5 (M0 row), all D-decisions.

---

## Objective (M0 gate — Editor.md §5)

> **Done means:** dockspace + input capture work; `Sandbox.exe` behavior is **identical** after the split;
> runtime targets carry **zero** editor code (no ImGui symbol in `Sandbox.exe` or `OpaaxEngine.dll`).

M0 is the *shell*: it stands up the editor as a **library + thin exe** composition and proves the seams,
the link topology, and the event/present plumbing — while the engine stays byte-for-byte a runtime engine.
No real panels, no viewport rewrite (that's M1). ImGui is drawn as an **overlay** over the existing engine
output *on purpose*: it validates ImGui, the CMake targets, and the event chain in isolation, touching the
renderer only at M1.

## Topology (the linker enforces it; the arrow never goes up — Editor.md §1)

```
Sandbox.exe        = OpaaxEngine + SandboxModule
SandboxEditor.exe  = OpaaxEngine + OpaaxEditorLib + SandboxModule
```
- `OpaaxEngine` — zero editor knowledge, ever (D4). Always `OPAAX_WITH_EDITOR=0`.
- `SandboxModule` (static) — the game; linked by **both** exes; never includes an editor header (D9).
- `OpaaxEditorLib` (static) — the whole editor; links Engine; owns ImGui (D4/D8).
- App subclass *composes* (one exe); the module *is the game* (both exes).
- Build config (Debug/Release) ⟂ target (Editor/Runtime): the editor must build in Release; runtime in Debug.

## Execution strategy — why this order

1. **Editor-free engine FIRST (S0).** Everything downstream assumes the engine carries no editor code;
   make that true before building the editor, or the "zero ImGui in runtime" gate is unprovable.
2. **Topology before content (S1).** Stand up the empty static libs so the linker enforces the
   arrow-never-up rule from day one — a misplaced include fails to link immediately, not three steps later.
3. **Seams are silent no-ops (S3–S5).** Each seam lands with a base no-op so runtime stays byte-identical;
   the editor value is added only by *overriding* in a composition root — never by touching the engine.
4. **De-risk the DLL boundary early (S9).** `GetSubsystem<T>()` across the DLL/exe boundary (I2) is load-
   bearing for M4; prove it in M0 with a throwaway probe, fix `OPAAX_SUBSYSTEM_TYPE` now if it duplicates.
5. **Renderer untouched until M1.** Present-split (S7) is the *only* render change; ImGui rides as an overlay.

---

## Steps S0–S12 (intent + gate; live status/outcomes → `todo.md`)

| # | Intent (why it exists here) | Gate proves | Deps |
|---|---|---|---|
| **S0** | Green baseline; make engine **editor-free** (D4) — the precondition for the zero-editor-code gate. | 3 presets green; TU drops; Sandbox draws 3 quads; 0 ImGui path in engine. | — |
| **S1** | Stand up the **link topology** empty (`OpaaxEditorLib`, `SandboxModule`) so the linker enforces D1/D8/D9. | Libs build+link; `dumpbin` = 0 ImGui in `Sandbox.exe` & `OpaaxEngine.dll`. | S0 |
| **S2** | Split Sandbox into **`Module/` (both exes) + `Runtime/` (one exe)** — content vs composition (D9). | ConfigTest compiles into SandboxModule; runtime identical. | S1 |
| **S3** | Seam `OnProvideServices` — a host adds its own app services last (D1). | base no-op ⇒ runtime byte-identical. | — |
| **S4** | Seam `TickFrame` — per-frame body; editor wraps `Engine().Loop()` in UI begin/end (D1). | base = `Engine().Loop()` ⇒ identical. | — |
| **S5** | Seam `OnRegisterModules` + `ModuleRegistrar` skeleton (`Components()`/`WorldSubsystems()` routes) (D1/D9). | registries accept & store (M0 = counts); no world yet. | — |
| **S6** | Wire `SandboxModule::RegisterModule` — prove the **registration flow order** (D9/§2). | log: `RegisterModule` fires after engine boot, before first world. | S2,S5 |
| **S7** | **Present-split** (D2/F2) — un-fuse present from render; host owns WHEN, device owns HOW. | render identical; `EndFrame` submits, host calls `Present()` after `TickFrame`. | S4 |
| **S8** | Editor target **real but empty**: `IEditorService`+`EditorService` (composition root, D3), `EditorContext` (flat refs), `EditorApplication`, `SandboxEditor.exe` (D1/D8). | editor boots; init post-startup + shutdown reverse; quads render; 0 UI; edits project 'Sandbox'. | S1,S3,S4 |
| **S9** | **De-risk I2**: `GetSubsystem<WorldManager>()` from `SandboxEditor.exe` must survive the DLL boundary. | non-null, else fix `OPAAX_SUBSYSTEM_TYPE` (out-of-line StaticTypeID) **now**, not at M4. | S8 |
| **S10** | ImGui **overlay + dockspace** — first visible UI; validates ImGui+CMake+event chain in isolation. | dockspace draws over the 3 quads; `ImGui_ImplGlfw_InitForOpenGL(win,true)`. | S7,S8 |
| **S11** | Input **SEAM only** (`EditorApplication::OnEvent`→`EditorService::RouteInput`) — the *system* is M-Input (D5). | seam exists + reaches ImGui `WantCapture*`; route body is M-Input, not here. | S8,S10 |
| **S12** | Extension registrar skeleton: `IEditorModule::OnRegister(EditorExtensionRegistrar&)` + 5 registries accept & store (D10). | registrar accepts; seals before first world. | S5,S8 |

**Status (see `todo.md` for detail):** S0–S8 DONE + green (debug-editor · release · tests 106/426). Next: **S9**.

---

## M0 execution risks (Editor.md §8, filtered to what M0 actually touches)

- **DLL type identity (I2).** Header-inline function-local statics duplicate per module. `OPAAX_SUBSYSTEM_TYPE`
  must resolve out-of-line. S9 is the probe; fix at M0, not M4.
- **Event ordering.** The editor must see window events *before* they're enqueued to the engine bus
  (`OpaaxApplication::OnEvent` wiring). S11 seam.
- **Orthogonal axes.** Editor must build in Release; the `OPAAX_EDITOR_SUPPORT → non-Release` coupling is
  removed. Proven at S1/S8.
- **`RendererManager` is trivial today.** M0 keeps it that way — present-split only (S7). No render-graph
  ambition; the real output structure is M1.
- **Migration overlap.** Old world (`CoreEngineApp`, `*Old`) is dead-but-compiled — nothing in M0 may add a
  dependency on it (ARCHITECTURE.md **X1**).

---

## Appendix — M-Input (referenced by S11; separate milestone, sketch only)

S11 lands the **seam**, not the system. The system is D5:

- **Route, not an engine context system.** The engine stays dumb: `InputManager` is *fed or not fed*; all
  routing policy lives in `EditorService::RouteInput`. Per-event order: ① ImGui `WantCapture*` eats it →
  ② viewport not hovered/focused ⇒ nothing passes → ③ reserved editor keys (Esc, play/pause) → ④ remainder
  by active world mode (`Edit` ⇒ editor tools; `Play` ⇒ engine bus → `InputManager` → game).
- **`InputManager::ResetState()`** — release-all, called whenever the route closes (focus lost, pause, PIE
  stop). Designed in from day one; without it a held key becomes a stuck key.
- Runtime: the chain doesn't exist — window → bus, zero cost.
- Gameplay input contexts / action maps are a **game-layer** concept, not conflated with routing.

---

## Cross-references
- **Design (why the architecture is shaped this way):** `Docs/Architectures/Editor.md` — D1–D10, §1–§8.
- **Live tracking (mark items done):** `.claude/task/todo.md`.
- **Invariants (checked every change):** `.claude/ARCHITECTURE.md` — I1 (one static), I2 (DLL type id),
  I4 (App vs Engine), LC (lifecycle), F2 (present-split), SE (seams), MR (module registration), X1 (old world).
- **Session cockpit:** `.claude/CLAUDE.local.md`.
