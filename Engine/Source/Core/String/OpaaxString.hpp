#pragma once

#include <cstdio>   // snprintf — FromInt / FromUInt
#include <cstring>  // strlen / memcpy / strstr / strcmp
#include <ostream>  // operator<<
#include <string>   // ToStdString
#include <string_view>
#include "Core/OpaaxTypes.h"
#include "Core/EngineAPI.h"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    constexpr char OpaaxString_InvalidCharacter = '\0';

    /**
     * @class OpaaxString
     *
     * Custom string with SSO (Small String Optimisation) of 15 chars inline.
     * Heap strings use a 2x growth strategy.
     *
     * Layout (24 bytes total, no padding waste):
     *  union { SSOBuffer[16], HeapData* }  — 8 bytes (pointer-aligned)
     *  Uint32 Length                        — 4 bytes
     *  Uint32 Capacity                      — 4 bytes  (heap capacity, 0 when SSO)
     *  bool   bUsingHeap                    — 1 byte   (moved after the two Uint32s)
     *  [3 bytes padding — unavoidable with bool, acceptable]
     *
     * NOTE: SSOCapacity is 15 to keep null terminator within the 16-byte SSO slot.
     *
     * THREAD SAFETY — thread-COMPATIBLE, which is the whole guarantee a value type should make.
     * Every instance owns its buffer outright: no static state, no copy-on-write, no refcount, so
     * distinct instances on distinct threads share nothing and need no synchronisation. One instance
     * concurrently mutated is a race, exactly as with std::string; a per-string mutex would cost
     * every call site to serialise something that is never actually shared. Code that DOES need one
     * string visible to several threads holds it behind its own lock — see OpaaxStringID for the
     * shared, interned case.
     *
     * @see OpaaxStringID for the interned O(1)-compare handle.
     * @see Core/Hash/OpaaxHash.h for std::hash<OpaaxString> (it lives there to avoid an include cycle).
     */
    class OPAAX_API OpaaxString final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    private:
        static constexpr Uint32 SSOCapacity = 15;

        //When growing heap, we at least double. This avoids O(n^2) append cost.
        static constexpr Uint32 GROWTH_FACTOR = 2;
        static constexpr Uint32 MIN_HEAP_CAPACITY = 32;

        // One below UINT32_MAX so `Capacity + 1` (the terminator slot) can never wrap to 0 and hand
        // back an undersized buffer for the memcpy that follows.
        static constexpr Uint32 MAX_CAPACITY = UINT32_MAX - 1;

        // Static substring — allocates a new OpaaxString
        static OpaaxString SubString(const OpaaxString& InStr, Uint32 Start, Uint32 InLength = UINT32_MAX)
        {
            if (Start >= InStr.Length) { return OpaaxString(); }

            // Clamp against the REMAINDER, never against Start + InLength: that sum overflows for a
            // large InLength and wrapped into a copy far past the end. UINT32_MAX needs no case here.
            const Uint32 lRemaining = InStr.Length - Start;
            const Uint32 lActual    = (InLength > lRemaining) ? lRemaining : InLength;

            return OpaaxString(InStr.CStr() + Start, lActual);
        }

    public:
        /**
         * Decimal text for an integer. Kept as named statics rather than one overload set so a
         * call site states the signedness it means, the way the enum-to-string helpers do.
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

        /** Counted — Str need not be null-terminated, and nothing past Count is read. */
        OpaaxString(const char* Str, Uint32 Count) { Append(Str, Count); }

        /** Explicit so a view never silently wins an overload the const char* form should take. */
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
         * Steal the heap pointer; leave the source in a valid empty SSO state.
         * @param Other 
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
        //Capacity param lets copy preserve the source's reserved capacity.
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
         *growth strategy: new capacity = max(current*2, needed, MIN_HEAP_CAPACITY), clamped.
         */
        void GrowHeap(Uint32 NewLength)
        {
            OPAAX_ASSERT(NewLength <= MAX_CAPACITY);

            const Uint32 lNewCapacity = [&]() -> Uint32
            {
                // Double only while doubling still fits — `Capacity * 2` wraps past MAX_CAPACITY/2 and
                // the old code then allocated a buffer SMALLER than the memcpy below writes.
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
         * Append exactly Count bytes — the primitive the other two forward to.
         * Str need not be null-terminated, and anything past Count is never read.
         */
        void Append(const char* Str, Uint32 Count)
        {
            if (!Str || Count == 0) { return; }

            // Refusing an impossible append beats wrapping into a short buffer and overrunning it.
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
                // Self-append (`Str += Str`, or any slice of our own buffer): GrowHeap frees the very
                // buffer Str points into, so the memcpy below would read freed memory. The offset
                // survives the move; the pointer does not.
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

        // Length is already known — no strlen over a string that just told us how long it is.
        void Append(const OpaaxString& Other) { Append(Other.CStr(), Other.Length); }

        /**
         * Reserve capacity without changing length.
         * Call this before a known sequence of appends to avoid repeated reallocs.
         * @param InCapacity 
         */
        void Reserve(Uint32 InCapacity)
        {
            if ((bUsingHeap && InCapacity <= Capacity) || (!bUsingHeap && InCapacity <= SSOCapacity))
            {
                return;
            }

            OPAAX_ASSERT(InCapacity <= MAX_CAPACITY);

            // Clamp rather than wrap. The allocation then fails loudly with bad_alloc instead of
            // quietly handing back a zero-sized buffer.
            GrowHeap(InCapacity > MAX_CAPACITY ? MAX_CAPACITY : InCapacity);
        }

        std::string ToStdString() const                                         { return std::string(CStr()); }
        OpaaxString SubString(Uint32 Start, Uint32 InLength = UINT32_MAX) const { return SubString(*this, Start, InLength); }

        Int32 Find(const char* Str, Uint32 StartPos = 0) const
        {
            // StartPos == Length is legal (searches the empty tail); past it, `CStr() + StartPos`
            // would hand strstr a pointer beyond the terminator and it would read on.
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
        Uint32  GetCapacity()   const noexcept { return bUsingHeap ? Capacity : SSOCapacity; }
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
        // Implicit: this is what lets a function take an OpaaxStringView and still be called with a
        // string, a literal or a const char*. The view borrows OUR bytes and dies with them.
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
        // NOTE: Layout ordered to minimise padding:
        //   union(8) | Length(4) | Capacity(4) | bUsingHeap(1) + [3 pad]
        // =============================================================================
    private:
        union
        {
            char SSOBuffer[SSOCapacity + 1]{0};
            char* HeapData;
        };

        Uint32 Length = 0;
        Uint32 Capacity = 0; // 0 when using SSO; heap allocated capacity when on heap
        bool bUsingHeap = false;
    };

    // Declared in OpaaxStringView.hpp, defined here: it returns an OpaaxString BY VALUE, so it needs
    // the complete type. Same reason std::hash<OpaaxString> lives in OpaaxHash.h.
    inline OpaaxString OpaaxStringView::ToString() const { return OpaaxString(m_Data, m_Length); }
} // namespace Opaax

// Formatter for spdlog / fmtlib.
//
// Formats a VIEW over the bytes we already hold — it must not build a std::string. Inheriting
// fmt::formatter<std::string> and copying into one, which this did, put a heap allocation on every
// logged string past SSO, and made `"{}", Str` quietly more expensive than `"{}", Str.CStr()`. A
// string_view costs nothing and keeps the same format spec ({:>10} and friends) via the base parse().
// Length is passed explicitly, so there is no strlen either.
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
