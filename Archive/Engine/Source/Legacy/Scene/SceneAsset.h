#pragma once

#include "Assets/IAsset.hpp"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // SceneAsset
    // =============================================================================
    /**
     * @class SceneAsset
     *
     * Logical scene asset — descriptor only. Holds the source path + asset ID
     * for a *.scene.json file so AssetRegistry / AssetManifest can speak about
     * scenes the same way they speak about textures and shaders.
     *
     * Does NOT own a live Scene instance: instantiating + driving a Scene's
     * runtime lifecycle is SceneManager's job. SceneAsset is the on-disk
     * descriptor; a Scene is the runtime world built from it.
     *
     * State is Loaded once the descriptor exists — the metadata IS the payload
     * at this stage. Step 4/5/6 will widen this as serializer + editor wiring
     * land (deserialized template, asset-browser hooks, double-click open).
     */
    class OPAAX_API SceneAsset final : public IAsset
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        SceneAsset(const OpaaxString& InSourcePath, OpaaxStringID InAssetID);
        ~SceneAsset() override;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        SceneAsset(const SceneAsset&)            = delete;
        SceneAsset& operator=(const SceneAsset&) = delete;

        // =============================================================================
        // Move - delete
        // =============================================================================
        SceneAsset(SceneAsset&&)                 = delete;
        SceneAsset& operator=(SceneAsset&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
        //~ Begin IAsset interface
    public:
        OpaaxStringID      GetAssetID()    const override { return m_AssetID;    }
        AssetType          GetType()       const override { return AssetType::Scene; }
        EAssetState        GetState()      const override { return m_State;      }
        const OpaaxString& GetSourcePath() const override { return m_SourcePath; }
        //~ End IAsset interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID m_AssetID    = {};
        OpaaxString   m_SourcePath;
        EAssetState   m_State      = EAssetState::Loaded;
    };

} // namespace Opaax
