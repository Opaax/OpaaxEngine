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
     * SSO of 15 + 1.
     */
    class OPAAX_API OpaaxString final
    {
        // =============================================================================
        // Statics
        // =============================================================================
    private:
        static constexpr Uint32 SSOCapacity = 15;

        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpaaxString() = default;
        ~OpaaxString() { Clear(); }
        
        OpaaxString(const char* Str) : SSOBuffer{0}, Length(0)
        {
            Uint32 lLength = static_cast<Uint32>(std::strlen(Str));
            Length = lLength;

            if (lLength <= SSOCapacity)
            {
                for (Uint32 i = 0; i < lLength; ++i)
                {
                    SSOBuffer[i] = Str[i];
                }
                SSOBuffer[lLength] = OpaaxString_InvalidCharacter;
            }
            else
            {
                AllocateAndCopyHeap(Str, Length);
            }
        }

        OpaaxString(const OpaaxString& Other)
        {
            Length      = Other.Length;
            bUsingHeap  = Other.bUsingHeap;

            if (bUsingHeap)
            {
                AllocateAndCopyHeap(Other.HeapData, Length);
            }
            else
            {
                std::memcpy(SSOBuffer, Other.SSOBuffer, Length + 1);
            }
        }

        OpaaxString& operator=(const OpaaxString& Other)
        {
            if (this != &Other)
            {
                Clear();
                
                Length      = Other.Length;
                bUsingHeap  = Other.bUsingHeap;

                if (bUsingHeap)
                {
                    AllocateAndCopyHeap(Other.HeapData, Length);
                }
                else
                {
                    std::memcpy(SSOBuffer, Other.SSOBuffer, Length + 1);
                }
            }
            return *this;
        }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void AllocateAndCopyHeap(const char* Str, Uint32 InLength)
        {
            HeapData = new char[InLength + 1];
            std::memcpy(HeapData, Str, InLength);
            HeapData[InLength] = '\0';
            bUsingHeap = true;
        }

        constexpr Uint32 ConstexprStrlen(const char* Str)
        {
            Uint32 lLenght = 0;
            
            while (Str[lLenght] != '\0')
            {
                ++lLenght;
            }
            
            return lLenght;
        }
        
    public:
        void Clear()
        {
            if (bUsingHeap)
            {
                delete[] HeapData;
                bUsingHeap = false;
            }

            SSOBuffer[0] = '\0';
        }

        const char* CStr() const
        {
            return bUsingHeap ? HeapData : SSOBuffer;
        }

        char* Data()
        {
            return bUsingHeap ? HeapData : SSOBuffer;
        }

        
        Uint32 GetMemoryUsage() const
        {
            Uint32 lCharSize = sizeof(char);
            return bUsingHeap ? Length * lCharSize + lCharSize : sizeof(SSOBuffer);
        }

        void Append(const char* Str)
        {
            
            if (!Str || Str[0] == '\0') 
            {
                return; // No operation if the input is null or empty
            }

            Uint32 lStrLen = static_cast<Uint32>(std::strlen(Str));
            Uint32 lNewLength = Length + lStrLen;

            if (lNewLength <= SSOCapacity)
            {
                // Fits in SSO buffer
                std::memcpy(SSOBuffer + Length, Str, lStrLen);
                Length = lNewLength;
                SSOBuffer[Length] = OpaaxString_InvalidCharacter;
            }
            else
            {
                // Needs to use heap memory or expand existing heap
                if (!bUsingHeap)
                {
                    // Transition from SSO to heap
                    char* lNewHeap = new char[lNewLength + 1];
                    std::memcpy(lNewHeap, SSOBuffer, Length);
                    std::memcpy(lNewHeap + Length, Str, lStrLen);
                    lNewHeap[lNewLength] = '\0';

                    HeapData = lNewHeap;
                    bUsingHeap = true;
                }
                else
                {
                    // Expand existing heap
                    char* NewHeap = new char[lNewLength + 1];
                    std::memcpy(NewHeap, HeapData, Length);
                    std::memcpy(NewHeap + Length, Str, lStrLen);
                    NewHeap[lNewLength] = '\0';

                    delete[] HeapData;
                    HeapData = NewHeap;
                }

                Length = lNewLength;
            }
        }

        void Append(const OpaaxString& Other)
        {
            Append(Other.CStr());
        }

        Uint32 GetLength() const { return Length; }
        bool IsUsingHeap() const { return bUsingHeap; }
        bool IsEmpty() const { return Length == 0; }
        bool IsValidIndex(Uint32 Index) const
        {
            if (Index >= GetLength())
            {
                OPAAX_CORE_ERROR("OpaaxString::IsValidIndex, Index is out of bounds!");
                return false;
            }
            
            return true;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        union
        {
            char SSOBuffer[SSOCapacity + 1]{0}; // Inline buffer for SSO
            char* HeapData; // Pointer for dynamically allocated strings
        };
    
        Uint32 Length = 0;
        bool bUsingHeap = false;

    public:
        char operator[](Uint32 Index) const
        {
            return IsValidIndex(Index) ? CStr()[Index] : OpaaxString_InvalidCharacter;
        }

        char operator[](Int32 Index) const
        {
            Uint32 lIndex = static_cast<Uint32>(Index);
            return IsValidIndex(lIndex) ? CStr()[lIndex] : OpaaxString_InvalidCharacter;
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

        bool operator==(const OpaaxString& Other) const
        {
            return std::strcmp(CStr(), Other.CStr()) == 0;
        }
        
        bool operator==(const char* Str) const
        {
            return std::strcmp(CStr(), Str) == 0;
        }
        
        bool operator!=(const OpaaxString& Other) const
        {
            return !(*this == Other);
        }
        
        inline OpaaxString operator+(const char* RHS) const
        {
            OpaaxString lResult(*this);
            lResult += RHS;
            return lResult;
        }
        
        inline OpaaxString operator+(const OpaaxString& RHS) const
        {
            OpaaxString lResult(*this);
            lResult += RHS;
            return lResult;
        }

        std::string ToStdString() const
        {
            return std::string(CStr());
        }

        static OpaaxString SubString(const OpaaxString& InOpaaxString, Uint32 Start, Uint32 InLength = UINT32_MAX)
        {
            
            if (Start >= InOpaaxString.GetLength())
            {
                return OpaaxString();
            }

            Uint32 lActualLength = (InLength == UINT32_MAX) ? (InOpaaxString.GetLength() - Start) : InLength;

            if (Start + lActualLength > InOpaaxString.GetLength())
            {
                lActualLength = InOpaaxString.GetLength() - Start;
            }

            char* lTemp = new char[lActualLength + 1];
            std::memcpy(lTemp, InOpaaxString.CStr() + Start, lActualLength);
            lTemp[lActualLength] = '\0';

            OpaaxString lResult(lTemp);
            delete[] lTemp;
            return lResult;
        }
        
        OpaaxString SubString(Uint32 Start, Uint32 InLength = UINT32_MAX) const
        {
            Uint32 lStringLength = Length;
            
            if (Start >= lStringLength)
            {
                return OpaaxString();
            }
            
            Uint32 lActualLength = (InLength == UINT32_MAX) ? (lStringLength - Start) : InLength;
            
            if (Start + lActualLength > lStringLength)
            {
                lActualLength = lStringLength - Start;
            }
            
            char* lTemp = new char[lActualLength + 1];
            std::memcpy(lTemp, CStr() + Start, lActualLength);
            lTemp[lActualLength] = '\0';
            
            OpaaxString lResult(lTemp);
            delete[] lTemp;
            return lResult;
        }
        
        Int32 Find(const char* Str, Uint32 StartPos = 0) const
        {
            const char* lResult = std::strstr(CStr() + StartPos, Str);
            return lResult ? static_cast<Int32>(lResult - CStr()) : -1;
        }

        char ToUpperChar(char Char) const
        {
            return (Char >= 'a' && Char <= 'z') ? Char - 'a' + 'A' : Char;
        }

        char ToLowerChar(char Char) const
        {
            return (Char >= 'A' && Char <= 'Z') ? Char - 'A' + 'a' : Char;
        }
        
        OpaaxString ToUpper() const
        {
            OpaaxString lResult(*this);
            
            char* lData = lResult.Data();
            
            for (size_t i = 0; i < lResult.GetLength(); ++i)
            {
                if (lData[i] >= 'a' && lData[i] <= 'z')
                {
                    lData[i] = ToUpperChar(lData[i]);
                }
            }
            
            return lResult;
        }
        
        OpaaxString ToLower() const
        {
            OpaaxString lResult(*this);
            
            char* lData = lResult.Data();
            
            for (size_t i = 0; i < lResult.GetLength(); ++i)
            {
                if (lData[i] >= 'A' && lData[i] <= 'Z')
                {
                    lData[i] = ToLowerChar(lData[i]);
                }
            }
            
            return lResult;
        }

        friend std::ostream& operator<<(std::ostream& OS, const OpaaxString& Str)
        {
            return OS << Str.CStr();
        }
    };
}

// Formatter for spdlog
#include <spdlog/fmt/fmt.h>

template<typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<Opaax::OpaaxString, T>::value, char>>
    : fmt::formatter<std::string>
{
    auto format(const T& String, format_context& CTX) const
    {
        return fmt::formatter<std::string>::format(std::string(String.CStr()), CTX);
    }
};