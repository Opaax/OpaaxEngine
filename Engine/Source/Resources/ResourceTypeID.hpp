#pragma once

#include <string_view>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Hash/OpaaxHash.h"
#include "Core/Reflection/TypeInfo.h"
#include "Core/Log/Logger.h"

// =============================================================================
// ResourceTypeID — Get<T>() maps a resource type to a dense Uint32 pool index.
//   One registry for the whole program; the same type always maps to the same index.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogResourceType{"ResourceType"};
    
    // -------------------------------------------------------------------------
    // Registry functions (the registry lives in ResourceTypeID.cpp).
    //   InternResourceType: same hash but different name is fatal.
    // -------------------------------------------------------------------------
    Uint32 InternResourceType(Uint64 InHash, std::string_view InName);
    Uint32 GetResourceTypeCount();

    // =============================================================================
    // ResourceTypeID
    // =============================================================================
    class ResourceTypeID final
    {
    public:
        template<typename T>
        static Uint32 Get() noexcept
        {
            // Cached per module, but the value comes from the shared registry.
            // The signature is unique per type, so it also serves the collision check.
            constexpr std::string_view lName = TypeSignature<T>();
            constexpr Uint64           lHash = OpaaxHash::Hash64(lName);
            static const Uint32        s_Index = InternResourceType(lHash, lName);
            return s_Index;
        }
    };
}
