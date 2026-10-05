# Block DA — Data assets (and the first game that prompted it) — CLOSED 2026-10-05, user-verified

Durable rules: ARCHITECTURE.md **§DA** (DA1–DA6), **MR2i** (drawer row), **MR4a** (engine type names),
**PH19** (`MoverTickContext::Owner`), **AN8** (Create → the Right-Click Actions block). Lessons
**L110** (teach, don't build, the first game) and **L111** (vendor API only in its wrapper).

## How it started — the first game (removed)
`IsItHardToPressTheJumpButton` (one-button speedrun platformer) was generated with OpaaxCreator and
built entirely by me: jump model, mover mode, two world subsystems, test exe, hand-written assets,
editor arc preview. It worked; their verdict was the real result: *"I really do not like how to
program a game in this engine right now"* — remove the game, keep the engine fixes; *"its boring to
create an .Opaax[Type] for every new type"*; *"FULL ecs … for designing its hard. check what jason
gregory say"*; next game = a tutorial they write themselves. Archived on `feature/jump-game`.

Engine fixes kept (`fix/first-game-engine-fixes`): OpaaxCreator's editor exe did not compile (stale
include) · new projects booted into an error + empty world (`startupLevel: "Main"`) — now a path and a
starter level · `MoverTickContext::Owner`.

## Gregory (GEA §13.2, §14.2–14.3), as it applied
Two object models (tool-side vs runtime) may differ; pure-component downsides = their complaint
(large behaviour from tiny parts, no whole-object view); behaviour lives in property classes or
script; type schemas with defaults drive the editor. We kept ECS at runtime and made the existing
schema (`OPAAX_PROPERTIES`) readable at runtime.

## Steps (branch `feature/data-assets`, stacked on `fix/first-game-engine-fixes`)
| Step | Commit | What | Gate |
|---|---|---|---|
| DA1 | `c91b84f` | `IPropertyVisitor` + `VisitProperties<T>`; `InheritMeta` moved to Core | 956 |
| DA2 | `f0c6b6e` (+`a666157`) | any reflected component drawn with no editor code; Sandbox Health/Gun lines deleted | 958, eye ✓ |
| — | `c583c7d` | their ask: DummyComponent → QuadComponent ("Quad"), `AddAlias("Dummy")`, 4 maps migrated byte-exact | 959, old map loads with 1 warning |
| DA3 | `b2efa4f` | engine data assets: registry, `.opaaxdata`, `As<T>`, `TDataAssetRef`, handle + live reload | 966 |
| DA4 | `6e1f307` (+`a119939`) | one document + panel + undo + save + [D]; `Sandbox::EnemyStats`, `Grunt.opaaxdata` | eye ✓ (their log) |
| DA5 | — | "Create ▸ Data Asset" → MOVED by them to Right-Click Actions | — |
| DA6 | `67921dd` | typed picker via payload sub-type; `EnemyComponent` | 968, eye ✓ |
| — | `4b41066` | their correction: no entt outside World/ — `Core/Reflection/TypeInfo.h`, `Find<T>()`, stability test | 971 |
| — | `9dcc4ac` (+`e21bf0e`) | `WeaponStats` + `Blaster.opaaxdata` so a wrong-type drop is testable by hand | eye ✓ |

Final: **971 / 10307 / 7**. A new data type = a struct + one `DataAssets().Register<T>()` line.

## Corrections, and what they changed
- *Jumper blank in the Inspector* (found after the fact): I had told them a field was editable
  without anything observing it → DA2, and the rule "no editor-UI claim without an observer".
- *"remove dummy comp, or change it for Quad Component"* → rename + alias, their re-save discarded.
- *"DA5 is part of Right Click Action"* → Create deferred to that block.
- *"no way to test if I can drop another type"* → a second example type.
- *"never use vendor stuff in core scripts"* → `TypeInfo.h`, L111.
