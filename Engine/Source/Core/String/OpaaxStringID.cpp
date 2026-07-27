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
    // =========================================================================
    class OpaaxStringIDPool final
    {
    public:
        OpaaxStringIDPool()
        {
            m_Strings.reserve(256);
            m_Strings.push_back(OpaaxGlobal::String_None);
            m_Lookups[OpaaxGlobal::String_None] = OpaaxGlobal::ID_None;
        }

        Uint32 GetOrAdd(const OpaaxString& InString)
        {
            std::unique_lock lLock(m_Mutex);

            auto lIt = m_Lookups.find(InString);
            if (lIt != m_Lookups.end())
            {
                return lIt->second;
            }

            const Uint32 lID = static_cast<Uint32>(m_Strings.size());
            m_Strings.push_back(InString);
            m_Lookups[InString] = lID;
            return lID;
        }

        const OpaaxString& Get(Uint32 InIndex) const noexcept
        {
            std::shared_lock lLock(m_Mutex);
            return (InIndex < m_Strings.size()) ? m_Strings[InIndex] : m_Strings[OpaaxGlobal::ID_None];
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

    // =========================================================================
    // The single pool. This definition lives ONLY here, so every module — engine DLL, editor lib,
    // game exe, test exe — reaches the same table through the exported accessor. Function-local
    // static: thread-safe lazy init (C++11 magic statics), and it outlives every id that names into
    // it because it is destroyed at process exit.
    // =========================================================================
    OpaaxStringIDPool& OpaaxStringID::GetPool()
    {
        static OpaaxStringIDPool s_Pool;
        return s_Pool;
    }

    OpaaxStringID::OpaaxStringID(const OpaaxString& InString)
    {
        m_ID = InString.IsEmpty() ? OpaaxGlobal::ID_None : GetPool().GetOrAdd(InString);
    }

    OpaaxString OpaaxStringID::ToString() const
    {
        return GetPool().Get(m_ID);
    }

    Uint32 OpaaxStringID::PoolSize() noexcept
    {
        return GetPool().GetPoolSize();
    }
}
