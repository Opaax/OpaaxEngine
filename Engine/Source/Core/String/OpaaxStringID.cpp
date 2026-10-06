#include "Core/String/OpaaxStringID.hpp"

#include "Core/Hash/OpaaxHash.h"

#include <unordered_map>
#include <shared_mutex>

namespace Opaax
{
    // =========================================================================
    // OpaaxStringIDPool — thread-safe intern table, OpaaxString <-> Uint32.
    //   Defined in the .cpp so there is only one instance.
    //   Index 0 is "None". Entries are never removed and never move, so returned
    //   references stay valid after the lock is released.
    // =========================================================================
    class OpaaxStringIDPool final
    {
    public:
        OpaaxStringIDPool()
        {
            m_Strings.reserve(256);
            Insert(OpaaxString(OpaaxGlobal::String_None));
        }

        Uint32 GetOrAdd(const OpaaxString& InString)
        {
            std::unique_lock lLock(m_Mutex);
            return Insert(InString);
        }

        /** Id of an already-interned string, or ID_None. Never adds. */
        Uint32 FindExisting(const OpaaxString& InString) const
        {
            std::shared_lock lLock(m_Mutex);

            const auto lIt = m_Lookup.find(InString);
            return (lIt != m_Lookup.end()) ? lIt->second : OpaaxGlobal::ID_None;
        }

        /** Entries never move, so the reference stays valid after the lock. */
        const OpaaxString& Get(Uint32 InIndex) const
        {
            const OpaaxString* lText = nullptr;
            {
                std::shared_lock lLock(m_Mutex);
                lText = (InIndex < m_Strings.size()) ? m_Strings[InIndex] : m_Strings[OpaaxGlobal::ID_None];
            }
            return *lText;
        }

        Uint32 GetPoolSize() const
        {
            std::shared_lock lLock(m_Mutex);
            return static_cast<Uint32>(m_Strings.size());
        }

    private:
        // Caller holds the write lock.
        Uint32 Insert(const OpaaxString& InString)
        {
            // try_emplace: one lookup on a miss.
            const auto [lIt, lInserted] = m_Lookup.try_emplace(InString, static_cast<Uint32>(m_Strings.size()));

            if (lInserted)
            {
                m_Strings.emplace_back(&lIt->first);
            }

            return lIt->second;
        }

        std::unordered_map<OpaaxString, Uint32, OpaaxHash> m_Lookup;   // owns the text
        TDynArray<const OpaaxString*>                      m_Strings;  // id -> text
        mutable std::shared_mutex                          m_Mutex;
    };

    // =========================================================================
    // The single pool, shared by every module through the exported accessor.
    // Never deleted, so ids stay valid during static destruction and shutdown.
    // =========================================================================
    OpaaxStringIDPool& OpaaxStringID::GetPool()
    {
        static OpaaxStringIDPool* s_Pool = new OpaaxStringIDPool();
        return *s_Pool;
    }

    OpaaxStringID::OpaaxStringID(const OpaaxString& InString)
    {
        m_ID = InString.IsEmpty() ? OpaaxGlobal::ID_None : GetPool().GetOrAdd(InString);
    }

    OpaaxStringID OpaaxStringID::Find(const OpaaxString& InString)
    {
        return OpaaxStringID(InString.IsEmpty() ? OpaaxGlobal::ID_None : GetPool().FindExisting(InString));
    }

    const char* OpaaxStringID::CStr() const
    {
        return GetPool().Get(m_ID).CStr();
    }

    OpaaxStringView OpaaxStringID::GetView() const
    {
        const OpaaxString& lText = GetPool().Get(m_ID);
        return OpaaxStringView(lText.CStr(), lText.GetLength());
    }

    OpaaxString OpaaxStringID::ToString() const
    {
        return GetPool().Get(m_ID);
    }

    Uint32 OpaaxStringID::PoolSize()
    {
        return GetPool().GetPoolSize();
    }

    bool OpaaxStringID::operator==(const OpaaxString& Other) const
    {
        return GetPool().Get(m_ID) == Other;
    }
}
