#pragma once

#include <nlohmann/json.hpp>
#include "Core/EngineAPI.h"

namespace Opaax
{
    using json = nlohmann::json;
    
    struct OPAAX_API ISerializable
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
        ISerializable()                         = default;
        explicit ISerializable(const json& InJson) {}
        virtual ~ISerializable()                = default;

        // =============================================================================
        // Function
        // =============================================================================
    protected:
        virtual void DeserializeImplementation(const json& Json) {}
        
    public:
        virtual json Serialize() const      { return {}; }
        void Deserialize(const json& Json)  { DeserializeImplementation(Json); }
    };
}
