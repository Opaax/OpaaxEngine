#pragma once

#include <cstdio>
#include <cstring>
#include <ostream>
#include <string_view>
#include "Core/OpaaxTypes.h"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    constexpr char OpaaxString_InvalidCharacter = '\0';

    /**
     * String with small string optimization (15 chars inline). Heap strings grow 2x.
     * 24 bytes. Same thread safety as std::string.
     *
     * @see OpaaxStringID for interned strings.
     * @see Core/Hash/OpaaxHash.h for std::hash<OpaaxString>.
     */
    class OPAAX_API OpaaxString final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    private:
        static constexpr Uint32 SSOCapacity = 15;

        // Heap growth at least doubles (avoids O(n^2) appends).
        static constexpr Uint32 GROWTH_FACTOR = 2;
        static constexpr Uint32 MIN_HEAP_CAPACITY = 32;

        // One below UINT32_MAX so Capacity + 1 cannot wrap to 0.
        static constexpr Uint32 MAX_CAPACITY = UINT32_MAX - 1;

        // Substring — returns a new OpaaxString
        static OpaaxString SubString(const OpaaxString& InStr, Uint32 Start, Uint32 InLength = UINT32_MAX)
        {
            if (Start >= InStr.Length) { return OpaaxString(); }

            // Clamp against the remainder (Start + InLength can overflow).
            const Uint32 lRemaining = InStr.Length - Start;
            const Uint32 lActual    = (InLength > lRemaining) ? lRemaining : InLength;

            return OpaaxString(InStr.CStr() + Start, lActual);
        }

    public:
        /**
         * Decimal text of an integer.
         */
        static OpaaxString FromInt(Int64 InValue)
        {
            char lBuffer[24];   // Int64 min is 20 chars + sign + null
            const int lWritten = std::snprintf(lBuffer, sizeof(lBuffer), "%lld", static_cast<long long>(InValue));
            return (lWritten > 0) ? OpaaxString(lBuffer) : OpaaxString();
        }

        static OpaaxString FromUInt(Uint64 InValue)
        {
            char lBuffer[24];
            const int lWritten = std::snprintf(lBuffer, sizeof(lBuffer), "%llu", static_cast<unsigned long long>(InValue));
            return (lWritten > 0) ? OpaaxString(lBuffer) : OpaaxString();
        }

        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpaaxString() = default;

        ~OpaaxString() { Clear(); }

        OpaaxString(const char* Str) : SSOBuffer{0}, Length(0), Capacity(0), bUsingHeap(false)
        {
            if (!Str) { return; }

            const Uint32 lLength = static_cast<Uint32>(std::strlen(Str));
            Length = lLength;

            if (lLength <= SSOCapacity)
            {
                std::memcpy(SSOBuffer, Str, lLength);
                SSOBuffer[lLength] = OpaaxString_InvalidCharacter;
            }
            else
            {
                AllocateAndCopyHeap(Str, lLength, lLength);
            }
        }

        /** Str need not be null-terminated; nothing past Count is read. */
        OpaaxString(const char* Str, Uint32 Count) { Append(Str, Count); }

        /** Explicit, so overloads prefer the const char* form. */
        explicit OpaaxString(std::string_view InView)
            : OpaaxString(InView.data(), static_cast<Uint32>(InView.size())) {}

        explicit OpaaxString(OpaaxStringView InView)
            : OpaaxString(InView.Data(), InView.GetLength()) {}

        OpaaxString(const OpaaxString& Other)
            : Length(Other.Length), Capacity(0), bUsingHeap(false)
        {
            if (Other.bUsingHeap)
            {
                AllocateAndCopyHeap(Other.HeapData, Other.Length, Other.Capacity);
            }
            else
            {
                std::memcpy(SSOBuffer, Other.SSOBuffer, Other.Length + 1);
            }
        }

        /**
         * Takes the heap pointer; leaves Other empty.
         */
        OpaaxString(OpaaxString&& Other) noexcept
            : Length(Other.Length), Capacity(Other.Capacity), bUsingHeap(Other.bUsingHeap)
        {
            if (Other.bUsingHeap)
            {
                HeapData = Other.HeapData;
                Other.HeapData = nullptr;
            }
            else
            {
                std::memcpy(SSOBuffer, Other.SSOBuffer, Other.Length + 1);
            }

            Other.Length = 0;
            Other.Capacity = 0;
            Other.bUsingHeap = false;
            Other.SSOBuffer[0] = OpaaxString_InvalidCharacter;
        }

        OpaaxString& operator=(const OpaaxString& Other)
        {
            if (this != &Other)
            {
                Clear();
                Length = Other.Length;
                Capacity = 0;

                if (Other.bUsingHeap)
                {
                    AllocateAndCopyHeap(Other.HeapData, Other.Length, Other.Capacity);
                }
                else
                {
                    bUsingHeap = false;
                    std::memcpy(SSOBuffer, Other.SSOBuffer, Other.Length + 1);
                }
            }
            return *this;
        }

        OpaaxString& operator=(OpaaxString&& Other) noexcept
        {
            if (this != &Other)
            {
                Clear();

                Length = Other.Length;
                Capacity = Other.Capacity;
                bUsingHeap = Other.bUsingHeap;

                if (Other.bUsingHeap)
                {
                    HeapData = Other.HeapData;
                    Other.HeapData = nullptr;
                }
                else
                {
                    std::memcpy(SSOBuffer, Other.SSOBuffer, Other.Length + 1);
                }

                Other.Length = 0;
                Other.Capacity = 0;
                Other.bUsingHeap = false;
                Other.SSOBuffer[0] = OpaaxString_InvalidCharacter;
            }
            return *this;
        }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        // InCapacity keeps the source's reserved capacity on copy.
        void AllocateAndCopyHeap(const char* Str, Uint32 InLength, Uint32 InCapacity)
        {
            const Uint32 lCapacity = (InCapacity >= InLength) ? InCapacity : InLength;
            HeapData = new char[lCapacity + 1];
            std::memcpy(HeapData, Str, InLength);
            HeapData[InLength] = '\0';
            Capacity = lCapacity;
            bUsingHeap = true;
        }

        /**
         * New capacity = max(current * 2, needed, MIN_HEAP_CAPACITY), clamped.
         */
        void GrowHeap(Uint32 NewLength)
        {
            OPAAX_ASSERT(NewLength <= MAX_CAPACITY);

            const Uint32 lNewCapacity = [&]() -> Uint32
            {
                // Double only while it fits (Capacity * 2 can wrap).
                Uint32 lCap = bUsingHeap
                                  ? ((Capacity <= MAX_CAPACITY / GROWTH_FACTOR) ? Capacity * GROWTH_FACTOR : MAX_CAPACITY)
                                  : MIN_HEAP_CAPACITY;
                if (lCap < NewLength) { lCap = NewLength; }
                if (lCap > MAX_CAPACITY) { lCap = MAX_CAPACITY; }
                return lCap;
            }();

            char* lNewHeap = new char[lNewCapacity + 1];

            if (bUsingHeap)
            {
                std::memcpy(lNewHeap, HeapData, Length);
                delete[] HeapData;
            }
            else
            {
                std::memcpy(lNewHeap, SSOBuffer, Length);
            }

            HeapData = lNewHeap;
            Capacity = lNewCapacity;
            bUsingHeap = true;
        }

        //----------------------------------------------------------------------------------------

    public:
        void Clear() noexcept
        {
            if (bUsingHeap && HeapData)
            {
                delete[] HeapData;
                HeapData = nullptr;
            }

            bUsingHeap = false;
            Length = 0;
            Capacity = 0;
            SSOBuffer[0] = OpaaxString_InvalidCharacter;
        }

        /**
         * Appends exactly Count bytes. Str need not be null-terminated.
         */
        void Append(const char* Str, Uint32 Count)
        {
            if (!Str || Count == 0) { return; }

            // Refuse an append that would overflow.
            if (Count > MAX_CAPACITY - Length)
            {
                OPAAX_ASSERT(false);
                return;
            }

            const Uint32 lNewLength = Length + Count;

            if (!bUsingHeap && lNewLength <= SSOCapacity)
            {
                // Fast path: still fits in SSO
                std::memcpy(SSOBuffer + Length, Str, Count);
                Length = lNewLength;
                SSOBuffer[Length] = OpaaxString_InvalidCharacter;
                return;
            }

            // Heap path: grow only if needed
            if (!bUsingHeap || lNewLength > Capacity)
            {
                // Self-append: GrowHeap frees the buffer Str points into, so keep the offset.
                const char*  lSelf    = CStr();
                const bool   lAliases = (Str >= lSelf) && (Str <= lSelf + Length);
                const Uint32 lOffset  = lAliases ? static_cast<Uint32>(Str - lSelf) : 0;

                GrowHeap(lNewLength);

                if (lAliases) { Str = HeapData + lOffset; }
            }

            std::memcpy(HeapData + Length, Str, Count);
            Length = lNewLength;
            HeapData[Length] = OpaaxString_InvalidCharacter;
        }

        void Append(const char* Str)
        {
            if (!Str) { return; }
            Append(Str, static_cast<Uint32>(std::strlen(Str)));
        }

        void Append(const OpaaxString& Other) { Append(Other.CStr(), Other.Length); }

        /**
         * Reserves capacity without changing the length.
         */
        void Reserve(Uint32 InCapacity)
        {
            if ((bUsingHeap && InCapacity <= Capacity) || (!bUsingHeap && InCapacity <= SSOCapacity))
            {
                return;
            }

            OPAAX_ASSERT(InCapacity <= MAX_CAPACITY);

            // Clamp rather than wrap (allocation then fails with bad_alloc).
            GrowHeap(InCapacity > MAX_CAPACITY ? MAX_CAPACITY : InCapacity);
        }

        OpaaxString SubString(Uint32 Start, Uint32 InLength = UINT32_MAX) const { return SubString(*this, Start, InLength); }

        Int32 Find(const char* Str, Uint32 StartPos = 0) const
        {
            // StartPos == Length is allowed; past it strstr would read beyond the terminator.
            if (!Str || StartPos > Length) { return -1; }

            const char* lResult = std::strstr(CStr() + StartPos, Str);
            return lResult ? static_cast<Int32>(lResult - CStr()) : -1;
        }

        static constexpr char ToUpperChar(char Char) noexcept
        {
            return (Char >= 'a' && Char <= 'z') ? static_cast<char>(Char - 'a' + 'A') : Char;
        }

        static constexpr char ToLowerChar(char Char) noexcept
        {
            return (Char >= 'A' && Char <= 'Z') ? static_cast<char>(Char - 'A' + 'a') : Char;
        }

        OpaaxString ToUpper() const
        {
            OpaaxString lResult(*this);
            char* lData = lResult.Data();
            for (Uint32 i = 0; i < lResult.Length; ++i)
            {
                lData[i] = ToUpperChar(lData[i]);
            }
            return lResult;
        }

        OpaaxString ToLower() const
        {
            OpaaxString lResult(*this);
            char* lData = lResult.Data();
            for (Uint32 i = 0; i < lResult.Length; ++i)
            {
                lData[i] = ToLowerChar(lData[i]);
            }
            return lResult;
        }

        //----------------------------------------------------------------------------------------
        //Get - Set

        const char* CStr() const noexcept   { return bUsingHeap ? HeapData : SSOBuffer; }
        char*       Data() noexcept         { return bUsingHeap ? HeapData : SSOBuffer; }

        Uint32  GetLength()     const noexcept { return Length; }
        bool    IsUsingHeap()   const noexcept { return bUsingHeap; }
        bool    IsEmpty()       const noexcept { return Length == 0; }

        bool IsValidIndex(Uint32 Index) const
        {
            if (Index >= Length)
            {
                return false;
            }
            return true;
        }

        Uint32 GetMemoryUsage() const noexcept
        {
            return bUsingHeap ? (Capacity + 1) * sizeof(char) : sizeof(SSOBuffer);
        }

        // =============================================================================
        // Operators
        // =============================================================================
    public:
        // Implicit, so functions taking an OpaaxStringView accept strings too. The view borrows our bytes.
        operator OpaaxStringView() const noexcept { return OpaaxStringView(CStr(), Length); }

        char operator[](Uint32 Index) const
        {
            return IsValidIndex(Index) ? CStr()[Index] : OpaaxString_InvalidCharacter;
        }

        char operator[](Int32 Index) const
        {
            return IsValidIndex(static_cast<Uint32>(Index)) ? CStr()[Index] : OpaaxString_InvalidCharacter;
        }

        OpaaxString& operator+=(const char* Other)
        {
            Append(Other);
            return *this;
        }

        OpaaxString& operator+=(const OpaaxString& Other)
        {
            Append(Other);
            return *this;
        }

        bool operator==(const OpaaxString& Other)   const noexcept { return std::strcmp(CStr(), Other.CStr()) == 0; }
        bool operator==(const char* Str)            const noexcept { return std::strcmp(CStr(), Str) == 0; }
        bool operator!=(const OpaaxString& Other)   const noexcept { return !(*this == Other); }
        bool operator!=(const char* Str)            const noexcept { return !(*this == Str); }

        OpaaxString operator+(const char* RHS) const
        {
            OpaaxString lR(*this);
            lR += RHS;
            return lR;
        }

        OpaaxString operator+(const OpaaxString& RHS) const
        {
            OpaaxString lR(*this);
            lR += RHS;
            return lR;
        }

        friend std::ostream& operator<<(std::ostream& OS, const OpaaxString& Str) { return OS << Str.CStr(); }

        // =============================================================================
        // Members
        // Ordered to minimise padding: union(8) | Length(4) | Capacity(4) | bUsingHeap(1)
        // =============================================================================
    private:
        union
        {
            char SSOBuffer[SSOCapacity + 1]{0};
            char* HeapData;
        };

        Uint32 Length = 0;
        Uint32 Capacity = 0; // 0 when using SSO
        bool bUsingHeap = false;
    };

    // Declared in OpaaxStringView.hpp; defined here because it needs the complete OpaaxString.
    inline OpaaxString OpaaxStringView::ToString() const { return OpaaxString(m_Data, m_Length); }
} // namespace Opaax

// Formatter for spdlog / fmtlib. Formats a view (no copy, no strlen).
#include <spdlog/fmt/fmt.h>

template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<Opaax::OpaaxString, T>::value, char>>
    : fmt::formatter<fmt::string_view>
{
    auto format(const T& String, format_context& CTX) const
    {
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(String.CStr(), String.GetLength()), CTX);
    }
};
