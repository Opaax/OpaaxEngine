#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // IMoverModeParams
    // =============================================================================
    /**
     * @interface IMoverModeParams
     *
     * Mode-owned, type-erased tuning for a single mover mode. Each IMoverMode defines its OWN
     * concrete params type (GroundMoveParams, FlyMoveParams, ...) so a mode only carries the knobs
     * it actually uses. The mover/physics system NEVER knows the concrete types — it holds only
     * IMoverModeParams* and calls these virtuals; the mode does the one downcast to its own type
     * (guarded by TypeTag). Same open/closed shape as IMoverMode itself.
     */
    class OPAAX_API IMoverModeParams
    {
    public:
        virtual ~IMoverModeParams() = default;

        // Stable per-concrete-type identity (address of a function-local static) — lets a mode
        // assert it was handed its own params type before downcasting.
        virtual const void* TypeTag() const = 0;

        // Serialization (authored on the MoverComponent, persisted per mode id).
        virtual nlohmann::json ToJson() const          = 0;
        virtual void           FromJson(const nlohmann::json&) = 0;

        // Deep copy (the MoverComponent owns params by UniquePtr; copy needs a clone).
        virtual UniquePtr<IMoverModeParams> Clone() const = 0;

        // Inspector editor. Body is gated #if OPAAX_WITH_EDITOR in each concrete .cpp (empty in
        // release, so engine-side params pull no ImGui into the release build).
        virtual void DrawEditor() = 0;
    };

    // Boilerplate for a concrete params type: a unique TypeTag + the override.
    #define OPAAX_MOVER_PARAMS_TYPE(Type)                                                   \
        static const void* StaticTypeTag() { static const int s_Tag = 0; return &s_Tag; }   \
        const void* TypeTag() const override { return StaticTypeTag(); }

} // namespace Opaax
