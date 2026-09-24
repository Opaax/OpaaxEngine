# Dev notes — raw engine issues (input for the engine-planning skill)

The Transform scale and Sprite has UX weakness:

* We need to update the transform scale to update the sprite size
* Its even weirder while the entity is a child since the parent might have scale for its own sprite is hard to predict the sprite size

Sprite Component:

* This may need a child class comp like: SpriteSheetComponent
  * Setup the size of the region and give a x index and y index
  * Currently possible with UV min/max but pain in the ass
* So with with this: SpriteSheetComponent we can force dev to make animation as atlas and so create child SpriteSheetComponent for an SpriteAnimationComponent (or similar)

Toolbar:

* We may need a real toolbar (Like normal app have for save/load/open etc...)
  * categories

Opaax Math

* Engine math before the engine is to big?

Audio System

* But I feel like we need an even stronger asset system

OpaaxString

* no printf style for format ?

Renderer2D

* s_Data static global no thread safe

Editor Proper asset icons

* Currently using char
* We may add resources for proper icons

Editor Play

* The button play/stop should be link with viewport? (I think ux will increase if so)

---

## Shipped (pruned 2026-06-12 — kept for history, do NOT re-plan these)

* AssetRegistery And AssetManifest unpredictable behavior → resolved ~M1 (registry is the single load path)
* Physics system "we need to create it" → M9 (active; seam + bodies + events + queries + mover done)
* Parralization (job system, threading) → M6 JobSubsystem
* Renderer2D hardcodé OpenGL / cannot make vulkan → M7 RHI seams + M8 Vulkan backend
* Camera2D hardcoded 1280x720 + need engine/game configs as json → M4 Camera System + engine.config.json
* CoreEngineApp m_World dead not used? → owned + used since M2.5 (World ownership inversion)
* OpenGLShader std::unordered_map<std::string> → now UnorderedMap<OpaaxString, ...> (OpaaxTypes aliases)
