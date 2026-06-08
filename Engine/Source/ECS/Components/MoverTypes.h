#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxMathTypes.h"

#include <nlohmann/json.hpp>

namespace Opaax
{
    using json = nlohmann::json;

    // =============================================================================
    // EMoverShape — collision proxy form for the geometric mover
    // =============================================================================
    /**
     * @enum EMoverShape
     * The capsule-family proxy the geometric mover sweeps. Capsule uses Height + Radius;
     * Circle uses Radius only (a degenerate capsule). Box/Polygon are NOT a geometric-mover
     * shape (Box2D's solver is capsule-only) — those arrive later as a kinematic-body mode.
     */
    enum class EMoverShape : Uint8
    {
        Capsule,
        Circle
    };

    inline const char* ToString(EMoverShape InShape) noexcept
    {
        switch (InShape)
        {
            case EMoverShape::Capsule: return "Capsule";
            case EMoverShape::Circle:  return "Circle";
        }
        return "Capsule";
    }

    inline EMoverShape MoverShapeFromString(const OpaaxString& InName) noexcept
    {
        if (InName == "Circle") { return EMoverShape::Circle; }
        return EMoverShape::Capsule;
    }

    // =============================================================================
    // MoverParams — movement-policy tuning (consumed by a mover mode)
    // =============================================================================
    /**
     * @struct MoverParams
     *
     * Data-driven tuning for a mover mode's movement policy (Quake-style ground/air model).
     * All speeds are world units / s, accelerations world units / s^2 unless noted. A mode
     * reads what it needs; unused fields cost nothing — modes stay decoupled from the schema.
     * Mirrors FollowParams/ShakeParams: a future "MoverProfile" asset reuses this codec.
     */
    struct OPAAX_API MoverParams
    {
        float Gravity      = 981.f;  // downward accel applied by the mode (NOT world gravity — geometric)
        float MaxSpeed     = 400.f;  // target horizontal speed at full input
        float Acceleration = 10.f;   // accel responsiveness multiplier (see Quake pmove model)
        float Friction     = 8.f;    // ground friction (1/s); bleeds speed when grounded
        float StopSpeed    = 100.f;  // floor under which friction is applied at a fixed rate
        float MinSpeed     = 10.f;   // below this the mover is snapped to rest
        float AirSteer     = 0.3f;   // [0..1] fraction of ground acceleration available in air
        float JumpSpeed    = 500.f;  // upward velocity set on a grounded jump
    };

    inline void to_json(json& OutJson, const MoverParams& InParams)
    {
        OutJson = json{
            { "gravity",      InParams.Gravity },
            { "max_speed",    InParams.MaxSpeed },
            { "acceleration", InParams.Acceleration },
            { "friction",     InParams.Friction },
            { "stop_speed",   InParams.StopSpeed },
            { "min_speed",    InParams.MinSpeed },
            { "air_steer",    InParams.AirSteer },
            { "jump_speed",   InParams.JumpSpeed }
        };
    }

    inline void from_json(const json& InJson, MoverParams& OutParams)
    {
        if (InJson.contains("gravity"))      { OutParams.Gravity      = InJson["gravity"].get<float>(); }
        if (InJson.contains("max_speed"))    { OutParams.MaxSpeed     = InJson["max_speed"].get<float>(); }
        if (InJson.contains("acceleration")) { OutParams.Acceleration = InJson["acceleration"].get<float>(); }
        if (InJson.contains("friction"))     { OutParams.Friction     = InJson["friction"].get<float>(); }
        if (InJson.contains("stop_speed"))   { OutParams.StopSpeed    = InJson["stop_speed"].get<float>(); }
        if (InJson.contains("min_speed"))    { OutParams.MinSpeed     = InJson["min_speed"].get<float>(); }
        if (InJson.contains("air_steer"))    { OutParams.AirSteer     = InJson["air_steer"].get<float>(); }
        if (InJson.contains("jump_speed"))   { OutParams.JumpSpeed    = InJson["jump_speed"].get<float>(); }
    }

    // =============================================================================
    // MoverInput — generic per-step intent (written by a producer, read by a mode)
    // =============================================================================
    /**
     * @struct MoverInput
     *
     * The intent fed to a mover each step — the Input ≠ Simulation ≠ State seam. Any producer
     * writes it (player controller, AI, cutscene, network); the active mode consumes it. Runtime
     * only, never serialized. MoveDir is a desired direction (x = horizontal axis for GroundMove);
     * Jump is an edge request the mode consumes when grounded.
     */
    struct MoverInput
    {
        Vector2F MoveDir = { 0.f, 0.f };
        bool     Jump    = false;
    };

} // namespace Opaax
