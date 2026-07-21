#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

#include <nlohmann/json.hpp>

namespace Opaax
{
    using json = nlohmann::json;

    // =============================================================================
    // EShakeDecay — X-Macro driven (Renderer/Camera/ShakeDecayList.h)
    // =============================================================================
    /**
     * @enum EShakeDecay
     * Amplitude envelope shape over a shake's Duration. The enum body and
     * the matching g_ShakeDecayIDs[] are both generated from ShakeDecayList.h —
     * adding a new curve means a single new line in that list file.
     */
    enum class EShakeDecay : Uint8
    {
        #define OPAAX_SHAKE_DECAY(Name) Name,
        #include "ShakeDecayList.h"
        #undef OPAAX_SHAKE_DECAY
        Count
    };

    /*** Parallel canonical-name array. Index by static_cast<Uint8>(EShakeDecay). */
    inline const OpaaxStringID g_ShakeDecayIDs[] =
    {
        #define OPAAX_SHAKE_DECAY(Name) OPAAX_ID(#Name),
        #include "ShakeDecayList.h"
        #undef OPAAX_SHAKE_DECAY
    };

    /*** EShakeDecay -> canonical OpaaxStringID. O(1) LUT. */
    inline const OpaaxStringID& ToStringID(EShakeDecay InDecay) noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InDecay);
        return (lIdx < static_cast<Uint8>(EShakeDecay::Count)) ? g_ShakeDecayIDs[lIdx] : g_ShakeDecayIDs[0];
    }

    /*** OpaaxStringID -> EShakeDecay. Linear scan; pure integer compare per slot. */
    inline EShakeDecay ShakeDecayFromStringID(const OpaaxStringID& InID) noexcept
    {
        for (Uint8 i = 0; i < static_cast<Uint8>(EShakeDecay::Count); ++i)
        {
            if (g_ShakeDecayIDs[i] == InID)
            {
                return static_cast<EShakeDecay>(i);
            }
        }
        return EShakeDecay::Linear;
    }

    // =============================================================================
    // ShakeParams
    // =============================================================================
    /**
     * @struct ShakeParams
     *
     * Data-driven parameters for ShakeCameraController. The entire struct is
     * asset-shaped — a future "ShakeAsset" codec can ship preset libraries
     * (punchy, tremor, impact) by serializing this directly.
     *
     * YFrequencyRatio + YPhase decouple the Y axis so the shake doesn't
     * degenerate into a single straight line. Defaults give a Lissajous-like
     * noise pattern; both are tunable per profile.
     */
    struct OPAAX_API ShakeParams
    {
        float       Amplitude       = 0.f;                  // peak pixel offset at t = 0
        float       Frequency       = 30.f;                 // base oscillation Hz on the X axis
        float       Duration        = 0.3f;                 // seconds before IsFinished returns true
        EShakeDecay Decay           = EShakeDecay::Linear;  // amplitude envelope shape
        float       YFrequencyRatio = 1.3f;                 // multiplies Frequency on the Y axis
        float       YPhase          = 1.7f;                 // radians; Y-axis phase offset
    };

    // =============================================================================
    // nlohmann ADL — entire struct is asset-shaped.
    // =============================================================================
    inline void to_json(json& OutJson, const ShakeParams& InParams)
    {
        const OpaaxString lDecayName = ToStringID(InParams.Decay).ToString();
        OutJson = json{
            { "amplitude",         InParams.Amplitude },
            { "frequency",         InParams.Frequency },
            { "duration",          InParams.Duration },
            { "decay",             lDecayName.CStr() },
            { "y_frequency_ratio", InParams.YFrequencyRatio },
            { "y_phase",           InParams.YPhase }
        };
    }

    inline void from_json(const json& InJson, ShakeParams& OutParams)
    {
        if (InJson.contains("amplitude"))         OutParams.Amplitude       = InJson["amplitude"].get<float>();
        if (InJson.contains("frequency"))         OutParams.Frequency       = InJson["frequency"].get<float>();
        if (InJson.contains("duration"))          OutParams.Duration        = InJson["duration"].get<float>();
        if (InJson.contains("decay"))
        {
            OutParams.Decay = ShakeDecayFromStringID(OpaaxStringID(InJson["decay"].get<std::string>()));
        }
        if (InJson.contains("y_frequency_ratio")) OutParams.YFrequencyRatio = InJson["y_frequency_ratio"].get<float>();
        if (InJson.contains("y_phase"))           OutParams.YPhase          = InJson["y_phase"].get<float>();
    }

} // namespace Opaax
