The Transform scale and Sprite has UX weakness:

* We need to update the transform scale to update the sprite size
* Its even weirder while the entity is a child since the parent might have scale for its own sprite is hard to predict the sprite size



AssetRegistery And AssetManifest:

* Sometime we use AssetRegistery sometime AssetManifest

  * This make some behavior unpredictable; for example Serialize a sprite texture was serialize with an ID from the manifest, but load with the registery that the load failed



Sprite Component:

* This may need a child class comp like: SpriteSheetComponent

  * Setup the size of the region and give a x index and y index
  * Currently possible with UV min/max but pain in the ass
* So with with this: SpriteSheetComponent we can force dev to make animation as atlas and so create child SpriteSheetComponent for an SpriteAnimationComponent (or similar)



Toolbar:

* We may need a real toolbar (Like normal app have for save/load/open etc...)

  * categories



Physics system

* We need to create it



Opaax Math

* Engine math before the engine is to big?



Parralization

* Job system
* Threading (physics, gameplay, other..?)



Audio System

* But I feel like we need an even stronger asset system



Renderer2D hardcodé OpenGL

* Cannot make vulkan yet



CoreEngineApp m\_World is dead not used?



OpaaxString

* no printf style for format ?



Camera2D 

* hardcode viewport 1280x720

  * So we need Engine/Game Configs as json or add more tool like .ini?



OpenGLShader:

* use std::unordered\_map<std::string> instead of Opaax types



Renderer2D

* s\_Data static global no thread safe



Editor Proper asset icons

* Currently using char 
* We may add resources for proper icons



Editor Play

* The button play/stop should be link with viewport? (I think ux will increase if so)

