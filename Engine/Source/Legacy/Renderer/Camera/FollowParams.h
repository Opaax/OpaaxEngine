#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "ECS/OpaaxEntity.hpp"

#include <nlohmann/json.hpp>

namespace Opaax
{
    using json = nlohmann::json;

    /**
     * @struct FollowParams
     *
     * Data-driven parameters for FollowCameraController. Designed so a future
     * "FollowProfile" asset can serialize and ship presets — Target is runtime-bound
     * (resolved per scene by gameplay code) and is intentionally skipped by the
     * json codec.
     */
    struct OPAAX_API FollowParams
    {
        // Runtime-bound — not serialized. Game code sets it per scene / per player.
        EntityID Target    = ENTITY_NONE;

        // Asset-shaped fields — survive a future FollowProfile codec.
        Vector2F Offset    = { 0.f, 0.f };
        float    Smoothing = 0.1f;            // seconds to halve the gap; <= 0 snaps instantly
        Vector2F Deadzone  = { 0.f, 0.f };    // half-extents of a no-follow box around target
    };

    // =============================================================================
    // nlohmann ADL — serializes asset fields only; Target stays default on load.
    // =============================================================================
    inline void to_json(json& OutJson, const FollowParams& InParams)
    {
        OutJson = json{
            { "offset",    { InParams.Offset.x,   InParams.Offset.y   } },
            { "smoothing", InParams.Smoothing },
            { "deadzone",  { InParams.Deadzone.x, InParams.Deadzone.y } }
        };
    }

    inline void from_json(const json& InJson, FollowParams& OutParams)
    {
        if (InJson.contains("offset"))
        {
            OutParams.Offset.x = InJson["offset"][0].get<float>();
            OutParams.Offset.y = InJson["offset"][1].get<float>();
        }
        if (InJson.contains("smoothing"))
        {
            OutParams.Smoothing = InJson["smoothing"].get<float>();
        }
        if (InJson.contains("deadzone"))
        {
            OutParams.Deadzone.x = InJson["deadzone"][0].get<float>();
            OutParams.Deadzone.y = InJson["deadzone"][1].get<float>();
        }
    }

} // namespace Opaax
