#pragma once
 
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
 
        // operator() overloads required by std::unordered_map / std::unordered_set
        Uint32 operator()(const OpaaxString& String) const noexcept { return Hash(String.CStr()); }
        Uint32 operator()(const char*        Str)    const noexcept { return Hash(Str); }
    };
 
} // namespace Opaax