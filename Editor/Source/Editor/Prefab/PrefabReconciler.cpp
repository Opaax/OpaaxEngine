#include "Editor/Prefab/PrefabReconciler.h"

#include "Editor/EditorContext.h"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"

#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Prefab/ResourcePrefabResolver.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor
{
    void PrefabReconciler::Bind(EditorResourceEvents& InEvents)
    {
        InEvents.OnResourceSaving.AddMember(this, &PrefabReconciler::HandleSaving);
        InEvents.OnResourceSaved.AddMember(this, &PrefabReconciler::HandleSaved);
    }

    void PrefabReconciler::Unbind(EditorResourceEvents& InEvents)
    {
        InEvents.OnResourceSaving.RemoveAll(this);
        InEvents.OnResourceSaved.RemoveAll(this);
    }

    void PrefabReconciler::HandleSaving(const ResourceSavedEvent& InEvent)
    {
        m_Pending = MapData{};

        if (InEvent.TypeId != ResourceTypeID::Get<PrefabResource>()) { return; }

        World* const lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr || InEvent.AssetPath.IsEmpty()) { return; }

        const ComponentRegistry& lRegistry = m_Context.Engine.GetRegistries().Components();

        // Only THIS prefab's entities. Every other placement is left alone — folding the whole
        // world would be correct but would rewrite entities nobody asked about.
        TDynArray<EntityID> lAffected;
        lWorld->Each<PrefabInstanceComponent>([&lAffected, &InEvent](EntityID InId,
                                                                    const PrefabInstanceComponent& InMarker)
        {
            if (InMarker.IsLinked() && InMarker.Prefab.Path == InEvent.AssetPath)
            {
                lAffected.emplace_back(InId);
            }
        });

        if (lAffected.empty()) { return; }

        // Captured and folded against the OLD payload, which is still resident — this is the whole
        // reason the announce has two phases. The records hold the author's deviations and nothing
        // else, because everything matching the old template diffs to nothing.
        m_Pending = MapSerializer::CaptureEntities(*lWorld, lRegistry, lAffected);

        // TAKEN NOW, BEFORE the fold. `CaptureEntities` leaves `Id` invalid — a hand-picked set is
        // not a map (**MP10**) — and it is recoverable only from the entities, which Fold is about
        // to replace with records. Expand needs it, because BuildInstance refuses an invalid map.
        m_Pending.Id = m_Pending.OwnerId();

        ResourcePrefabResolver lResolver(m_Context.Paths, m_Context.Resources);
        const Uint64 lFolded = PrefabFold::Fold(m_Pending, lResolver, lRegistry);

        if (lFolded == 0)
        {
            m_Pending = MapData{};   // nothing foldable — leave the world alone
        }
    }

    void PrefabReconciler::HandleSaved(const ResourceSavedEvent& InEvent)
    {
        if (m_Pending.Instances.empty())
        {
            m_Pending = MapData{};
            return;
        }

        World* const lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr)
        {
            m_Pending = MapData{};
            return;
        }

        const ComponentRegistry& lRegistry = m_Context.Engine.GetRegistries().Components();

        if (!m_Pending.Id.IsValid())
        {
            // Banked in HandleSaving; without it Expand can build nothing.
            OPAAX_LOG(LogPrefabReconciler, Warn,
                      "Prefab '{}' saved, but its placements name no map — not re-applied",
                      InEvent.AssetPath.CStr());
            m_Pending = MapData{};
            return;
        }

        // Expanded against the NEW payload — the reload has already happened.
        ResourcePrefabResolver lResolver(m_Context.Paths, m_Context.Resources);
        const Uint64 lExpanded = PrefabFold::Expand(m_Pending, lResolver, lRegistry);

        // Restore, not Instantiate: these entities still exist and must keep their identities. It
        // also REMOVES components the new prefab no longer has, which is what makes deleting a
        // component from a prefab reach its instances.
        const Uint64 lUpdated = MapFactory::Restore(m_Pending, *lWorld, lRegistry);

        lWorld->MarkChanged();

        OPAAX_LOG(LogPrefabReconciler, Info,
                  "Prefab '{}' saved — re-applied to {} placement(s), {} entity(ies) updated",
                  InEvent.AssetPath.CStr(), lExpanded, lUpdated);

        m_Pending = MapData{};
    }
}
