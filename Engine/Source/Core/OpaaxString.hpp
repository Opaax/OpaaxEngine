#pragma once

#include <cstring>
#include <string_view>
#include "OpaaxTypes.h"
#include "EngineAPI.h"
#include "Log/OpaaxLog.h"

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

        // Static substring — allocates a new OpaaxString
        static OpaaxString SubString(const OpaaxString& InStr, Uint32 Start, Uint32 InLength = UINT32_MAX)
        {
            if (Start >= InStr.Length) { return OpaaxString(); }

            const Uint32 lActual = (InLength == UINT32_MAX || Start + InLength > InStr.Length)
                                       ? (InStr.Length - Start)
                                       : InLength;

            // PERF: Avoid double alloc — construct directly from pointer range.
            OpaaxString lResult;
            lResult.Reserve(lActual);
            lResult.Append(InStr.CStr() + Start); // TODO: add Append(const char*, Uint32 count) to avoid over-copy
            lResult.Length = lActual;
            if (lResult.bUsingHeap) { lResult.HeapData[lActual] = '\0'; }
            else { lResult.SSOBuffer[lActual] = '\0'; }
            return lResult;
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
         *growth strategy: new capacity = max(current*2, needed, MIN_HEAP_CAPACITY).
         */
        void GrowHeap(Uint32 NewLength)
        {
            const Uint32 lNewCapacity = [&]() -> Uint32
            {
                Uint32 lCap = bUsingHeap ? Capacity * GROWTH_FACTOR : MIN_HEAP_CAPACITY;
                if (lCap < NewLength) { lCap = NewLength; }
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

        void Append(const char* Str)
        {
            if (!Str || Str[0] == '\0') { return; }

            const Uint32 lStrLen = static_cast<Uint32>(std::strlen(Str));
            const Uint32 lNewLength = Length + lStrLen;

            if (!bUsingHeap && lNewLength <= SSOCapacity)
            {
                // Fast path: still fits in SSO
                std::memcpy(SSOBuffer + Length, Str, lStrLen);
                Length = lNewLength;
                SSOBuffer[Length] = OpaaxString_InvalidCharacter;
                return;
            }

            // Heap path: grow only if needed
            if (!bUsingHeap || lNewLength > Capacity)
            {
                GrowHeap(lNewLength);
            }

            std::memcpy(HeapData + Length, Str, lStrLen);
            Length = lNewLength;
            HeapData[Length] = OpaaxString_InvalidCharacter;
        }

        void Append(const OpaaxString& Other)
        {
            Append(Other.CStr());
        }

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

            GrowHeap(InCapacity);
        }

        std::string ToStdString() const
        {
            return std::string(CStr());
        }

        OpaaxString SubString(Uint32 Start, Uint32 InLength = UINT32_MAX) const
        {
            return SubString(*this, Start, InLength);
        }

        Int32 Find(const char* Str, Uint32 StartPos = 0) const
        {
            const char* lResult = std::strstr(CStr() + StartPos, Str);
            return lResult ? static_cast<Int32>(lResult - CStr()) : -1;
        }

        char ToUpperChar(char Char) const noexcept
        {
            return (Char >= 'a' && Char <= 'z') ? static_cast<char>(Char - 'a' + 'A') : Char;
        }

        char ToLowerChar(char Char) const noexcept
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
                OPAAX_CORE_ERROR("OpaaxString::IsValidIndex — Index {} out of bounds (Length={})", Index, Length);
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

        bool operator==(const OpaaxString& Other) const noexcept { return std::strcmp(CStr(), Other.CStr()) == 0; }
        bool operator==(const char* Str) const noexcept { return std::strcmp(CStr(), Str) == 0; }
        bool operator!=(const OpaaxString& Other) const noexcept { return !(*this == Other); }
        bool operator!=(const char* Str) const noexcept { return !(*this == Str); }

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

        friend std::ostream& operator<<(std::ostream& OS, const OpaaxString& Str)
        {
            return OS << Str.CStr();
        }

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
} // namespace Opaax

// Formatter for spdlog / fmtlib
#include <spdlog/fmt/fmt.h>

template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<Opaax::OpaaxString, T>::value, char>>
    : fmt::formatter<std::string>
{
    auto format(const T& String, format_context& CTX) const
    {
        return fmt::formatter<std::string>::format(std::string(String.CStr()), CTX);
    }
};
