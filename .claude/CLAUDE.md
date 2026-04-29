\# CLAUDE.md — OpaaxEngine



> This file is read by Claude Code at the start of EVERY session — including after auto-compaction.

> It is the supreme law of this codebase. Prompts are requests. This file is policy.

> Keep it accurate. Claude Code may append auto-learned facts under \[AUTO-MEMORY] below.



\---



\## PROJECT OVERVIEW



\*\*Engine name:\*\* OpaaxEngine  

\*\*Type:\*\* Custom 2D platformer engine  

\*\*Language:\*\* C++20  

\*\*Graphics:\*\* OpenGL (Glad loader) — Vulkan planned  

\*\*Windowing:\*\* GLFW 3.5  

\*\*Target:\*\* PC — Steam, Epic Games Store (Windows primary, Linux secondary)  

\*\*Build system:\*\* CMake  

\*\*Namespace:\*\* `Opaax`  

\*\*DLL export macro:\*\* `OPAAX\_API`



\### Vendors (all in `Engine/Vendors/`)

| Vendor | Purpose |

|--------|---------|

| GLFW   | Window, context, input events |

| Glad   | OpenGL function loader |

| glm    | Math (vectors, matrices) |

| spdlog | Logging |

| ImGui  | Debug/editor UI |

| EnTT   | ECS (entity-component-system) |



\---



\## REPOSITORY LAYOUT



```

Engine/

├── Source/

│   └── Core/

│       ├── CoreEngineApp.h/.cpp       # Main engine application class

│       ├── EngineAPI.h                # Platform macros, OPAAX\_API, asserts

│       ├── OpaaxTypes.h               # Type aliases (UniquePtr, SharedPtr, TDynArray…)

│       ├── OpaaxString.hpp            # Custom SSO string

│       ├── OpaaxStringID.hpp          # Hashed string ID (FNV-1a)

│       ├── OpaaxGlobal.h              # Global sentinel values

│       ├── Window.h/.cpp              # GLFW window wrapper

│       ├── Log/

│       │   ├── OpaaxLog.h             # Logger macros + OpaaxLog class

│       │   └── OpaaxLog.cpp

│       └── Systems/

│           ├── Subsystem.h            # ISubsystem + ISubsystemManager<T>

│           └── EngineSubsystem.h      # IEngineSubsystem, EngineSubsystemBase, EngineSubsystemMgr

├── Vendors/                           # Third-party libs — READ ONLY, never edit

├── Assets/                            # Engine-side assets (shaders, meshes…)

└── CMakeLists.txt

Docs/

└── Milestones/                        # One .md per milestone — M-XX\_Name.md

tasks/

├── todo.md                            # Active task plan for current session/milestone

└── lessons.md                         # Accumulated corrections — updated after every fix

```



\---



\## ARCHITECTURE SUMMARY



\### Boot sequence

`CoreEngineApp::Run()` → `Initialize()` → `Startup()` → game loop → `Shutdown()`



\### Game loop (fixed-timestep with interpolated render)

```

OnUpdate(deltaTime)            // variable: input, AI, scripts

while accumulator >= fixed:

&#x20;   OnFixedUpdate(fixedDelta)  // physics at 60 Hz

OnRender(alpha)                // interpolated render

Window::Update()               // glfwSwapBuffers + glfwPollEvents

```



\### Subsystem pattern

\- `ISubsystem` — pure interface (Startup / Shutdown / Update / FixedUpdate / Render)

\- `IEngineSubsystem` — adds `CoreEngineApp\*` back-pointer (window access, input, etc.)

\- `ISubsystemManager<T>` — factory-based: `RegisterSubsystem<T>(args)` → `StartupAll()`

\- `EngineSubsystemMgr` — concrete manager owned by `CoreEngineApp`



\### String system

\- `OpaaxString` — custom SSO string (no `std::string` in hot paths)

\- `OpaaxStringID` — FNV-1a hashed ID for asset keys, tags, etc.

&#x20; Macro: `OPAAX\_ID("name")` or `OPAAX\_ID(opaaxStringInstance)`



\---



\## CODE STANDARDS — NON-NEGOTIABLE



\- \*\*C++20\*\* — concepts, ranges, designated initializers available

\- \*\*No exceptions\*\* — `noexcept` on everything that must not throw

\- \*\*No RTTI\*\* — `dynamic\_cast` / `typeid` banned; use compile-time dispatch

\- \*\*No implicit heap alloc in hot paths\*\* — pre-allocate, pools, or stack

\- \*\*Data-oriented design\*\* — prefer SoA over deep inheritance trees

\- \*\*Naming:\*\*

&#x20; - Classes / structs: `PascalCase`

&#x20; - Member variables: `m\_PascalCase`

&#x20; - Static members: `s\_PascalCase`

&#x20; - Constants / macros: `UPPER\_SNAKE\_CASE`

&#x20; - Local variables: `lCamelCase` (existing convention — keep it)

&#x20; - Template params: `TPascalCase`

\- \*\*All text files:\*\* LF line endings

\- \*\*Includes:\*\* engine headers `"..."`, vendor headers `<...>`

\- \*\*`OPAAX\_API`\*\* on every class/function exported from the DLL



\---



\## COMMENT STYLE



```cpp

// NOTE:      Non-obvious design decision or important behavior.

// TODO:      Work still needed — must reference a milestone.

// FIXME:     Known bug or fragile code — must be addressed before ship.

// HACK:      Temporary workaround — revisit before shipping.

// PERF:      Performance-critical path — do not touch without profiling.

// \[REPLACED] Reason: <one-line problem> — original code commented out below.

// \[NEW]      <one-line explanation of improvement>

```



\---



\## LOGGING



Use engine macros only — never call spdlog directly in engine code.



```cpp

OPAAX\_CORE\_TRACE / INFO / WARN / ERROR / CRITICAL   // engine-internal

OPAAX\_TRACE / INFO / WARN / ERROR / CRITICAL         // game / sandbox layer

```



\---



\## PLATFORM MACROS (`EngineAPI.h`)



\- `OPAAX\_PLATFORM\_WINDOWS` / `\_LINUX` / `\_MACOS`

\- `OPAAX\_DEBUG` → enables `OPAAX\_ENABLE\_ASSERTS` and `OPAAX\_DEBUGBREAK()`

\- `OPAAX\_WITH\_EDITOR` (0/1) — editor-only code gates

\- `ENGINE\_EXPORTS` — set by CMake when building the DLL



\---



\## TYPE ALIASES (always use these — never raw std types)



```cpp

Opaax::UniquePtr<T>        // std::unique\_ptr

Opaax::SharedPtr<T>        // std::shared\_ptr

Opaax::MakeUnique<T>(...)

Opaax::MakeShared<T>(...)

Opaax::TDynArray<T>        // std::vector

Opaax::TFixedArray<T, N>   // std::array

Opaax::TFunction<Sig>      // std::function

Opaax::Uint8/16/32/64

Opaax::Int8/16/32/64

```



\---



\## WHAT CLAUDE NEVER DOES IN THIS CODEBASE



\- Does not write game logic — that belongs in the game/sandbox layer.

\- Does not use third-party engines or frameworks beyond the listed vendors.

\- Does not produce pseudocode — every snippet must compile against this codebase.

\- Does not defer known technical debt — flags `// FIXME:` immediately and moves on.

\- Does not use `dynamic\_cast`, `typeid`, or `std::exception`.

\- Does not edit anything under `Engine/Vendors/` — treat as read-only reference.

\- Does not ask for permission before fixing obvious problems.



\---



\## REFACTOR PROTOCOL



Before any architectural change (system rewrite, interface change, cross-file impact):



```

REFACTOR NOTICE

\- What:   <system or file affected>

\- Why:    <the core problem>

\- Impact: <what else will be affected>

\- Action: proceeding now.

```



When replacing code, always keep the original commented above the replacement:



```cpp

// \[REPLACED] Reason: <one-line problem>

// <original code, commented out>

//

// \[NEW] <one-line improvement>

<new code>

```



For minor fixes (naming, small logic, comment cleanup): fix silently, no notice required.



\---



\## WORKFLOW ORCHESTRATION



\### 1. Plan mode by default

\- Switch to plan mode for ANY non-trivial task (3+ steps or any architectural decision).

\- If execution deviates from the plan: \*\*STOP\*\*, replan immediately — do not push through.

\- Use plan mode for verification steps too, not just construction.

\- Write detailed specs upfront to eliminate ambiguity before touching a single file.



\### 2. Sub-agent strategy

\- Use sub-agents aggressively to keep the main context window clean.

\- Delegate research, exploration, and parallel analysis to sub-agents.

\- For complex problems, distribute compute across multiple sub-agents.

\- One focused task per sub-agent — do not overload a single sub-agent.



\### 3. Self-improvement loop

\- After EVERY correction from the developer: update `tasks/lessons.md` with the pattern.

\- Write a rule for self that prevents repeating the same mistake.

\- Re-read `tasks/lessons.md` at the start of every session on this project.

\- Iterate on lessons relentlessly until error rate drops.



\### 4. Verify before marking done

\- Never mark a task complete without proving it works.

\- Diff against baseline behavior when the change is behaviorally significant.

\- Internal check: "Would a senior engine programmer sign off on this right now?"

\- Run the build, check logs, demonstrate correctness — never assume it works.



\### 5. Require elegance (balanced)

\- For non-trivial changes: pause and ask "Is there a more elegant solution?"

\- If a fix feels hacky: ask "Knowing everything now, what would the clean solution be?"

\- For simple, obvious fixes: do not over-engineer — ship the clean minimal version.

\- Challenge your own work before presenting it to the developer.



\### 6. Autonomous bug resolution

\- When a bug is reported: fix it. Do not ask the developer to fix it themselves.

\- Read logs, compiler errors, and failed test output — then resolve.

\- Do not require context switches or handholding from the developer.

\- Fix failing CI checks without being told how.



\---



\## TASK MANAGEMENT



For every non-trivial session, maintain `tasks/todo.md`:



1\. \*\*Plan first\*\* — write the plan with verifiable checkboxes before touching code.

2\. \*\*Verify the plan\*\* — confirm scope is correct before starting implementation.

3\. \*\*Track progress\*\* — check off items as they complete; never batch-check.

4\. \*\*Explain changes\*\* — one-line high-level summary at each completed step.

5\. \*\*Document results\*\* — add a review section when the task is done.

6\. \*\*Capture lessons\*\* — update `tasks/lessons.md` after any correction from developer.



\### `tasks/todo.md` format

```markdown

\## M-XX — <Task name>



\### Plan

\- \[ ] Step 1: <specific, verifiable action>

\- \[ ] Step 2

\- \[ ] Step 3



\### Review

\- What worked:

\- What did not:

\- Technical debt introduced:

\- Lessons added to lessons.md: yes/no

```



\### `tasks/lessons.md` format

```markdown

\## Lesson — <date> — <short title>

\*\*Mistake:\*\* <what went wrong>

\*\*Root cause:\*\* <why it happened>

\*\*Rule:\*\* <the rule to never break again>

\*\*Applied in:\*\* <file or system where this was fixed>

```



\---



\## FUNDAMENTAL PRINCIPLES



\- \*\*Simplicity first\*\* — make every change as small as possible. Minimal code impact.

\- \*\*No laziness\*\* — find root causes. No temporary band-aids. Senior engineer standards only.

\- \*\*Minimal impact\*\* — changes touch only what is necessary. Do not introduce side effects.



\---



\## COMPACTION INSTRUCTIONS



> Claude Code reads this block when compacting context. Preserve accordingly.



\*\*Preserve at all costs:\*\*

\- Active milestone ID and its acceptance criteria

\- Current state of `tasks/todo.md` (which boxes are checked)

\- Any `// FIXME:` items introduced in the current session

\- Last 3 entries in `tasks/lessons.md`

\- The file currently being modified and its pending changes



\*\*Safe to drop first:\*\*

\- Vendor documentation and GLFW internals

\- Resolved task steps already checked off

\- Build/diagnostic log output older than the current task



\---



\## ACTIVE TECHNICAL DEBT



| Tag | Location | Problem | Target |

|-----|----------|---------|--------|

| FIXME | `EngineAPI.h` | `OPAAX\_API` is `\_\_declspec` only — Linux/macOS visibility unhandled | M-?? |

| FIXME | `CoreEngineApp::Run()` | `OpaaxString`/`OpaaxStringID` smoke-test code lives in the game loop | Next session |

| FIXME | `OpaaxLog::Shutdown()` | No-op — spdlog loggers not flushed or deregistered on shutdown | M-?? |

| TODO  | `CMakeLists.txt` | GLSL shader compile step fully commented out | Renderer milestone |

| TODO  | `EngineSubsystem.h` | `EngineSubsystemBase` is a placeholder — Input/Renderer/Audio not implemented | M-?? |



\---



\## CURRENT MILESTONE



> Do not edit here. Update `CLAUDE.local.md` for session-specific focus and milestone ID.

> Claude reads both files — `CLAUDE.md` for policy, `CLAUDE.local.md` for current state.



\---



<!-- AUTO-MEMORY -->

<!-- Claude Code appends auto-learned facts below this line.                          -->

<!-- Review this section periodically — incorrect entries propagate across sessions.  -->

<!-- Delete stale or wrong entries immediately when spotted.                          -->

