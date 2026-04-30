#pragma once
#include "Core/Component/OpaaxComponent.h"
#include "ECS/OpaaxEntity.hpp"

namespace Opaax::ECS
{
    using json = nlohmann::json;

    /**
     * @struct ParentComponent
     *
     * Optional component placed on a child entity. Carries the runtime EntityID of the parent.
     * Persistence: the serializer writes the parent's UUID (not its EntityID) and resolves it
     * back to a runtime ID after all entities are loaded — see SceneSerializer.
     */
    struct OPAAX_API ParentComponent : public OpaaxComponentBase
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        ParentComponent() = default;
        explicit ParentComponent(EntityID InParent) : Parent(InParent) {}
        explicit ParentComponent(const json& Json) : OpaaxComponentBase(Json) { Deserialize(Json); }
        virtual ~ParentComponent() = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin OpaaxComponentBase Interface
        // NOTE: Serialize/Deserialize here only round-trip the runtime EntityID for symmetry.
        //   The scene-level link is written/read by SceneSerializer using parent UUIDs.
    protected:
        void DeserializeImplementation(const json& Json) override;

    public:
        json Serialize() const override;
        //~ End OpaaxComponentBase Interface

        // =============================================================================
        // Members
        // =============================================================================
        EntityID Parent = ENTITY_NONE;
    };
}
