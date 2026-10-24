#pragma once

#include "Core/OpaaxTypes.h"
#include "Resources/Asset/Asset.h"
#include "World/Serialization/MapData.h"
#include "World/Serialization/MapFile.h"

namespace Opaax
{
    // =============================================================================
    // MapAsset — a .opaaxmap as an asset, read from the file itself: loaded once, shared by every world
    //   that mounts it. Holds the map as saved (its entities and its folded prefab placements); a Level
    //   instantiates it. A map that does not load mounts nothing, never an empty world.
    // =============================================================================
    class MapAsset final : public Asset
    {
        OPAAX_OBJECT(MapAsset, Asset)
        OPAAX_ASSET_SOURCE(MapFile::MAP_EXTENSION)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        MapAsset();
        ~MapAsset() override;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Asset interface
    public:
        /** Reads the map file. */
        bool LoadSource(const char* InAbsPath) override;

        /** The entity, component and placement records (not the JSON payloads). */
        Uint64 GetResourceSize() const override;
        //~End Asset interface

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The map as saved: placements folded. */
        const MapData& GetData() const noexcept { return m_Data; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        MapData m_Data;
    };
}
