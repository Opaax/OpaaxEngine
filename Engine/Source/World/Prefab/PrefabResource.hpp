#pragma once

#include <optional>

#include "Resources/ResourceFormat.h"
#include "World/Prefab/PrefabFile.h"

namespace Opaax
{
    // =============================================================================
    // PrefabResource — a .opaaxprefab as a resource (loaded once, shared by every placement).
    //   FailFast: an empty prefab would silently place nothing.
    //   Hard references are held by the map, not here.
    //   Holds the raw data (own entities + records); consumers use IPrefabResolver (flattened).
    // =============================================================================
    struct PrefabResource final
    {
        PrefabData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Opaax Prefab", PrefabFile::PREFAB_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;

        static std::optional<PrefabResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            PrefabResource lResource;
            if (!PrefabFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // PrefabFile already logged why
            }

            return lResource;
        }

        /**
         * Required by the concept; never used with FailFast.
         */
        static PrefabResource Placeholder() { return PrefabResource{}; }

        /**
         * Size of the entity and component records (not the JSON payloads).
         */
        Uint64 ByteSize() const noexcept
        {
            Uint64 lBytes = sizeof(PrefabResource);

            for (const EntityData& lEntity : Data.Entities)
            {
                lBytes += sizeof(EntityData) + lEntity.Name.GetLength();
                lBytes += static_cast<Uint64>(lEntity.Components.size()) * sizeof(ComponentData);
            }

            for (const PrefabInstanceRecord& lRecord : Data.Instances)
            {
                lBytes += sizeof(PrefabInstanceRecord) + lRecord.Prefab.GetLength();
                lBytes += static_cast<Uint64>(lRecord.Overrides.size()) * sizeof(PrefabOverrideEntry);
            }

            return lBytes;
        }
    };
}
