#pragma once
 
#include <string_view>

#include "EngineAPI.h"
#include "OpaaxString.hpp"
#include "OpaaxTypes.h"
 
namespace Opaax
{
    struct OPAAX_API OpaaxHash
    {
        // FNV-1a 32-bit constants
        static constexpr Uint32 FNV1a_Prime       = 16777619u;
        static constexpr Uint32 FNV1a_OffsetBasis = 2166136261u;
        
        static constexpr Uint32 Hash(const char* Str, Uint32 HashValue = FNV1a_OffsetBasis) noexcept
        {
            while (*Str)
            {
                HashValue ^= static_cast<Uint32>(static_cast<unsigned char>(*Str++));
                HashValue *= FNV1a_Prime;
            }
            return HashValue;
        }
        
        static Uint32 Hash(const OpaaxString& String) noexcept
        {
            return Hash(String.CStr());
        }
 
        // FNV-1a 64-bit constants
        static constexpr Uint64 FNV1a_Prime64       = 1099511628211ull;
        static constexpr Uint64 FNV1a_OffsetBasis64 = 14695981039346656037ull;

        // Wide, collision-resistant hash for stable identities (e.g. ResourceTypeID
        // over a compile-time type signature). string_view-based: works on non
        // null-terminated views and folds string literals via string_view's ctor.
        static constexpr Uint64 Hash64(std::string_view Str, Uint64 HashValue = FNV1a_OffsetBasis64) noexcept
        {
            for (const char lChar : Str)
            {
                HashValue ^= static_cast<Uint64>(static_cast<unsigned char>(lChar));
                HashValue *= FNV1a_Prime64;
            }
            return HashValue;
        }

        // operator() overloads required by std::unordered_map / std::unordered_set
        Uint32 operator()(const OpaaxString& String) const noexcept { return Hash(String.CStr()); }
        Uint32 operator()(const char*        Str)    const noexcept { return Hash(Str); }
    };
 
} // namespace Opaax