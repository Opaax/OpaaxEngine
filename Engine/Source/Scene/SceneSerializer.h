#pragma once

#include "Scene.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    class World;

    /**
     * @class SceneSerializer
     *
     * Serializes a Scene's entity subset of a World to JSON on disk.
     * Deserializes JSON back into a World, tagging entities with the destination Scene.
     *
     * Asset references are stored as the canonical asset ID — the manifest's
     * logical name when one exists for the file ("Textures/Player"), otherwise
     * the project-relative path. Never an absolute path. Deserialization calls
     * AssetRegistry::Load() for each ID, which routes through Normalize().
     *
     * Only components registered in the component registry are serialized/deserialized. Unknown components are silently skipped.
     */
    class OPAAX_API SceneSerializer
    {
        // =============================================================================
        // Static
        // =============================================================================
    public:

        /**
         * Serialize the active scene's entity subset of InWorld to JSON at InPath.
         * @param InScene scene whose name + SceneID context drives the dump.
         * @param InPath  on-disk JSON destination.
         * @param InWorld engine-shared World to read entities from.
         * @return true on success.
         */
        static bool Serialize(const Scene& InScene, const char* InPath, const World& InWorld);

        /**
         * Deserialize JSON at InPath into InWorld, contributing entities tagged
         * with InScene's SceneID.
         * @param InScene destination scene context (provides SceneID at Step 3+).
         * @param InPath  on-disk JSON source.
         * @param InWorld engine-shared World to create entities in.
         * @return true on success.
         */
        static bool Deserialize(Scene& InScene, const char* InPath, World& InWorld);
    };

} // namespace Opaax