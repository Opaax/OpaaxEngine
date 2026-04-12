#pragma once
#include "Core/OpaaxMathTypes.h"
#include "Core/Component/OpaaxComponent.h"

namespace Opaax::ECS
{
    using json = nlohmann::json;
    
    /**
     * @struct TransformComponent
     *
     * 2D only for now. Z is draw order (painter's algorithm).
     * No dirty flag here — the render system reads this every frame.
     *      If transform caching becomes a perf issue, add a dirty bit then.
     */
    struct OPAAX_API TransformComponent : public OpaaxComponentBase
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        TransformComponent()             = default;
        TransformComponent(Vector2F InPosition, Vector2F InScale) : Position(InPosition), Scale {InScale} {}
        explicit TransformComponent(const json& Json) : OpaaxComponentBase(Json) { Deserialize(Json); }
        virtual ~TransformComponent()    = default;
        
        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin OpaaxComponentBase Interface
    protected:
        void DeserializeImplementation(const json& Json) override;
        
    public:
        json Serialize() const override;
        //~ End OpaaxComponentBase Interface

        // =============================================================================
        // Members
        // =============================================================================

        Vector2F Position = { 0.f, 0.f };
        Vector2F Scale    = { 1.f, 1.f };
        float    Rotation = 0.f;           // radians, CCW
        float    ZOrder   = 0.f;           // draw order — higher = drawn on top

    };
};
