#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Resources/Asset/Asset.h"

namespace Sandbox
{
    /** One group of a wave: how many enemies of a type come, and how they move. */
    struct WaveGroup
    {
        Opaax::OpaaxString Type;
        Opaax::Int32       Count = 0;
        Opaax::OpaaxString Pattern;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WaveGroup, Type, Count, Pattern)
        OPAAX_PROPERTIES(WaveGroup, OPAAX_PROP(Type), OPAAX_PROP(Count), OPAAX_PROP(Pattern))
    };

    // =============================================================================
    // EnemyWave — an asset class read from files of its own format: a .wave file is the asset, with
    //   nothing beside it (Waves/Wave01.wave). The file is JSON written by hand:
    //     { "wave": "Wave01", "enemies": [ { "type": "Grunt", "count": 8, "pattern": "SineSweep" } ] }
    //   Its window shows the groups, read-only: the file is the source.
    // =============================================================================
    class EnemyWave final : public Opaax::Asset
    {
        OPAAX_OBJECT(EnemyWave, Opaax::Asset)
        OPAAX_ASSET_SOURCE(".wave")

    public:
        /** The name the file gives the wave. */
        Opaax::OpaaxString WaveName;

        Opaax::TDynArray<WaveGroup> Groups;

        OPAAX_PROPERTIES(EnemyWave, OPAAX_PROP(WaveName), OPAAX_PROP(Groups))

        //~Begin Asset interface
        /** Reads the file; false when it is not a wave (missing, not JSON, no "enemies" list). */
        bool LoadSource(const char* InAbsPath) override;

        Opaax::Uint64 GetResourceSize() const override;
        //~End Asset interface
    };
}
