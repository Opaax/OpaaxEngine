# Workflow Orchestration

## 1. Set Plan Mode as Default

- Enter plan mode for EVERY non-trivial task (3+ steps or architectural decisions)
- If something deviates from the plan, STOP and re-plan immediately — do not keep pushing forward
- Use plan mode for verification steps, not only for implementation
- Write detailed specifications upfront to reduce ambiguity

## 2. Sub-Agent Strategy

- Use sub-agents heavily to keep the main context clean
- Delegate research, exploration, and parallel analysis to sub-agents
- For complex problems, distribute more compute across sub-agents
- One task per sub-agent for focused execution

## 3. Self-Improvement Loop

- After EVERY user correction: update `.claude/task/lessons.md` using the template
- Write rules for yourself that prevent repeating the same mistake
- Iterate relentlessly on those lessons until the error rate drops
- Re-read lessons at the start of each session for relevant projects

## 4. Verify Before Considering Done

- Never mark a task complete without proving it works
- Identify the difference between expected behavior and your changes when relevant
- Ask yourself: "Would a senior engineer approve this?"
- Run tests, check logs, demonstrate correctness
- Never claim "build green" without running `./build.bat <preset> </dev/null` and showing the tail of the output. Reading code is not building it. (Lesson 10.)

## 5. Demand Elegance (Balanced)

- For non-trivial changes: pause and ask, "Is there a more elegant solution?"
- If a fix feels hacky: "Knowing everything I know now, what would the elegant solution be?"
- Spend time on simple and obvious fixes — do not over-engineer
- Challenge your own work before presenting it

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
5. **Document results**: Add a review section in `tasks/todo.md`
6. **Capture lessons**: Update `.claude/task/lessons.md` after corrections

# Milestone

- **Create Milestone**: Use `.claude/milestone/Milestone_Template.md` as the template for every new milestone. Fill it out completely before starting work.
- **Update Local**: Keep `.claude/CLAUDE.local.md` updated with the current active milestone name and its objective. This is the single source of truth for "what are we building right now."
- **Finishing Milestone**: A milestone is complete only when all its systems work end-to-end AND the user explicitly decides to move on. Do not self-close a milestone.
- **Scope Guard**: If the user asks for something that falls outside the current milestone's scope, pause and ask: "This looks outside the current milestone — should we add it to the backlog, open a new milestone, or proceed anyway?"

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

Architecture decisions should keep these use cases in mind. Do not build systems that make these harder to implement.

## Knowledge Base — `.claude/data/`

When making architecture or implementation decisions, **check this folder first** for reference material before relying on general knowledge. It contains PDFs such as:
- *Game Engine Architecture* by Jason Gregory — authoritative reference for engine structure, loops, asset pipelines, etc.
- Other public domain or open-source engine references added over time
- when creating files, class, struct or what ever item, add it into '.claude/data/engine-structure.md'

Always prefer guidance from these documents over generic advice when they are relevant.

## Code Style — `.claude/CLAUDE.local.md`

Personal coding conventions and preferences live here. Read this file at the start of any coding session. If it is missing or empty, ask the user before assuming a style.

## Skills — `.claude/skill/`

Reusable task-specific instructions are stored here. **Do not load skills proactively.** Only load and apply a skill when the user explicitly requests it by name or asks you to use the skill system.