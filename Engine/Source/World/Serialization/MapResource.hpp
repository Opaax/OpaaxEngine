#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceConcept.hpp"
#include "World/Serialization/MapFile.h"

namespace Opaax
{
    // =============================================================================
    // MapResource — a `.opaaxmap` as a RESOURCE (**WM4**: a Map is a resource, and it is the
    //   serialization unit of the World > Level > Map model).
    //
    //   A plain struct satisfying CResource — no base class, no registration. Its whole body is
    //   an adapter: MapFile owns the format, this owns the resource contract, and going through
    //   the ResourceManager is what buys dedup (two worlds opening the same map parse it once)
    //   and a lifetime the caller does not hand-manage.
    //
    //   FAIL FAST, not Placeholder. ResourceConcept's own rule is the deciding question — does
    //   a degraded substitute keep gameplay CORRECT? An empty map does not degrade, it LIES:
    //   the level appears to load, the world comes up empty, and nothing anywhere reports a
    //   problem. That is the same class of silent-wrong-answer failure the codebase treats as
    //   its worst, so a missing map must resolve to null and be noticed.
    //
    //   NO LoadContext::Acquire — and that is the rule, not an omission (**WM4**). Acquire is
    //   for HARD dependencies: it loads them inline and chains their refcounts to the parent.
    //   A map has none today, and when a map gains texture references those ARE Acquire'd while
    //   a Level's maps still are not — acquiring a level's maps would load every one of them at
    //   once and make unloading a single map impossible, which is the exact opposite of
    //   streaming.
    //
    //   No Initialize(): nothing here touches the GPU, so the payload is complete the moment
    //   Load returns and the pool can publish it without a main-thread pass.
    // =============================================================================
    struct MapResource final
    {
        MapData Data;

        // ---- CResource contract --------------------------------------------------
        static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;

        static std::optional<MapResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            // InCtx unused BY DESIGN — see the WM4 note above. It stays in the signature
            // because the concept requires it, and because a map WILL acquire its textures here
            // once a component can name one.
            MapResource lResource;
            if (!MapFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // MapFile already logged which of the reasons it was
            }

            return lResource;
        }

        /**
         * Required by the concept, and never resolved to under FailFast — the pool answers null
         * for a failed FailFast handle rather than handing this out. It exists so the contract
         * is satisfied uniformly, not as a fallback anything is meant to receive.
         */
        static MapResource Placeholder() { return MapResource{}; }

        /**
         * Optional accounting hook the pool picks up with if-constexpr.
         *
         * STRUCTURAL SIZE ONLY: it counts the entity and component records, and deliberately
         * NOT the nlohmann::json payload trees hanging off them, because measuring those would
         * mean walking (or dumping) every payload on a call that exists for a memory readout.
         * Reporting a bounded under-count with the reason written down beats both a plausible
         * lie and an O(payload) query — and beats sizeof(MapResource), which is what the pool
         * would use if this were absent and which would report ~64 bytes for a 10k-entity map.
         */
        Uint64 ByteSize() const noexcept
        {
            Uint64 lBytes = sizeof(MapResource);

            for (const EntityData& lEntity : Data.Entities)
            {
                lBytes += sizeof(EntityData) + lEntity.Name.GetLength();
                lBytes += static_cast<Uint64>(lEntity.Components.size()) * sizeof(ComponentData);
            }

            return lBytes;
        }
    };
}
