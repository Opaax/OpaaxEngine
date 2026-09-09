#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Prefab/PrefabData.h"
#include "World/Serialization/MapData.h"

namespace Opaax
{
    class ComponentRegistry;

    inline constexpr LogCategory LogPrefabFold{"PrefabFold"};

    // =============================================================================
    // IPrefabResolver — the ONE thing the fold cannot work out for itself.
    //
    //   Turning an asset-relative prefab path into its data needs `IPaths` (an app service) and the
    //   `ResourceManager` (an engine subsystem), neither of which the World layer reaches. Rather
    //   than drag both down here, the caller answers the question — and the callers that matter are
    //   exactly the two that already hold everything: the editor's document layer, and `Level`.
    //
    //   A resolver may return null, and BOTH directions handle it without losing data — see Fold.
    // =============================================================================
    class IPrefabResolver
    {
    public:
        virtual ~IPrefabResolver() = default;

        /** @return the prefab's entities, or NULL when the path names nothing loadable. */
        virtual const PrefabData* Resolve(const OpaaxString& InAssetPath) const = 0;
    };

    // =============================================================================
    // PrefabFold — entities <-> instance records (⑦-C **K3**).
    //
    //   THE ONLY TWO PLACES A MAP CHANGES SHAPE. Fold runs on the way OUT to a `.opaaxmap`, Expand
    //   on the way back IN, and nothing between them knows prefabs exist: `MapJson` reads and
    //   writes `MapData::Instances` as an ordinary field, and `MapSerializer`/`MapFactory` are
    //   untouched — which is why a PIE clone (**WM6**) and an undo record still carry their
    //   instance entities expanded, correctly, without a special case anywhere.
    //
    //   This placement is what kept P3 from threading a resolver through 114 call sites: the
    //   question "what is in this prefab?" is asked at the two moments that have an answer, not by
    //   every layer that happens to be holding a MapData.
    // =============================================================================
    class OPAAX_API PrefabFold
    {
    public:
        /**
         * Replace the instance ENTITIES in InOutData with instance RECORDS — the form a file stores.
         *
         * Entities are grouped by their marker's `InstanceId`, each is diffed against the template
         * carrying its `TemplateGuid`, and only the deviations are kept (**PrefabOverrides**).
         *
         * AN UNRESOLVABLE PREFAB LEAVES ITS ENTITIES EXPANDED, with a Warn, and that is the
         * important half: a map whose prefab file was renamed still saves everything it has, so the
         * author loses a link rather than their level. The alternative — dropping them — would be
         * silent data loss at the one moment the author is trying to preserve their work.
         *
         * An entity carrying a marker whose `TemplateGuid` is not in the prefab is also left
         * expanded: it is no longer part of that prefab, and inventing a template for it would
         * write an override against the wrong entity.
         *
         * @return How many records were produced. Idempotent: folding twice changes nothing,
         *   because the second pass finds no marked entities left.
         */
        static Uint64 Fold(MapData& InOutData, const IPrefabResolver& InResolver,
                           const ComponentRegistry& InRegistry);

        /**
         * The inverse: turn every record back into entities and clear the record list.
         *
         * Each record is rebuilt through `PrefabFactory::BuildInstance` — so the guids are DERIVED
         * exactly as they were when the instance was first placed (**K2**), which is what makes an
         * inter-entity reference survive a save/load round trip — and then each entity takes its
         * override patch.
         *
         * AN UNRESOLVABLE PREFAB LOSES THAT PLACEMENT, with an Error, and it cannot be otherwise:
         * the entities live only in the prefab file. This is the asymmetry with Fold, and it is the
         * reason Fold refuses to drop anything.
         *
         * @return How many placements were expanded.
         */
        static Uint64 Expand(MapData& InOutData, const IPrefabResolver& InResolver,
                             const ComponentRegistry& InRegistry);
    };
}
