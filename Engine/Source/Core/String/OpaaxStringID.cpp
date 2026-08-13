#include "Core/String/OpaaxStringID.hpp"

#include "Core/Hash/OpaaxHash.h"

#include <unordered_map>
#include <shared_mutex>

namespace Opaax
{
    // =========================================================================
    // OpaaxStringIDPool — thread-safe intern table, OpaaxString <-> Uint32.
    //
    // Defined in the .cpp, not the header, so the engine DLL owns the ONLY definition. See the
    // OpaaxStringID class note (I2): a header-inline pool accessor lets each module emit its own
    // function-local static, and the same string then interns to different ids on either side of the
    // DLL/exe line. Index 0 is reserved for "None" — always valid, never removed.
    //
    // STORAGE — the map OWNS every text, the array only points at it. That is what makes a handed-out
    // `const OpaaxString&` safe to hold after the lock drops: unordered_map keeps pointers and
    // references to its elements valid across rehash, and nothing is ever erased here, so a pointer
    // stays good for the life of the process. m_Strings may still reallocate, but the reader copies
    // the POINTER out under the lock and the pointee never moves. Storing the text in a vector
    // instead — the previous shape — meant a concurrent GetOrAdd could reallocate under a reader that
    // had already released its shared lock and was about to copy from the old block. It also kept a
    // second copy of every interned string, once as a vector element and once as a map key.
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

        /*** Uint32 of an ALREADY-interned string, or ID_None. Never grows the table. */
        Uint32 FindExisting(const OpaaxString& InString) const
        {
            std::shared_lock lLock(m_Mutex);

            const auto lIt = m_Lookup.find(InString);
            return (lIt != m_Lookup.end()) ? lIt->second : OpaaxGlobal::ID_None;
        }

        /*** Entries are immortal and address-stable, so this reference outlives the lock. */
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
            // try_emplace, not find-then-insert: one hash lookup on the miss path instead of two.
            const auto [lIt, lInserted] = m_Lookup.try_emplace(InString, static_cast<Uint32>(m_Strings.size()));

            if (lInserted)
            {
                m_Strings.push_back(&lIt->first);
            }

            return lIt->second;
        }

        std::unordered_map<OpaaxString, Uint32, OpaaxHash> m_Lookup;   // owns the text
        TDynArray<const OpaaxString*>                      m_Strings;  // id -> text, into m_Lookup
        mutable std::shared_mutex                          m_Mutex;
    };

    // =========================================================================
    // The single pool. This definition lives ONLY here, so every module — engine DLL, editor lib,
    // game exe, test exe — reaches the same table through the exported accessor.
    //
    // IMMORTAL ON PURPOSE. The pointer is a function-local static (thread-safe lazy init via C++11
    // magic statics), but the pool it names is never deleted, so the table outlives every static,
    // every worker thread still draining at shutdown, and every id that resolves through it. A
    // function-local OBJECT would be destroyed at exit and any ToString() ordered after that — from a
    // later-destroyed static, say — would read a dead table. One deliberate, bounded allocation buys
    // the FName property that a name handle is valid for as long as the process is.
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
