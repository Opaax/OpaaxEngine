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
