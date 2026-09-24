#include "ResourceTypeID.hpp"

#include <cstdlib>
#include <string>

#include "Core/EngineAPI.h"
#include "Core/Hash/OpaaxHash.h"

namespace Opaax
{
    // Compile-time sanity for the 64-bit hash that backs ResourceTypeID.
    static_assert(OpaaxHash::Hash64("")   == OpaaxHash::FNV1a_OffsetBasis64, "FNV-1a64: empty == offset basis");
    static_assert(OpaaxHash::Hash64("ab") != OpaaxHash::Hash64("ba"),    "FNV-1a64: order-sensitive");
    static_assert(OpaaxHash::Hash64("x")  != OpaaxHash::FNV1a_OffsetBasis64, "FNV-1a64: non-empty mixes");

    namespace
    {
        // =====================================================================
        // The single resource-type registry. Lives in exactly one module (the
        // engine DLL, where this TU compiles); reached from every module through
        // the exported Intern/Count functions below.
        // =====================================================================
        struct TypeEntry
        {
            Uint64      Hash;
            std::string Name;   // full type signature — the collision-check key
        };

        struct TypeRegistry
        {
            TDynArray<TypeEntry>         Entries;      // index == dense ResourceTypeID
            TUnorderedMap<Uint64, Uint32> HashToIndex;
            Mutex                        Lock;         // lazy registration may come off worker threads (M2)
        };

        TypeRegistry& GetRegistry()
        {
            static TypeRegistry s_Registry;
            return s_Registry;
        }
    }

    Uint32 InternResourceType(Uint64 InHash, std::string_view InName)
    {
        TypeRegistry&    lReg = GetRegistry();
        TLockGuard<Mutex> lGuard(lReg.Lock);

        if (const auto lIt = lReg.HashToIndex.find(InHash); lIt != lReg.HashToIndex.end())
        {
            const TypeEntry& lExisting = lReg.Entries[lIt->second];
            if (lExisting.Name != InName)
            {
                // Two distinct type signatures collided on one 64-bit hash — they would
                // share a pool index, i.e. one pool reinterpreting the other's payload.
                // Fatal in ALL builds by design (review ruling C2): fail loud, never corrupt.
                OPAAX_LOG(LogResourceType, Critical, "ResourceTypeID hash collision (hash {}): '{}' vs '{}' — aborting", InHash, lExisting.Name, std::string(InName));
                OPAAX_DEBUGBREAK();
                std::abort();
            }
            return lIt->second;
        }

        const Uint32 lIndex = static_cast<Uint32>(lReg.Entries.size());
        lReg.Entries.emplace_back(InHash, std::string(InName));
        lReg.HashToIndex.emplace(InHash, lIndex);
        return lIndex;
    }

    Uint32 GetResourceTypeCount()
    {
        TypeRegistry&    lReg = GetRegistry();
        TLockGuard<Mutex> lGuard(lReg.Lock);
        return static_cast<Uint32>(lReg.Entries.size());
    }
}
