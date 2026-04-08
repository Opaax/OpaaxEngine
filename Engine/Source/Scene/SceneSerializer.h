#pragma once

#include "Scene.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    /**
     * @class SceneSerializer
     *
     * Serializes a Scene (World + metadata) to JSON on disk.
     * Deserializes JSON back into a Scene.
     *
     * Assets are stored as path strings — not as handles.
     * Deserialization calls AssetRegistry::Load() for each texture path.
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
         * Serialize InScene to a JSON file at InPath.
         * @param InScene 
         * @param InPath 
         * @return true on success.
         */
        static bool Serialize(const Scene& InScene, const char* InPath);
        
        /**
         * Deserialize a JSON file at InPath into InScene.
         * @param InScene 
         * @param InPath 
         * @return true on success.
         */
        static bool Deserialize(Scene& InScene, const char* InPath);
    };

} // namespace Opaax