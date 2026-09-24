---
name: low-credits-mode
description: Choose and execute a task that fits the account's remaining credit budget.
disable-model-invocation: true
---

# Low Credits Mode

## Step 0 — Activation validation (IMPORTANT)

Before doing anything, confirm at least one:
- The user explicitly mentions low credits / budget constraints.
- The invocation argument states a budget level (e.g. `/low-credits-mode VERY_LOW`).

If neither → EXIT the skill immediately.

## Step 1 — Budget level

Claude cannot see the account's real credit balance — the level comes from the user:
- `VERY_LOW`: critical constraint.
- `LOW`: constrained.
- Unstated: assume `LOW` (fail-safe).

## Step 2 — Task cost evaluation

- `CHEAP`: trivial modification or lookup.
- `MODERATE`: small system change.
- `EXPENSIVE`: architecture, multi-system, exploration.

## Step 3 — Hard constraints

- **VERY_LOW:** only CHEAP tasks. MODERATE/EXPENSIVE → return reduced scope OR refuse with minimal guidance.
- **LOW:** CHEAP and MODERATE allowed. EXPENSIVE → reduce scope automatically.

## Step 4 — Minimal context loading

- Read `CLAUDE.Dev.md` (this folder; may be empty — skip if so).
- Extract ONLY directly relevant sections. Do NOT summarize the entire file. Do NOT explore unrelated systems.

## Step 5 — Execution strategy

- No exploration, no multiple solutions, no long explanations.
- Prefer: direct code · direct answer · existing engine patterns.

## Step 6 — Output rules

Output MUST be short, actionable, single-solution. Append:

```
Mode: LOW_CREDIT
Scope: REDUCED | FULL
```
