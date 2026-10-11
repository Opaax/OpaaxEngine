#pragma once

#include "Core/EngineAPI.h"
#include "Core/Serialize/Serializeable.h"

namespace Opaax
{
    using json = nlohmann::json;
    
    struct OPAAX_API IOpaaxComponent : public ISerializable
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        IOpaaxComponent()             = default;
        explicit IOpaaxComponent(const json& Json) : ISerializable(Json) {}
        virtual ~IOpaaxComponent()    = default;

        // =============================================================================
        // Override
        // =============================================================================

        //~ Begin ISerializable Interface
    protected:
        void DeserializeImplementation(const json& Json) override {}
        
    public:
        json Serialize() const                  override { return ISerializable::Serialize(); }
        //~ End ISerializable Interface
    };

    struct OPAAX_API OpaaxComponentBase : public IOpaaxComponent
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpaaxComponentBase()             = default;
        explicit OpaaxComponentBase(const json& Json) : IOpaaxComponent(Json) {}
        virtual ~OpaaxComponentBase()    = default;

        // =============================================================================
        // Override
        // =============================================================================

        //~ Begin ISerializable Interface
    protected:
        void DeserializeImplementation(const json& Json) override;
        
    public:
        json Serialize() const  override;
        //~ End ISerializable Interface
    };
}
