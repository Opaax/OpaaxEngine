#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "World/Prefab/PrefabFile.h"

namespace Opaax
{
    // =============================================================================
    // PrefabResource — a `.opaaxprefab` as a RESOURCE.
    //
    //   MapResource's shape (**WM4**), and for its reasons: a plain struct satisfying CResource,
    //   no base class and no registration, whose whole body is an adapter. PrefabFile owns the
    //   format, this owns the resource contract, and going through the ResourceManager is what
    //   buys dedup — a level placing forty instances of one prefab parses the file ONCE — plus a
    //   lifetime no caller hand-manages.
    //
    //   FAIL FAST, not Placeholder, and the deciding question is ResourceConcept's own: does a
    //   degraded substitute keep gameplay CORRECT? An empty prefab does not degrade, it LIES —
    //   the instantiate reports success, nothing appears, and no layer says why. That is the
    //   silent-wrong-answer class this codebase treats as its worst.
    //
    //   NO LoadContext::Acquire, and since P5b that is a RULE for a prefab too (**PF11**): a hard
    //   reference is held by the MAP that mounted the entity, not by this payload — the resolver
    //   releases the prefab resource the moment an instance is instantiated, so a chain hung off
    //   it would not outlive the gun it was meant to serve.
    //
    //   THE DATA IS RAW (P7): its own entities and its placement RECORDS, as the file holds them.
    //   Consumers never read it directly — IPrefabResolver hands back the FLATTENED prefab.
    //
    //   No Initialize(): nothing here touches the GPU, so the payload is complete the moment Load
    //   returns and the pool can publish it without a main-thread pass.
    // =============================================================================
    struct PrefabResource final
    {
        PrefabData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Opaax Prefab", PrefabFile::PREFAB_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;

        static std::optional<PrefabResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            // InCtx unused for now — see the Acquire note above; P5 is where it stops being.
            PrefabResource lResource;
            if (!PrefabFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // PrefabFile already logged which of the reasons it was
            }

            return lResource;
        }

        /**
         * Required by the concept, and never resolved to under FailFast — the pool answers null
         * for a failed FailFast handle rather than handing this out.
         */
        static PrefabResource Placeholder() { return PrefabResource{}; }

        /**
         * Optional accounting hook the pool picks up with if-constexpr.
         *
         * STRUCTURAL SIZE ONLY, for MapResource::ByteSize's reason: it counts the entity and
         * component records and deliberately NOT the nlohmann::json payload trees hanging off
         * them, because measuring those would mean walking every payload on a call that exists
         * for a memory readout.
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
