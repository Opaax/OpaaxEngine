#pragma once

#include "OpaaxTypes.h"
#include "EngineAPI.h"
#include "OpaaxGlobal.h"
#include "OpaaxHash.h"
#include "OpaaxString.hpp"

namespace Opaax
{
    /*
    * @struct StringId
    * 
    * Hash string for fast comparisons and lookups.
    */
    struct OPAAX_API OpaaxStringID final
    {
        //  thread-safe Pool global
        struct OpaaxStringIDPool
        {
            TDynArray<OpaaxString> Strings;// Index -> String
            std::unordered_map<OpaaxString, Uint32, OpaaxHash> Lookups;// String -> Index
            std::mutex Mutex;
        
            OpaaxStringIDPool()
            {
                // Index 0 = "None"
                Strings.push_back(OpaaxGlobal::String_None);
                Lookups[OpaaxGlobal::String_None] = OpaaxGlobal::ID_None;
            }
        
            Uint32 GetOrAdd(const OpaaxString& String)
            {
                std::scoped_lock lLock(Mutex);
            
                auto it = Lookups.find(String);
                if (it != Lookups.end())
                {
                    return it->second;
                }
            
                Uint32 lID = static_cast<Uint32>(Strings.size());
                
                Strings.push_back(String);
                Lookups[String] = lID;
                
                return lID;
            }
        
            const OpaaxString& Get(Uint32 Index) const
            {
                return (Index < Strings.size()) ? Strings[Index] : Strings[OpaaxGlobal::ID_None];
            }
        
            Uint32 GetPoolSize() const
            {
                return static_cast<Uint32>(Strings.size());
            }
        };
        
        // =============================================================================
        // Statics
        // =============================================================================
    private:
        static OpaaxStringIDPool& GetPool()
        {
            static OpaaxStringIDPool lPool;
            return lPool;
        }
        
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpaaxStringID(const OpaaxString& String)
        {
            m_ID = String.IsEmpty() ? OpaaxGlobal::ID_None : GetPool().GetOrAdd(String);
        }
        
        OpaaxStringID(const char* String) : OpaaxStringID(OpaaxString(String)) {}
        OpaaxStringID(const std::string& String) : OpaaxStringID(OpaaxString(String.c_str())) {}
    
        // Copy & Move
        OpaaxStringID(const OpaaxStringID& Other) = default;
        OpaaxStringID(OpaaxStringID&& Other) noexcept = default;

        // =============================================================================
        // FUNCTIONS
        // =============================================================================
    public:
        //--------------------------------- GET - SET ---------------------------------//
        FORCEINLINE Uint32 GetId() const { return m_ID; }
        FORCEINLINE bool IsValid() const { return m_ID != OpaaxGlobal::ID_None; }

        OpaaxString ToString()
        {
            return GetPool().Get(m_ID);
        }

        const OpaaxString& ToString() const
        {
            return GetPool().Get(m_ID);
        }

        // =============================================================================
        // MEMBERS
        // =============================================================================
    private:
        Uint32 m_ID;

        // =============================================================================
        // OPERATORS
        // =============================================================================
    public:
        OpaaxStringID& operator=(const OpaaxStringID& Other) = default;
        OpaaxStringID& operator=(OpaaxStringID&& Other) noexcept = default;
        
        constexpr bool operator==(const OpaaxStringID& Other) const { return m_ID == Other.m_ID; }
        constexpr bool operator!=(const OpaaxStringID& Other) const { return m_ID != Other.m_ID; }
    };
    
#define OPAAX_ID(STR) ::Opaax::OpaaxStringID(STR)
}

template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_base_of<Opaax::OpaaxStringID, T>::value, char>>
    : fmt::formatter<std::string>
{
    auto format(const T& StringID, format_context& CTX) const
    {
        return fmt::formatter<std::string>::format(std::string(StringID.ToString().CStr()), CTX);
    }
};
