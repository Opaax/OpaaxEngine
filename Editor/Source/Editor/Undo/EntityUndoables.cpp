#include "Editor/Undo/EntityUndoables.h"

#include "Editor/EditorContext.h"
#include "Editor/Operation/EditorSelection.hpp"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/MapFactory.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor
{
    namespace
    {
        // Put InData's entities back and select them — recreating any that are gone, on their own
        // Guids. MapFactory::Restore answers exactly this question ("be this again"), so create and
        // delete share one body run in opposite directions.
        void RestoreEntities(EditorContext& InContext, const MapData& InData)
        {
            World* const lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr) { return; }

            MapFactory::Restore(InData, *lWorld, InContext.Engine.GetRegistries().Components());

            // AFTER the restore: a recreated entity comes back on a new handle, and the selection
            // stores handles.
            TDynArray<EntityID> lIds;
            lIds.reserve(InData.Entities.size());

            for (const EntityData& lEntity : InData.Entities)
            {
                if (Entity lFound = lWorld->FindByGuid(lEntity.Id); lFound.IsValid())
                {
                    lIds.emplace_back(lFound.GetHandle());
                }
            }

            InContext.Selection.Replace(lWorld, lIds);
        }

        // The inverse, and it clears the selection for the reason DestroySelected does: what was
        // selected no longer exists.
        void DestroyEntities(EditorContext& InContext, const MapData& InData)
        {
            World* const lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr) { return; }

            InContext.Selection.Clear();

            for (const EntityData& lEntity : InData.Entities)
            {
                if (Entity lFound = lWorld->FindByGuid(lEntity.Id); lFound.IsValid())
                {
                    lWorld->DestroyEntity(lFound.GetHandle());
                }
            }

            lWorld->MarkChanged();
        }

        // Write one entity's name. Nothing observes EntityMeta, so the revision is bumped by hand —
        // the same reason EntityOps::Rename does it.
        void WriteName(EditorContext& InContext, const Guid& InId, const OpaaxString& InName)
        {
            World* const lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr) { return; }

            Entity lEntity = lWorld->FindByGuid(InId);
            if (!lEntity.IsValid()) { return; }

            lEntity.Get<EntityMeta>().Name = InName;
            lWorld->MarkChanged();
        }

        bool Same(const TransformComponent& InLeft, const TransformComponent& InRight)
        {
            return InLeft.Position == InRight.Position
                && InLeft.Rotation == InRight.Rotation
                && InLeft.Scale    == InRight.Scale;
        }

        // Put one side of a drag back. Nothing else has to happen: a drag never changed the
        // selection, so there is none to restore.
        void WriteTransforms(EditorContext& InContext, const TDynArray<EntityTransform::Entry>& InEntries,
                             const bool bInBefore)
        {
            World* const lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr) { return; }

            for (const EntityTransform::Entry& lEntry : InEntries)
            {
                Entity lEntity = lWorld->FindByGuid(lEntry.Id);
                if (!lEntity.IsValid()) { continue; }

                TransformComponent* const lTransform = lEntity.TryGet<TransformComponent>();
                if (lTransform == nullptr) { continue; }

                *lTransform = bInBefore ? lEntry.Before : lEntry.After;
            }

            // A component written in place is invisible to the world (**MP5**).
            lWorld->MarkChanged();
        }
    }

    void EntityCreate::Undo(EditorContext& InContext) { DestroyEntities(InContext, Entities); }
    void EntityCreate::Redo(EditorContext& InContext) { RestoreEntities(InContext, Entities); }

    void PrefabInstantiate::Undo(EditorContext& InContext) { DestroyEntities(InContext, Entities); }
    void PrefabInstantiate::Redo(EditorContext& InContext) { RestoreEntities(InContext, Entities); }

    void EntityDelete::Undo(EditorContext& InContext) { RestoreEntities(InContext, Entities); }
    void EntityDelete::Redo(EditorContext& InContext) { DestroyEntities(InContext, Entities); }

    void EntityRename::Undo(EditorContext& InContext) { WriteName(InContext, EntityId, Before); }
    void EntityRename::Redo(EditorContext& InContext) { WriteName(InContext, EntityId, After); }

    void EntityTransform::Begin(const EditorContext& InContext, const char* InName)
    {
        Entries.clear();
        Name = InName != nullptr ? InName : "Transform";

        World* const lWorld = InContext.Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return; }

        // BY GUID, not by handle: a step outlives the drag, and a handle does not survive an undo
        // that recreated the entity.
        for (const EntityID lId : InContext.Selection.Ids())
        {
            Entity lEntity{ lId, lWorld };

            const TransformComponent* const lTransform = lEntity.TryGet<TransformComponent>();
            if (lTransform == nullptr) { continue; }

            Entries.emplace_back(Entry{ lEntity.GetGuid(), *lTransform, *lTransform });
        }
    }

    bool EntityTransform::End(const EditorContext& InContext)
    {
        World* const lWorld = InContext.Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return false; }

        bool lMoved = false;

        for (Entry& lEntry : Entries)
        {
            Entity lEntity = lWorld->FindByGuid(lEntry.Id);
            if (!lEntity.IsValid()) { continue; }

            const TransformComponent* const lTransform = lEntity.TryGet<TransformComponent>();
            if (lTransform == nullptr) { continue; }

            lEntry.After = *lTransform;

            if (!Same(lEntry.Before, lEntry.After)) { lMoved = true; }
        }

        return lMoved;
    }

    void EntityTransform::Undo(EditorContext& InContext) { WriteTransforms(InContext, Entries, true); }
    void EntityTransform::Redo(EditorContext& InContext) { WriteTransforms(InContext, Entries, false); }
}
