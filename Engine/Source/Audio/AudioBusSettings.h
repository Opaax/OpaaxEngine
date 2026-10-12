#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"

namespace Opaax
{
    // =============================================================================
    // AudioBusSettings — one bus of the mix tree, as the engine config lists it. Every bus is under
    //   the master ("Master", which is always there), directly or through its parent.
    // =============================================================================
    struct AudioBusSettings
    {
        OpaaxStringID Name;

        /** The bus above this one. None: the master. */
        OpaaxStringID Parent;

        /** Linear: the volume an options menu sets. */
        float Volume = 1.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioBusSettings, Name, Parent, Volume)

        OPAAX_PROPERTIES(AudioBusSettings,
                         OPAAX_PROP(Name).SetTooltip("Sounds name it to play in this bus (\"Music\")."),
                         OPAAX_PROP(Parent).SetTooltip("The bus above. Empty: the master."),
                         OPAAX_PROP(Volume).SetRange(0.f, 1.f))
    };

    /** The master's name. */
    inline OpaaxStringID AudioMasterBus()
    {
        static const OpaaxStringID s_Master = OPAAX_ID("Master");
        return s_Master;
    }

    /** The bus a sound plays in when it names none. */
    inline OpaaxStringID AudioDefaultBus()
    {
        static const OpaaxStringID s_Default = OPAAX_ID("SFX");
        return s_Default;
    }

    /** The buses a new config lists: under the master, Music, SFX, Dialogue, Ambience and UI. */
    inline TDynArray<AudioBusSettings> DefaultAudioBuses()
    {
        TDynArray<AudioBusSettings> lBuses;
        for (const char* lName : { "Music", "SFX", "Dialogue", "Ambience", "UI" })
        {
            AudioBusSettings lBus;
            lBus.Name = OPAAX_ID(lName);
            lBuses.push_back(lBus);
        }
        return lBuses;
    }
}
