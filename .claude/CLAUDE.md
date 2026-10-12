# OpaaxEngine

The rules, commands and conventions for working on this repository are in AGENTS.md:

@../AGENTS.md

# Guide Lines

## AI

- Skills for the common tasks are in `.claude/skills/`: building and testing, driving a game or the
  editor through automation, adding a gameplay type, changing the renderer.

## Code

- Use core type instead of std
- Porpose a wrapper or new engine core type if new std code appear.
- Follow engine nommenclature

## Best Pratice

- When adding a new feature, continue to implements the docs in `Docs/*`
- When you are ask to create a new feature, ask yourself how Jason Gregory said on his book? How other engine resolve this problem ? How to implement it to match the engine architecture.
- Never code on main branch
- Never create temp nor anykind of tempory files outside this directory `OpaaxEngine/*`; Create temp stuff here instead.
- Learn by yourself and after user feedback by using @.claude/lessons.md
- Always propose a solution to make data accessible for human.