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
        // The single resource type registry, in the engine DLL, reached through the exported functions.
        // =====================================================================
        struct TypeEntry
        {
            Uint64      Hash;
            std::string Name;   // full type signature
        };

        struct TypeRegistry
        {
            TDynArray<TypeEntry>         Entries;      // index == dense ResourceTypeID
            TUnorderedMap<Uint64, Uint32> HashToIndex;
            Mutex                        Lock;         // registration may happen on worker threads
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
                // Two type signatures with the same hash would share a pool: fatal in every build.
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
