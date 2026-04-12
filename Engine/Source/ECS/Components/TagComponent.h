#pragma once
#include "Core/OpaaxStringID.hpp"
#include "Core/Component/OpaaxComponent.h"

namespace Opaax::ECS
{
    using json = nlohmann::json;
    
    /**
     * @class TagComponent
     *
     * Human-readable name for an entity. Used by editor hierarchy + debug logs.
     * OpaaxStringID — O(1) compare, no heap per entity.
     */
    struct OPAAX_API TagComponent : public OpaaxComponentBase
    {
        static OpaaxString TagComponentName;
        
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
        TagComponent() noexcept = default;
        explicit TagComponent(OpaaxStringID InTag) noexcept : Tag(InTag) {}
        explicit TagComponent(const json& Json) : OpaaxComponentBase(Json) { Deserialize(Json); }
        explicit TagComponent(const char* InTag) : Tag(InTag) {}

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
        OpaaxStringID Tag;
    };
}
