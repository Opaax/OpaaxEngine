#pragma once
#include "Core/OpaaxTypes.h"
#include "Core/Component/OpaaxComponent.h"

namespace Opaax::ECS
{
    using json = nlohmann::json;

    /**
     * @struct UuidComponent
     *
     * Stable 64-bit identity for an entity. Generated on CreateEntity and persisted by the
     * scene serializer so cross-references (e.g. ParentComponent) survive save/load even
     * though entt::entity values are not stable across deserialization.
     */
    struct OPAAX_API UuidComponent : public OpaaxComponentBase
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        UuidComponent() = default;
        explicit UuidComponent(Uint64 InId) : Id(InId) {}
        explicit UuidComponent(const json& Json) : OpaaxComponentBase(Json) { Deserialize(Json); }
        virtual ~UuidComponent() = default;

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
        Uint64 Id = 0;
    };

    /**
     * Free function: returns a non-zero random 64-bit ID.
     * Backed by a thread-local std::mt19937_64 seeded from std::random_device.
     */
    OPAAX_API Uint64 GenerateUuid();
}
