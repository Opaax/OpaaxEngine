#include "Editor/Prefab/PrefabReconciler.h"

#include "Editor/EditorContext.h"
#include "Editor/Operation/EditorSelection.hpp"

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
        m_Affected.clear();

        if (InEvent.TypeId != ResourceTypeID::Get<PrefabResource>()) { return; }

        World* const lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr || InEvent.AssetPath.IsEmpty()) { return; }

        const ComponentRegistry& lRegistry = m_Context.Engine.GetRegistries().Components();

        // ONE resolver for both halves of this phase — it is built BEFORE the write, so what it
        // flattens is the OLD prefab, which is the whole point of folding here (**PF8**).
        ResourcePrefabResolver lResolver(m_Context.Paths, m_Context.Resources, lRegistry);

        // The placements this save REACHES: those of the prefab itself, and those of any prefab
        // that places it, at any depth (P7) — an outer prefab or a variant whose base just changed.
        // Every other placement is left alone; folding the whole world would be correct but would
        // rewrite entities nobody asked about.
        TDynArray<EntityID> lAffected;
        lWorld->Each<PrefabInstanceComponent>([&](EntityID InId, const PrefabInstanceComponent& InMarker)
        {
            if (!InMarker.IsLinked()) { return; }

            if (InMarker.Prefab.Path == InEvent.AssetPath
                || lResolver.Places(InMarker.Prefab.Path, InEvent.AssetPath))
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

        // And the guids, for the same reason: Fold replaces the entities with records, and the
        // orphan check in HandleSaved needs to know what was there.
        for (const EntityData& lEntity : m_Pending.Entities) { m_Affected.emplace_back(lEntity.Id); }

        const Uint64 lFolded = PrefabFold::Fold(m_Pending, lResolver, lRegistry);

        if (lFolded == 0)
        {
            m_Pending = MapData{};   // nothing foldable — leave the world alone
            m_Affected.clear();
        }
    }

    void PrefabReconciler::HandleSaved(const ResourceSavedEvent& InEvent)
    {
        // Taken and cleared FIRST, whatever happens below — both are one save's parameters.
        MapData             lPending  = Move(m_Pending);
        const TDynArray<Guid> lAffected = Move(m_Affected);
        m_Pending = MapData{};
        m_Affected.clear();

        if (lPending.Instances.empty()) { return; }

        World* const lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return; }

        const ComponentRegistry& lRegistry = m_Context.Engine.GetRegistries().Components();

        if (!lPending.Id.IsValid())
        {
            // Banked in HandleSaving; without it Expand can build nothing.
            OPAAX_LOG(LogPrefabReconciler, Warn,
                      "Prefab '{}' saved, but its placements name no map — not re-applied",
                      InEvent.AssetPath.CStr());
            return;
        }

        // Expanded against the NEW payload — the reload has already happened.
        ResourcePrefabResolver lResolver(m_Context.Paths, m_Context.Resources, lRegistry);
        const Uint64 lExpanded = PrefabFold::Expand(lPending, lResolver, lRegistry);

        // Restore, not Instantiate: these entities still exist and must keep their identities. It
        // also REMOVES components the new prefab no longer has, which is what makes deleting a
        // component from a prefab reach its instances.
        const Uint64 lUpdated = MapFactory::Restore(lPending, *lWorld, lRegistry);

        // AND DESTROYS WHAT THE PREFAB NO LONGER HAS. Restore names what the new template
        // produced; an instance entity whose template was DELETED from the prefab is named by
        // nothing and would stay behind — the user's report, found in a minute of real use.
        Uint64 lRemoved = 0;

        for (const Guid& lWas : lAffected)
        {
            bool lStillNamed = false;
            for (const EntityData& lNow : lPending.Entities)
            {
                if (lNow.Id == lWas) { lStillNamed = true; break; }
            }
            if (lStillNamed) { continue; }

            Entity lOrphan = lWorld->FindByGuid(lWas);
            if (!lOrphan.IsValid()) { continue; }

            // Out of the selection first — a destroyed entity's handle must not linger there.
            if (m_Context.Selection.Contains(lOrphan)) { m_Context.Selection.Toggle(lOrphan); }

            lWorld->DestroyEntity(lOrphan.GetHandle());
            ++lRemoved;
        }

        lWorld->MarkChanged();

        OPAAX_LOG(LogPrefabReconciler, Info,
                  "Prefab '{}' saved — re-applied to {} placement(s), {} entity(ies) updated, {} removed",
                  InEvent.AssetPath.CStr(), lExpanded, lUpdated, lRemoved);
    }
}
