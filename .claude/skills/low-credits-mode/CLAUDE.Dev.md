Pas de transform hiérarchique (parent/enfant)TransformComponent
Impossible de faire un platformer correct Renderer2D hardcodé
OpenGL — pas d'abstraction textureRenderer2D.h/cpp Vulkan impossible plus tard
Pas de physics/collisionMoteur entier Platformer non fonctionnel CoreEngineApp a un m\_World mort inutilisé CoreEngineApp.h
Confusion architectureEngineAPI.h — OPAAX\_API \_\_declspec(dllexport/import) Windows-only EngineAPI.h Build Linux/Mac cassé
Pas d'animation sprite (frame-based)Renderer/Assets
Pas de tilemapRendererPas d'audioMoteur entier
OpaaxString — pas de printf-style format OpaaxString.hpp
InputSubsystem — pas de gamepad polling GLFWInputSubsystem
AssetHandle — thread safety partielle (Load/Unload main thread only)AssetRegistry.h
Renderer2D — pas de rotation sur les quads
Renderer2D SceneSerializer — composants hardcodés (pas de registry)SceneSerializer.cpp
World.cppCamera2D hardcode viewport 1280x720 en fallback
Camera2D.h OpenGLShader utilise std::unordered\_map[std::string](std::string)OpenGLShader.h
Renderer2D::s\_Data static global — non thread-safe Renderer2D.cpp



