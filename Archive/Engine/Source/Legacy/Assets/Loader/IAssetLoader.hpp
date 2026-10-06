#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    /**
     * @struct IAssetLoader
     * Loader Interface.
     * Each asset type (Texture, Audio, Animation...) has its own loader.
     *
     * Load() return a raw ptr — AssetRegistry take ownership
     *
     * @tparam T Asset type
     */
    template<typename T>
    struct IAssetLoader
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IAssetLoader() = default;

        // =============================================================================
        // Functions
        // =============================================================================

        /**
         * Construct an asset from disk.
         * @param InAbsPath     Resolved absolute path (the loader actually reads from this).
         * @param InCanonicalID Registry-stable ID stamped onto the asset so
         *                      asset->GetAssetID() == handle.GetID() == registry cache key.
         * @return nullptr on failure — never throws
         */
        virtual T* Load(const char* InAbsPath, OpaaxStringID InCanonicalID) = 0;

        /**
         * @param InAsset true if the loaded asset is valid and ready to use.
         * @return true if the loaded asset is valid and ready to use.
         */
        virtual bool IsValid(T* InAsset) = 0;
    };

} // namespace Opaax