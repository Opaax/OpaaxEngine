#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"

#include <nlohmann/json.hpp>
#include <fstream>

namespace Opaax
{
    /**
     * @struct AssetDescriptor
     *
     * One entry in the manifest.
     *
     * ID           = logical name used in code via OPAAX_ID("Player")
     * RelPath      = path relative to base path — resolved at load time via OpaaxPath::Resolve()
     * type         = Type tag — informational for editor, not used by runtime loader
     */
    struct AssetDescriptor
    {
        OpaaxStringID  ID;          // logical name  — OPAAX_ID("Player")
        OpaaxString    RelPath;     // relative path — "GameAssets/Textures/Player.png"
        OpaaxStringID  Type;        // "Texture2D", "AudioClip", etc. — editor only
    };
    
    /**
     * @class AssetManifest
     *
     * Loads and owns all AssetDescriptors from one or more JSON manifest files.
     * Multiple manifests can be loaded (engine manifest + game manifest).
     *
     * Usage:
     *  AssetManifest::LoadFile("EngineAssets/AssetManifest.json");
     *  AssetManifest::LoadFile("GameAssets/AssetManifest.json");
     *  const AssetDescriptor* lDesc = AssetManifest::Find(OPAAX_ID("Player"));
     */
    class OPAAX_API AssetManifest
    {
        // =============================================================================
        // Statics
        // =============================================================================
    private:
        static bool GenerateEmpty(const char* InAbsPath) noexcept;
        
    public:

        /**
         * Load a manifest file and merge into the registry.
         * @param InAbsPath 
         * @return the number of entries loaded, -1 on failure.
         */
        static Int32 LoadFile(const char* InAbsPath);

        /***/
        static Int32 LoadFile(const OpaaxString& InAbsPath);

        /**
         * Find a descriptor by logical ID.
         * @param InID 
         * @return nullptr if not found — caller falls back to direct path resolution.
         */
        static const AssetDescriptor* Find(OpaaxStringID InID) noexcept;
        
        /**
         * Reverse lookup — find a descriptor by its relative path.
         * @param InRelPath 
         * @return 
         */
        static const AssetDescriptor* FindByPath(const OpaaxString& InRelPath) noexcept;
        
        /**
         * 
         * @param InID 
         * @return true if a logical ID exists in the manifest.
         */
        static bool Contains(OpaaxStringID InID) noexcept;
        
        /**
         * Clear all loaded descriptors — called by AssetRegistry::Shutdown().
         */
        static void Clear() noexcept;
        
        /**
         * 
         * @return Read-only access for editor (AssetBrowserPanel)
         */
        static const UnorderedMap<Uint32, AssetDescriptor>& GetAll() noexcept
        {
            return s_Descriptors;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static UnorderedMap<Uint32, AssetDescriptor> s_Descriptors;
    };

} // namespace Opaax