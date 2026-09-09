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
    //   NO LoadContext::Acquire YET, and this is the one that is an OMISSION rather than a rule.
    //   MapResource cannot acquire because Acquire takes an ABSOLUTE path and asset-relative ->
    //   absolute lives in IPaths, which the Resources layer does not reach. A prefab has the same
    //   blocker AND a harder requirement than a map: ⑦-C **K5**'s HARD reference (a gun naming
    //   its bullet prefab) must be resident before the gun runs, which a resolve-on-first-touch
    //   cache cannot promise. That is ⑦-C P5, and it is what finally forces LoadContext to carry
    //   a path resolver.
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

            return lBytes;
        }
    };
}
