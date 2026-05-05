# Backlog — Out of Scope for the Refactor Program

> Items flagged in `.claude/skills/engine-planning/engine-planning.md` (and elsewhere) that are NOT part of M0–M7. Revisit after the refactor program closes, or open standalone milestones for the urgent ones.

---

## New Subsystems

- **Physics system.** Probably 2D box / circle + raycasts at first. Needed for the platformer target. Depends on a stable World / Scene (M2) — could realistically start after M2.
- **Audio system.** User explicitly waiting for "stronger asset system" (M1). Unblocked after M1; can run in parallel with M3+.
- **Job system / threading.** Frame-graph / parallel render extraction / parallel physics. Requires the renderer pass system (M4) to be design-aware of jobs from day one. Defer until after M4 to avoid retrofitting.

---

## Gameplay-side Components

- **`SpriteSheetComponent`** — atlas index + region size. Forces atlas-based animation pipeline.
- **`SpriteAnimationComponent`** — child of sheet, plays frames over time.

---

## Editor UX

- **Real toolbar** with categories (file / save / load / open, view, build, …).
- **Asset icons** — replace placeholder chars with actual icon resources.
- **Editor Play / Stop ↔ Viewport linkage** — current Play button feels detached.

---

## Engine Plumbing

- **`OpaaxString` printf-style format.** Currently no `Format(...)` helper.
- **Engine math sizing review.** "Engine math before the engine is too big?" — open question on whether to keep current math layer or trim / extend.
- **Vulkan backend.** `Renderer2D` is currently hardcoded OpenGL. M4 (Renderer Pass System) is the *enabler* — Vulkan implementation itself is post-refactor work.

---

## Already Resolved (do not re-track)

- Transform scale ↔ Sprite size coupling (commit `6d8ca40`).
- Auto-appear new scene on creation (commit `ab82842`).
- Load / Save with native dialogs (commit `5c201c6`).
- `AssetScanner` extension-based scanning (commit `81465e6`).
- `projectRoot` path info added (commit `e01562b`).
