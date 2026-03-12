#pragma once
#include "EngineAPI.h"
#include "OpaaxString.hpp"
#include "OpaaxTypes.h"

namespace Opaax {
    class OpaaxString;
}

namespace Opaax
{
    struct OPAAX_API OpaaxHash
    {
        // Constants for FNV-1a Hash
        static constexpr Uint32 FNV1a_Prime = 16777619u;
        static constexpr Uint32 FNV1a_OffsetBasis = 2166136261u;

        /**
         * Computes the FNV-1a hash for a given null-terminated string.
         * This is a constexpr function that allows compile-time hash calculations.
         *
         * @param Str A pointer to a null-terminated string to be hashed.
         * @param HashValue An optional parameter representing the current hash value.
         *        Defaults to the FNV-1a Offset Basis if not provided.
         * @return The computed FNV-1a hash as a 32-bit unsigned integer.
         */
        static constexpr Uint32 Hash(const char* Str, Uint32 HashValue = FNV1a_OffsetBasis)
        {
            return *Str ? Hash(Str + 1, (HashValue ^ static_cast<Uint32>(*Str)) * FNV1a_Prime) : HashValue;
        }

        /**
         * Computes the FNV-1a hash for an `OpaaxString` object.
         * This function utilizes the `Data` method of `OpaaxString` to retrieve the string
         * and calculates the hash recursively.
         *
         * @param String The `OpaaxString` object for which the hash is to be computed.
         * @param HashValue An optional parameter representing the current hash value.
         *        Defaults to the FNV-1a Offset Basis if not provided.
         * @return The computed FNV-1a hash as a 32-bit unsigned integer.
         */
        static constexpr Uint32 Hash(OpaaxString& String, Uint32 HashValue = FNV1a_OffsetBasis)
        {
            return Hash(String.Data(), HashValue);
        }
        
        Uint32 operator()(OpaaxString& String) const
        {
            return Hash(String);  
        }

        Uint32 operator()(const OpaaxString& String) const
        {
            return Hash(String.CStr());  
        }

        Uint32 operator()(const char* String) const
        {
            return Hash(String);  
        }
    };
}
