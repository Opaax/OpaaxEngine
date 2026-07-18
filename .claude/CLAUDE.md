# Workflow Orchestration

## 0. Read the Contract First

- At the start of any engine work, read `.claude/ARCHITECTURE.md` (the invariants contract — I1/I5,
  LC lifecycle, BO boot order, F frame, SE seams) and `.claude/lessons.md` (L1–L8 post-mortems). The
  contract is citable in review ("violates I1"); breaking one of its invariants is a STOP-and-re-plan
  trigger (§1), and when it disagrees with the code, the code wins and you fix the contract in the same change.

## 1. Set Plan Mode as Default

- Enter plan mode for EVERY non-trivial task (3+ steps or architectural decisions)
- If something deviates from the plan, STOP and re-plan immediately — do not keep pushing forward
- Use plan mode for verification steps, not only for implementation
- Write detailed specifications upfront to reduce ambiguity

## 2. Sub-Agent Strategy

- Sub-agents start cold and re-derive context the main session already has — they are the expensive path, not the default
- Use them for large read-only research sweeps (audits, codebase-wide searches) where the findings are small relative to the exploration
- Implementation, debugging, and anything that needs this session's context stays in the main session
- One task per sub-agent for focused execution

## 3. Self-Improvement Loop

- After EVERY user correction: update `.claude/lessons.md` using the template
- Write rules for yourself that prevent repeating the same mistake
- Iterate relentlessly on those lessons until the error rate drops
- Re-read lessons at the start of each session for relevant projects

## 4. Verify Before Considering Done

- Never mark a task complete without proving it works
- Identify the difference between expected behavior and your changes when relevant
- Ask yourself: "Would a senior engineer approve this?"
- Run tests, check logs, demonstrate correctness

## 5. Demand Elegance (Hard)

- For non-trivial changes: pause and ask, "Is there a more elegant solution?"
- If a fix feels hacky: "Knowing everything I know now, what would the elegant solution be?"
- Spend time on simple and obvious fixes — do not over-engineer
- Challenge your own work before presenting it
- Use best pattern for the current job
- Code has to be simple but respect programming principal (OOP, SOLID, KISS)

## 6. Autonomous Bug Fixing

- When a bug is reported: fix it. Do not ask the user to do it for you
- Investigate logs, errors, and failing tests — then resolve them
- Do not require the user to change context for you
- Fix failing CI tests without being told how

# Task Management

1. **Plan first**: Write the plan in `.claude/task/todo.md` with verifiable items
2. **Validate the plan**: Review it before starting implementation
3. **Track progress**: Mark items complete as you go
4. **Explain changes**: Provide a high-level summary at each step
5. **Document results**: Add a review section in `.claude/task/todo.md` Create it if not here
6. **Capture lessons**: Update `.claude/task/lessons.md` after corrections Create it if not here
7. **Synthetize Lessons**: when the task is done; take the most important lessons, put it in `.claude/lessons.md`
8. **Finish the task**: when the task is finished, called by the user, clean `.claude/task/todo.md` and `.claude/task/lessons.md`

# Project Context

## What This Project Is

This is a **custom 2D game engine** built from scratch by a solo developer with no prior game engine experience. The goal is a engine that is:
- **Simple**: understandable end-to-end by one person
- **Extensible**: clean architecture that can grow without rewriting
- **Usable**: good enough to ship real games with

Inspirations are Unreal Engine, Unity, and Godot — not in complexity, but in robustness, structure, and best practices. When in doubt, favor the simpler path that a mid-sized indie engine would take.

## First Target Games

The engine must be validated against at least one of these genres:
- **Shmup** (shoot-'em-up): fast entities, bullets, patterns, scrolling backgrounds
- **Platformer**: physics, tilemaps, player controller, camera

## Knowledge Bank — `.claude/data/`

When making architecture or implementation decisions, **check this folder first** for reference material before relying on general knowledge. It contains PDFs such as:
- *Game Engine Architecture* by Jason Gregory — authoritative reference for engine structure, loops, asset pipelines, etc.
- Other public domain or open-source engine references added over time

Always prefer guidance from these documents over generic advice when they are relevant.

## Skills — `.claude/skills/`

Reusable task-specific instructions are stored here. **Do not load skills proactively.** Only load and apply a skill when the user explicitly requests it by name or asks you to use the skill system.