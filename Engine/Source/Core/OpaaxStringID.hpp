#pragma once

#include "OpaaxTypes.h"
#include "EngineAPI.h"
#include "OpaaxGlobal.h"
#include "OpaaxHash.h"
#include "OpaaxString.hpp"

#include <unordered_map>
#include <mutex>
#include <shared_mutex>

namespace Opaax
{
    /**
     * @class OpaaxStringIDPool
     *
     * Thread-safe intern table. Maps OpaaxString <-> Uint32 index.
     * Index 0 is reserved for "None".
     */
    class OpaaxStringIDPool final
    {
    public:
        OpaaxStringIDPool()
        {
            // Index 0 = "None" — always valid, never removed
            m_Strings.reserve(256);
            m_Strings.push_back(OpaaxGlobal::String_None);
            m_Lookups[OpaaxGlobal::String_None] = OpaaxGlobal::ID_None;
        }

        Uint32 GetOrAdd(const OpaaxString& String)
        {
            std::unique_lock lLock(m_Mutex);

            auto lIt = m_Lookups.find(String);
            if (lIt != m_Lookups.end())
            {
                return lIt->second;
            }

            const Uint32 lID = static_cast<Uint32>(m_Strings.size());
            m_Strings.push_back(String);
            m_Lookups[String] = lID;
            return lID;
        }

        const OpaaxString& Get(Uint32 Index) const noexcept
        {
            std::shared_lock lLock(m_Mutex);
            return (Index < m_Strings.size()) ? m_Strings[Index] : m_Strings[OpaaxGlobal::ID_None];
        }

        Uint32 GetPoolSize() const noexcept
        {
            std::shared_lock lLock(m_Mutex);
            return static_cast<Uint32>(m_Strings.size());
        }

    private:
        TDynArray<OpaaxString>                              m_Strings;
        std::unordered_map<OpaaxString, Uint32, OpaaxHash>  m_Lookups;
        mutable std::shared_mutex                           m_Mutex;
    };

} // namespace Opaax::Internal

namespace Opaax
{
    /**
     * @struct OpaaxStringID
     *
     * Interned string handle. Comparison is always O(1) integer compare.
     * Construction from a string is O(1) amortized (hash lookup + possible insert).
     *
     * Use OPAAX_ID("MyString") to construct at callsites.
     */
    struct OPAAX_API OpaaxStringID final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpaaxStringID() noexcept : m_ID(OpaaxGlobal::ID_None) {}

        explicit OpaaxStringID(const OpaaxString& String)
        {
            m_ID = String.IsEmpty() ? OpaaxGlobal::ID_None : GetPool().GetOrAdd(String);
        }

        OpaaxStringID(const char*        String) : OpaaxStringID(OpaaxString(String)) {}
        OpaaxStringID(const std::string& String) : OpaaxStringID(OpaaxString(String.c_str())) {}

        OpaaxStringID(const OpaaxStringID&)            = default;
        OpaaxStringID(OpaaxStringID&&) noexcept        = default;
        OpaaxStringID& operator=(const OpaaxStringID&) = default;
        OpaaxStringID& operator=(OpaaxStringID&&) noexcept = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        OpaaxString ToString() const { return GetPool().Get(m_ID); }
        
        // ----------------------------------------------------------------------------
        // Get - Set
    public:
        FORCEINLINE Uint32 GetId()   const noexcept { return m_ID; }
        FORCEINLINE bool   IsValid() const noexcept { return m_ID != OpaaxGlobal::ID_None; }


        // =============================================================================
        // Operators
        // =============================================================================
    public:
        constexpr bool operator==(const OpaaxStringID& Other) const noexcept { return m_ID == Other.m_ID; }
        constexpr bool operator!=(const OpaaxStringID& Other) const noexcept { return m_ID != Other.m_ID; }
        
        // This does NOT intern the string — it is a read-only lookup.
        bool operator==(const OpaaxString& Other) const
        {
            return ToString() == Other;
        }

        bool operator!=(const OpaaxString& Other) const { return !(*this == Other); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_ID;

        // =============================================================================
        // Pool accessor — single static instance, lazy-initialized, thread-safe (C++11)
        // =============================================================================
        static OpaaxStringIDPool& GetPool()
        {
            static OpaaxStringIDPool s_Pool;
            return s_Pool;
        }
    };

    // Convenience macro — keeps callsite noise down
    #define OPAAX_ID(STR) ::Opaax::OpaaxStringID(STR)

} // namespace Opaax

// fmtlib / spdlog formatter
#include <spdlog/fmt/fmt.h>

template<typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_same_v<T, Opaax::OpaaxStringID>, char>>
    : fmt::formatter<std::string>
{
    auto format(const T& StringID, format_context& CTX) const
    {
        return fmt::formatter<std::string>::format(std::string(StringID.ToString().CStr()), CTX);
    }
};