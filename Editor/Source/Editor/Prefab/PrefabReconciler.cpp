#include "Editor/Prefab/PrefabReconciler.h"

#include "Editor/EditorContext.h"
#include "Editor/Operation/EditorSelection.hpp"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourceTypeID.hpp"

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

        // One resolver for this phase, built before the write, so it flattens the old prefab.
        ResourcePrefabResolver lResolver(m_Context.Paths, m_Context.Resources, lRegistry);

        // The placements this save affects: those of the prefab itself and of any prefab placing it, at
        // any depth. Other placements are left alone.
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

        // Captured and folded against the old data (still loaded): the records then hold only the
        // author's changes.
        m_Pending = MapSerializer::CaptureEntities(*lWorld, lRegistry, lAffected);

        // Taken before the fold (CaptureEntities leaves Id invalid, and Fold replaces the entities).
        // Expand needs it: BuildInstance refuses an invalid map.
        m_Pending.Id = m_Pending.OwnerId();

        // The guids too, for the orphan check in HandleSaved.
        for (const EntityData& lEntity : m_Pending.Entities) { m_Affected.emplace_back(lEntity.Id); }

        const Uint64 lFolded = PrefabFold::Fold(m_Pending, lResolver, lRegistry);

        if (lFolded == 0)
        {
            m_Pending = MapData{};   // nothing to fold: leave the world alone
            m_Affected.clear();
        }
    }

    void PrefabReconciler::HandleSaved(const ResourceSavedEvent& InEvent)
    {
        // Taken and cleared first, whatever happens below.
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
            // Stored in HandleSaving; without it Expand can build nothing.
            OPAAX_LOG(LogPrefabReconciler, Warn,
                      "Prefab '{}' saved, but its placements name no map — not re-applied",
                      InEvent.AssetPath.CStr());
            return;
        }

        // Expanded against the new data (already reloaded).
        ResourcePrefabResolver lResolver(m_Context.Paths, m_Context.Resources, lRegistry);
        const Uint64 lExpanded = PrefabFold::Expand(lPending, lResolver, lRegistry);

        // Restore, not Instantiate: the entities keep their identities. It also removes components the
        // new prefab no longer has.
        const Uint64 lUpdated = MapFactory::Restore(lPending, *lWorld, lRegistry);

        // Destroy instance entities whose template was removed from the prefab (Restore does not name them).
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

            // Out of the selection first.
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
