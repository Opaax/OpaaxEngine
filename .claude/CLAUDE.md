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

- After EVERY user correction: update `tasks/lessons.md` using the template
- Write rules for yourself that prevent repeating the same mistake
- Iterate relentlessly on those lessons until the error rate drops
- Re-read lessons at the start of each session for relevant projects

## 4. Verify Before Considering Done

- Never mark a task complete without proving it works
- Identify the difference between expected behavior and your changes when relevant
- Ask yourself: “Would a senior engineer approve this?”
- Run tests, check logs, demonstrate correctness

## 5. Demand Elegance (Balanced)

- For non-trivial changes: pause and ask, “Is there a more elegant solution?”
- If a fix feels hacky: “Knowing everything I know now, what would the elegant solution be?”
- Spend time on simple and obvious fixes — do not over-engineer
- Challenge your own work before presenting it

## 6. Autonomous Bug Fixing

- When a bug is reported: fix it. Do not ask the user to do it for you
- Investigate logs, errors, and failing tests — then resolve them
- Do not require the user to change context for you
- Fix failing CI tests without being told how

# Task Management

1. **Plan first**: Write the plan in `tasks/todo.md` with verifiable items
2. **Validate the plan**: Review it before starting implementation
3. **Track progress**: Mark items complete as you go
4. **Explain changes**: Provide a high-level summary at each step
5. **Document results**: Add a review section in `tasks/todo.md`
6. **Capture lessons**: Update `tasks/lessons.md` after corrections
7. **Code Base**: Update `code/code-base.md` after milestone or if empty from the current project

# Core Principles

- **Main Principles**: Look for `code/code.md` ask and update if you find other things
- **Code Base**: Look for `code/code-base.md`
- **Simplicity first**: Make every change as simple as possible. Minimal impact on the code.
- **No laziness**: Find root causes. No temporary patches. Senior engineering standards.
- **Minimal impact**: Changes should touch only what is necessary. Avoid introducing bugs.

#Milestone

- **Create Milestone**: Look for `milestone/Milestone_Template.md`
- **Update Local**: Update the local `CLAUDE.local.md` current milestone
- **Finishing Milestone**: A milestone is done only when the system(s) work and the user decide to move to another milestone.
- **Weird Behavior**: If the user make a weird behavior like asking something that shouldn't appear in this milestone ask for details.
