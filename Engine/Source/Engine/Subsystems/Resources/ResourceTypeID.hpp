#pragma once

#include <string_view>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Hash/OpaaxHash.h"
#include "Core/Log/Logger.h"

// =============================================================================
// ResourceTypeID — Get<T>() maps a resource type to a dense Uint32 pool index.
//   Same value in every module (one registry in the engine DLL) and every run.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogResourceType{"ResourceType"};
    
    // -------------------------------------------------------------------------
    // Type signature from the compiler: unique per type, also used for collision checks.
    // -------------------------------------------------------------------------
    template<typename T>
    constexpr std::string_view ResourceTypeName() noexcept
    {
#if defined(_MSC_VER)
        return __FUNCSIG__;
#elif defined(__clang__) || defined(__GNUC__)
        return __PRETTY_FUNCTION__;
#else
        return "";
#endif
    }

    // -------------------------------------------------------------------------
    // Registry functions (the registry lives in ResourceTypeID.cpp).
    //   InternResourceType: same hash but different name is fatal.
    // -------------------------------------------------------------------------
    OPAAX_API Uint32 InternResourceType(Uint64 InHash, std::string_view InName);
    OPAAX_API Uint32 GetResourceTypeCount();

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
            constexpr std::string_view lName = ResourceTypeName<T>();
            constexpr Uint64           lHash = OpaaxHash::Hash64(lName);
            static const Uint32        s_Index = InternResourceType(lHash, lName);
            return s_Index;
        }
    };
}
