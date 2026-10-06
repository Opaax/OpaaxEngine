ENGINE\_ARCHITECTURE\_SPEC.md

1\. APPLICATION LAYER

Purpose

Process-level systems (OS, platform, window, logging)

```text id="app\_layer" Application - Platform - Window - Log - Paths - CommandLine - Engine

\## Rules

\- No gameplay logic

\- No world knowledge

\- Lifetime = process lifetime

\---

\# 2. ENGINE LAYER

\## Purpose

Core runtime services shared by all modes (Editor / PIE / Game)

```text id="engine\_layer"

Engine

\- AssetManager

\- Renderer

\- AudioManager

\- InputManager

\- EventBus

\- WorldManager

\- ConfigManager

\- Editor (optional)

\- GameInstance (optional)

Rules

Owns global systems

Does NOT own simulation state

Can create/destroy Worlds

•

•

•

13\. WORLD MANAGEMENT

Purpose

Simulation container management

```text id="world\_manager" WorldManager - EditorWorld - PIEWorld - GameWorld

\## Rules

\- Multiple Worlds may exist conceptually

\- Each World is fully isolated

\- Worlds do not share simulation state

\---

\# 4. WORLD SYSTEM

\## Purpose

Runtime simulation container

```text id="world"

World

\- entt::registry

\- GuidRegistry

\- PhysicsManager

\- TimerManager

\- LevelManager

\- PersistentLevel

\- ActiveLevel

Rules

World owns simulation state

World is independent of GameInstance

World is the ECS boundary (entt lives here)

5\. ENTITY SYSTEM (ENTT BASED)

Storage

entt::registry is the source of truth

Engine Wrapper

```cpp id="entity\_api" class Entity { entt::entity handle; World\* world;

•

•

•

•

2public: Guid GetGuid();

template<typename T> T\& Get();

template<typename T> bool Has(); };

\## Entity Metadata Component

```cpp id="entity\_meta"

struct EntityMeta

{

Guid persistentGuid;

MapId ownerMap;

};

Rules

No raw entt usage outside World layer

GUID is persistent identity

entt entity is runtime-only handle

6\. GUID SYSTEM

Purpose

Cross-stream safe referencing

```text id="guid" GuidRegistry Guid → (World\*, entt::entity)

\## Rules

\- Used for all persistent references

\- Missing entities return null-safe result

\- Never store raw pointers across maps

\---

\# 7. LEVEL SYSTEM

\## Purpose

Streaming + world composition

```text id="level"

LevelManager

\- LoadLevel()

\- UnloadLevel()

\- LoadMap()

•

•

•

3- UnloadMap()

\- StreamingSystem

Events

```text id="level\_events" LevelLoaded LevelUnloaded MapLoaded MapUnloaded

\## Rules

\- Lives inside World

\- Controls ActiveLevel composition

\- Must be deterministic and stream-safe

\---

\# 8. LEVEL HIERARCHY

```text id="level\_hierarchy"

World

\- PersistentLevel

\- ActiveLevel

ActiveLevel

\- PersistentMap

\- Map\_A

\- Map\_B

\- Map\_C

Rules

PersistentLevel is always loaded

ActiveLevel is streamed

Maps are independent units

9\. MAP SYSTEM

Purpose

Pure data container

```text id="map" Map - EntityDefinitions - TerrainData - LightData - Metadata - EditorData

\## Rules

\- No runtime ownership

\- No systems

\- Can be loaded/unloaded independently

•

•

•

4---

\# 10. GAME INSTANCE LAYER

\## Purpose

Persistent gameplay state

```text id="game\_instance"

GameInstance

\- SaveSystem

\- QuestSystem

\- InventorySystem

\- DialogueSystem

\- AchievementSystem

Rules

Exists outside World lifecycle

Stores progression state only

Does NOT own simulation

11\. INPUT SYSTEM (UPDATED MODEL)

Purpose

Layered input processing (NOT exclusive modes)

```text id="input" InputManager - RawDeviceState - InputLayers\[]

\## Input Layer

```cpp id="input\_layer"

struct InputLayer

{

int Priority;

bool CanConsume;

bool BlocksLowerLayers;

HandleInput();

};

Default Layers

```text id="layers" UIInput GameInput EditorInput DebugInput

•

•

•

5## Rules

\- Input is processed in layered order

\- Multiple layers can be active simultaneously

\- Layers may consume or pass-through input

\- No PIE vs Editor exclusive switching

\---

\## Input Flow

```text id="input\_flow"

Hardware Input

→ InputLayer\[UI]

→ InputLayer\[Game]

→ InputLayer\[Editor]

→ InputLayer\[Debug]

12\. EDITOR LAYER

Purpose

Authoring tools runtime

```text id="editor" Editor - EditorWorld - SelectionSystem - UndoRedoSystem - GizmoSystem - Inspector -

AssetBrowser - PIEManager - EditorUI

\## Rules

\- EditorWorld is separate from PIEWorld

\- Editor never modifies PIEWorld directly

\---

\# 13. PIE SYSTEM

\## Purpose

Isolated runtime simulation inside editor

\## Lifecycle

```text id="pie"

EditorWorld

→ Create PIEWorld

→ Create GameInstance (optional)

→ Load Levels

6→ Run Simulation

→ Destroy PIEWorld

Rules

PIEWorld is fully isolated

EditorWorld remains unchanged

Uses same World system as game

14\. SYSTEM OWNERSHIP RULES

```text id="ownership" Application owns Engine

Engine owns WorldManager

WorldManager owns Worlds

World owns: - entt registry - LevelManager - Physics - Timer system

LevelManager owns: - Levels

Levels own: - Maps

GameInstance owns: - persistence systems only

\---

\# 15. CORE DESIGN PRINCIPLES

```text id="principles"

\- Worlds are isolated simulation units

\- entt is ECS backend, wrapped by engine API

\- GameInstance is persistence only

\- Maps are pure data containers

\- Entities never cross-map via pointers

\- GUID is the only persistent reference system

\- Input is layered, not mode-based

\- Editor and PIE are fully isolated worlds

16\. KEY ARCHITECTURAL SUMMARY

```text id="summary" Application → Engine → WorldManager → World → LevelManager → Maps →

Entities

•

•

•

7GameInstance → persistent gameplay state

InputManager → layered input routing

Editor → tooling layer over Engine

PIE → isolated World instance running inside Editor ```

8

