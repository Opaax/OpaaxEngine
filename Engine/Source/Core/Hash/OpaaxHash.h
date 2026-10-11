#pragma once
 
#include <string_view>

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/OpaaxTypes.h"
 
namespace Opaax
{
    struct OpaaxHash
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

        // Works on non-null-terminated views. Same result as the const char* version.
        static constexpr Uint32 Hash(OpaaxStringView View, Uint32 HashValue = FNV1a_OffsetBasis) noexcept
        {
            for (const char lChar : View)
            {
                HashValue ^= static_cast<Uint32>(static_cast<unsigned char>(lChar));
                HashValue *= FNV1a_Prime;
            }
            return HashValue;
        }
 
        // FNV-1a 64-bit constants
        static constexpr Uint64 FNV1a_Prime64       = 1099511628211ull;
        static constexpr Uint64 FNV1a_OffsetBasis64 = 14695981039346656037ull;

        // 64-bit hash for stable identities (e.g. ResourceTypeID).
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

// std::hash<OpaaxString>, so TUnorderedMap<OpaaxString, T> works with the default hasher.
// Lives here because OpaaxString.hpp cannot include this header.
template<>
struct std::hash<Opaax::OpaaxString>
{
    size_t operator()(const Opaax::OpaaxString& String) const noexcept
    {
        return static_cast<size_t>(Opaax::OpaaxHash::Hash(String.CStr()));
    }
};

// Same hash as OpaaxString, so a view can look up a string key.
template<>
struct std::hash<Opaax::OpaaxStringView>
{
    size_t operator()(Opaax::OpaaxStringView View) const noexcept
    {
        return static_cast<size_t>(Opaax::OpaaxHash::Hash(View));
    }
};
