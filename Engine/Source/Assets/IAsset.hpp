#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // EAssetState
    // =============================================================================
    /**
     * @enum EAssetState
     * Lifecycle state of a managed asset.
     * - Unloaded: registered, no payload in memory
     * - Loading:  load operation in flight (sync today, async later)
     * - Loaded:   payload resident, usable
     * - Failed:   load attempted and failed; payload not usable
     */
    enum class EAssetState : Uint8
    {
        Unloaded = 0,
        Loading,
        Loaded,
        Failed
    };

    // =============================================================================
    // AssetType — X-Macro driven (Assets/AssetTypeList.h)
    // =============================================================================
    /**
     * @enum AssetType
     * User-facing asset categorization. The enum body and matching
     * g_AssetTypeIDs[] are both generated from AssetTypeList.h —
     * adding a new type means a single new line in that list file.
     */
    enum class AssetType : Uint8
    {
        Unknown = 0,
        #define OPAAX_ASSET_TYPE(Name) Name,
        #include "AssetTypeList.h"
        #undef OPAAX_ASSET_TYPE
        Count
    };

    /*** Parallel canonical-name array. Index by static_cast<Uint8>(AssetType). */
    inline const OpaaxStringID g_AssetTypeIDs[] =
    {
        OPAAX_ID("Unknown"),
        #define OPAAX_ASSET_TYPE(Name) OPAAX_ID(#Name),
        #include "AssetTypeList.h"
        #undef OPAAX_ASSET_TYPE
    };

    /*** AssetType -> canonical OpaaxStringID. O(1) LUT. */
    inline const OpaaxStringID& ToStringID(AssetType InType) noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InType);
        return (lIdx < static_cast<Uint8>(AssetType::Count)) ? g_AssetTypeIDs[lIdx] : g_AssetTypeIDs[0];
    }

    /*** OpaaxStringID -> AssetType. Linear scan; pure integer compare per slot. */
    inline AssetType AssetTypeFromStringID(const OpaaxStringID& InID) noexcept
    {
        for (Uint8 i = 0; i < static_cast<Uint8>(AssetType::Count); ++i)
        {
            if (g_AssetTypeIDs[i] == InID)
            {
                return static_cast<AssetType>(i);
            }
        }
        return AssetType::Unknown;
    }

    // =============================================================================
    // IAsset
    // =============================================================================
    /**
     * @class IAsset
     * Pure-virtual interface every managed asset will eventually implement
     */
    class OPAAX_API IAsset
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        virtual ~IAsset() = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /*** Logical, registry-stable identifier (interned StringID). */
        virtual OpaaxStringID         GetAssetID()    const = 0;

        /*** User-facing asset category. */
        virtual AssetType             GetType()       const = 0;

        /*** Current lifecycle state. */
        virtual EAssetState           GetState()      const = 0;

        /*** Project-root-relative source path (e.g. "Engine/Assets/Textures/Player.png"). */
        virtual const OpaaxString&    GetSourcePath() const = 0;
    };
}
