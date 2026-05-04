# Lessons

> Per `.claude/CLAUDE.md` §3: write rules for myself that prevent repeating mistakes.
> Re-read at the start of each OpaaxEngine session.

---

## L-001 — Audit existing persistence before adding new persistence (2026-05-02, M2 Step 3)

**Mistake:** Planned and shipped Save/Open dialogs in M2 Step 3 without auditing the pre-existing implicit save paths. Result: `CoreEngineApp::OnShutdown` + `GameScene::OnUnload` both auto-saved to a hardcoded `GameAssets/Scenes/GameScene.json`, silently overwriting the user's edited scene at every quit. User had to flag the bug.

**Rule:** When introducing a new persistence/IO mechanism (save, snapshot, cache, manifest write), grep the codebase first for every existing call site that already writes to disk in the same domain. Specifically grep for: `Save`, `Serialize`, `Write`, `OnUnload`, `OnShutdown`, virtual life-cycle hooks, and any "auto" prefix. Reconcile before adding the new path.

**How to apply:** During plan-mode Phase 1 (exploration), spawn an Explore agent dedicated to the question "what currently writes to disk in this domain, and when?". Don't merge with the broader feature exploration — give it its own focus so audits don't get truncated.

---

## L-002 — Verification must cover cross-feature regressions, not just the new happy path (2026-05-02, M2 Step 3)

**Mistake:** M2 Step 3 verification checklist tested Save → Open round-trip, Ctrl+S, New Scene confirm, and PIE regression. Missed: (a) shutdown auto-save collision (L-001), (b) Asset Browser refresh after a new file lands, (c) Asset Browser surfacing the loaded scene, (d) `AssetScanner` poisoning the manifest with `.scene.json` entries on next boot. Three follow-up bugs in one session.

**Rule:** When the new feature creates files of a type the engine already scans/loads/displays, verification must include: (1) the file appears in any browser/picker without restart, (2) reloading the engine doesn't corrupt or duplicate state from those files, (3) any "currently active X" indicator stays in sync, (4) any subsystem that auto-scans the dir (AssetScanner, registry) handles the new extension cleanly.

**How to apply:** Extend the verification section in plans for any feature touching the asset/scene/file domain with a "Cross-feature scan" subsection: list the subsystems that currently observe that file type and confirm each behaves correctly post-change.
