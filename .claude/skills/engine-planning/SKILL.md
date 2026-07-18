---
name: engine-planning
description: Audit the engine for weaknesses, categorize them, and prepare a plan of milestones.
disable-model-invocation: true
---

# Engine Planning

## Step 0 — Activation validation (IMPORTANT)

- Check whether a planning pass is already in progress (unfinished output in `.claude/task/todo.md`
  or open questions in `engine-planning.md`). If so, resume it — do not start over.

## Step 1 — Budget check

- Claude cannot see the account's real credit balance. The budget comes from the user:
  if the invocation argument or message says `VERY_LOW` / `LOW`, honor it; if unstated, assume `LOW` (fail-safe).
- This skill requires heavy reflection (broad engine review). Under `VERY_LOW`, abort and say why;
  under `LOW`, propose a reduced-scope pass (one engine area) before proceeding.

## Step 2 — Context loading (minimal)

- Read `engine-planning.md` (this folder) — the raw dev-notes list of engine issues.
  Parse it per `reference.md` (unstructured, mixed FR/EN, one issue per line).
- Cross-check against `.claude/milestone/Engine_Task.md` + `Backlog.md` + `proposals/` so already-known
  or already-fixed weaknesses are not rediscovered.
- Extract ONLY directly relevant sections. Do NOT summarize entire files.

## Step 3 — The work

- Sweep for weaknesses; categorize findings (correctness / capacity / missing system / editor-DX).
- Shape findings into proposed milestones using `.claude/milestone/Milestone_Template.md`,
  each with a Demo gate + Unit-test gate.

## Step 4 — Output

- Clear, prioritized plan of milestones.
- If the user validates the plan → write it into `.claude/task/todo.md`.
