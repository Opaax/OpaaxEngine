#pragma once

#include "Core/OpaaxTypes.h"
#include "Resources/Asset/Asset.h"
#include "World/Prefab/PrefabData.h"
#include "World/Prefab/PrefabFile.h"

namespace Opaax
{
    // =============================================================================
    // PrefabAsset — a .opaaxprefab as an asset, read from the file itself: loaded once, shared by every
    //   placement. Holds the raw data (its own entities and the records of the prefabs it places);
    //   consumers use IPrefabResolver, which flattens it. A prefab that does not load places nothing.
    //   Its hard references are held by the map that places it, not here.
    // =============================================================================
    class PrefabAsset final : public Asset
    {
        OPAAX_OBJECT(PrefabAsset, Asset)
        OPAAX_ASSET_SOURCE(PrefabFile::PREFAB_EXTENSION)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        PrefabAsset();
        ~PrefabAsset() override;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Asset interface
    public:
        /** Reads the prefab file. */
        bool LoadSource(const char* InAbsPath) override;

        /** The entity and component records (not the JSON payloads). */
        Uint64 GetResourceSize() const override;
        //~End Asset interface

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The prefab as saved, not flattened. */
        const PrefabData& GetData() const noexcept { return m_Data; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        PrefabData m_Data;
    };
}
