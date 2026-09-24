#pragma once

#include <string_view>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Hash/OpaaxHash.h"
#include "Application/Services/ILogger.h"

// =============================================================================
// ResourceTypeID — cross-module type identity, stable across DLLs and runs.
//
//   ResourceTypeID::Get<T>() maps a resource type to a DENSE Uint32 pool index.
//   The index comes from a constexpr FNV-1a-64 hash of the compiler-generated
//   type signature, interned through ONE engine-exported registry (single
//   instance across the DLL/exe boundary). Deterministic across modules and runs
//   (same compiler ⇒ same signature ⇒ same hash ⇒ same index) — the property M4
//   serialization will lean on.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogResourceType{"ResourceType"};
    
    // -------------------------------------------------------------------------
    // Compile-time type signature — unique + stable per type per compiler. We hash
    // the whole signature string (no name extraction needed): distinct types have
    // distinct signatures, so it doubles as the collision-check name.
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
    // Exported registry plumbing. Implementation (the single registry instance) is
    // hidden in ResourceTypeID.cpp; both the engine DLL and consumer exes call these
    // exported functions, so every module resolves against the SAME map.
    //   InternResourceType: hash-equal / name-different ⇒ FATAL in all builds
    //   (a silent collision means two types share one pool — memory corruption).
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
            // NOTE: hash + signature are computed once at compile time; the interned
            // index is cached in a per-module local static, but the VALUE is globally
            // consistent because InternResourceType returns from the shared registry.
            constexpr std::string_view lName = ResourceTypeName<T>();
            constexpr Uint64           lHash = OpaaxHash::Hash64(lName);
            static const Uint32        s_Index = InternResourceType(lHash, lName);
            return s_Index;
        }
    };
}
