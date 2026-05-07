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
    // AssetType
    // =============================================================================
    /**
     * @enum AssetType
     * User-facing asset categorization. Initial set; extend by adding values.
     * Distinct from the registry's internal std::type_index and the manifest's
     * editor-side OpaaxStringID type tag.
     */
    enum class AssetType : Uint8
    {
        Unknown = 0,
        Texture2D,
        Shader,
        Scene
    };

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
