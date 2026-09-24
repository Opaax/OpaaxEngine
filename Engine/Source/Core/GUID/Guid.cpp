#include "Guid.h"

#include <random>

namespace
{
    constexpr char           k_HexDigits[]  = "0123456789abcdef";
    constexpr Opaax::Uint32  k_HalfChars    = 16;                  // one Uint64 in hex
    constexpr Opaax::Uint32  k_GuidChars    = k_HalfChars * 2;

    // Big-endian nibbles, so lexicographic order over the text matches numeric order over the
    // value — which is what lets a map file be sorted by its guid STRINGS (M5).
    void WriteHex64(Opaax::Uint64 InValue, char* OutChars) noexcept
    {
        for (Opaax::Int32 lIndex = static_cast<Opaax::Int32>(k_HalfChars) - 1; lIndex >= 0; --lIndex)
        {
            OutChars[lIndex] = k_HexDigits[InValue & 0xFull];
            InValue >>= 4;
        }
    }

    // SplitMix64's finalizer — the standard 64-bit avalanche mix. Every input bit affects every
    // output bit, which is what Derive needs from each of its stages.
    Opaax::Uint64 Mix64(Opaax::Uint64 InValue) noexcept
    {
        InValue ^= InValue >> 30;
        InValue *= 0xbf58476d1ce4e5b9ULL;
        InValue ^= InValue >> 27;
        InValue *= 0x94d049bb133111ebULL;
        InValue ^= InValue >> 31;

        return InValue;
    }

    bool ReadHex64(const char* InChars, Opaax::Uint64& OutValue) noexcept
    {
        Opaax::Uint64 lValue = 0;

        for (Opaax::Uint32 lIndex = 0; lIndex < k_HalfChars; ++lIndex)
        {
            const char    lChar = InChars[lIndex];
            Opaax::Uint64 lNibble;

            if      (lChar >= '0' && lChar <= '9') { lNibble = static_cast<Opaax::Uint64>(lChar - '0'); }
            else if (lChar >= 'a' && lChar <= 'f') { lNibble = static_cast<Opaax::Uint64>(lChar - 'a' + 10); }
            else if (lChar >= 'A' && lChar <= 'F') { lNibble = static_cast<Opaax::Uint64>(lChar - 'A' + 10); }
            else                                   { return false; }

            lValue = (lValue << 4) | lNibble;
        }

        OutValue = lValue;
        return true;
    }
}

namespace Opaax
{
    // =========================================================================
    // New — two 64-bit draws from a thread-local Mersenne engine, seeded once per
    // thread from random_device. Forced non-zero so the invalid sentinel can never
    // be minted by accident.
    // =========================================================================
    Guid Guid::New() noexcept
    {
        static thread_local std::mt19937_64 sEngine{ std::random_device{}() };
        std::uniform_int_distribution<Uint64> lDist;

        Guid lGuid;
        lGuid.High = lDist(sEngine);
        lGuid.Low  = lDist(sEngine);

        if ((lGuid.High | lGuid.Low) == 0)
        {
            lGuid.Low = 1;
        }

        return lGuid;
    }

    // =========================================================================
    // Derive — each half folds BOTH words of BOTH inputs, so the result cannot
    // inherit a collision from one 64-bit half of either argument. Forced non-zero
    // for New()'s reason: the invalid sentinel must never be mintable by accident.
    // =========================================================================
    Guid Guid::Derive(const Guid& InInstance, const Guid& InTemplate) noexcept
    {
        const Uint64 lTemplateSeed = Mix64(InTemplate.High ^ (InTemplate.Low + 0x9e3779b97f4a7c15ULL));
        const Uint64 lInstanceSeed = Mix64(InInstance.High ^ (InInstance.Low + 0x9e3779b97f4a7c15ULL));

        Guid lGuid;
        lGuid.High = Mix64(lTemplateSeed ^ lInstanceSeed);
        lGuid.Low  = Mix64(lTemplateSeed + ~lInstanceSeed);

        if ((lGuid.High | lGuid.Low) == 0)
        {
            lGuid.Low = 1;
        }

        return lGuid;
    }

    // =========================================================================
    // Text form
    // =========================================================================
    OpaaxString Guid::ToString() const
    {
        char lChars[k_GuidChars + 1];

        WriteHex64(High, lChars);
        WriteHex64(Low,  lChars + k_HalfChars);
        lChars[k_GuidChars] = '\0';

        return OpaaxString(lChars);
    }

    bool Guid::FromString(const OpaaxString& InText, Guid& OutGuid) noexcept
    {
        if (InText.GetLength() != k_GuidChars)
        {
            return false;
        }

        const char* lChars = InText.CStr();

        // Parsed into LOCALS first: a half-written OutGuid would be a different identity, not a
        // rejected one, and the caller would have no way to tell.
        Uint64 lHigh = 0;
        Uint64 lLow  = 0;
        if (!ReadHex64(lChars, lHigh) || !ReadHex64(lChars + k_HalfChars, lLow))
        {
            return false;
        }

        OutGuid.High = lHigh;
        OutGuid.Low  = lLow;
        return true;
    }
}
