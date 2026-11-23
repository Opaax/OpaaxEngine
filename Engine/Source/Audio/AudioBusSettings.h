#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Tag/OpaaxTag.h"
#include "Core/Tag/OpaaxTagJson.h"

namespace Opaax
{
    /** The master bus, and the root of every bus's tag: a bus is an Audio.Bus tag ("Audio.Bus.Music"). */
    inline OpaaxTag AudioMasterBus() { static const OpaaxTag s_Tag("Audio.Bus"); return s_Tag; }

    /** The bus a sound plays in when it names none. */
    inline OpaaxTag AudioDefaultBus() { static const OpaaxTag s_Tag("Audio.Bus.SFX"); return s_Tag; }

    /**
     * A bus name as the mix reads it: a tag under Audio.Bus. A name written before tags reads under that
     * root ("Music" is Audio.Bus.Music, "Master" the master); none stays none.
     */
    inline OpaaxTag ToBusTag(const OpaaxTag InName)
    {
        static const OpaaxTag s_OldMaster("Master");
        return InName == s_OldMaster ? AudioMasterBus() : WithRoot(AudioMasterBus(), InName);
    }

    // =============================================================================
    // AudioBusSettings — one bus of the mix tree, as the engine config lists it. Its name is an
    //   Audio.Bus tag, and the tree comes from the names: Audio.Bus.SFX.Footsteps is under Audio.Bus.SFX,
    //   every bus under the master, Audio.Bus. A bus written before tags (a name, and a Parent) reads as
    //   its tag: "Footsteps" under "SFX" is Audio.Bus.SFX.Footsteps.
    // =============================================================================
    struct AudioBusSettings
    {
        /** Its tag: sounds name it to play in this bus ("Audio.Bus.Music"). */
        OpaaxTag Name;

        /** Linear: the volume an options menu sets. */
        float Volume = 1.f;

        friend void to_json(nlohmann::json& OutJson, const AudioBusSettings& InBus)
        {
            OutJson = nlohmann::json{ { "Name", InBus.Name }, { "Volume", InBus.Volume } };
        }

        friend void from_json(const nlohmann::json& InJson, AudioBusSettings& OutBus)
        {
            OutBus = AudioBusSettings{};
            OutBus.Volume = InJson.value("Volume", 1.f);

            const auto lName = InJson.find("Name");
            if (lName == InJson.end())
            {
                return;
            }

            const OpaaxTag lTag    = lName->get<OpaaxTag>();
            const auto     lParent = InJson.find("Parent");
            const OpaaxTag lUnder  = (lParent != InJson.end() && lParent->is_string()) ? lParent->get<OpaaxTag>() : OpaaxTag();

            // A name written before tags, under a parent: the parent's tag, then the name.
            OutBus.Name = (lTag.IsValid() && lUnder.IsValid() && !lTag.MatchesTag(AudioMasterBus()))
                              ? OpaaxTag(ToBusTag(lUnder).ToString() + "." + lTag.ToString())
                              : ToBusTag(lTag);
        }

        OPAAX_PROPERTIES(AudioBusSettings,
                         OPAAX_PROP(Name).SetTooltip("Sounds name it to play in this bus (\"Audio.Bus.Music\").\n"
                                                     "One more name puts it under a bus (\"Audio.Bus.SFX.Footsteps\")."),
                         OPAAX_PROP(Volume).SetRange(0.f, 1.f))
    };

    /** The buses a new config lists: under the master, Music, SFX, Dialogue, Ambience and UI. */
    inline TDynArray<AudioBusSettings> DefaultAudioBuses()
    {
        TDynArray<AudioBusSettings> lBuses;
        for (const char* lName : { "Audio.Bus.Music", "Audio.Bus.SFX", "Audio.Bus.Dialogue", "Audio.Bus.Ambience", "Audio.Bus.UI" })
        {
            AudioBusSettings lBus;
            lBus.Name = OpaaxTag(lName);
            lBuses.push_back(lBus);
        }
        return lBuses;
    }
}
