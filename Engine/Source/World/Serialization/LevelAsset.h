#pragma once

#include "Core/OpaaxTypes.h"
#include "Resources/Asset/Asset.h"
#include "World/Serialization/LevelFile.h"

namespace Opaax
{
    // =============================================================================
    // LevelAsset — a .opaaxlevel as an asset, read from the file itself: the level's name and which
    //   maps it has. A level that does not load opens nothing, never an empty world. Its maps are not
    //   loaded with it: the world's Level mounts them one by one (loading them all at once would keep
    //   one from being unmounted).
    // =============================================================================
    class LevelAsset final : public Asset
    {
        OPAAX_OBJECT(LevelAsset, Asset)
        OPAAX_ASSET_SOURCE(LevelFile::LEVEL_EXTENSION)

        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        LevelAsset();
        ~LevelAsset() override;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin Asset interface
    public:
        /** Reads the level file. */
        bool LoadSource(const char* InAbsPath) override;

        /** The name and the map paths. */
        Uint64 GetResourceSize() const override;
        //~End Asset interface

        // =========================================================================
        // Get
        // =========================================================================
    public:
        /** The level as saved. */
        const LevelData& GetData() const noexcept { return m_Data; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        LevelData m_Data;
    };
}
