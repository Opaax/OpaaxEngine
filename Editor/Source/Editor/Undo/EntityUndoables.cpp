#include "Editor/Undo/EntityUndoables.h"

#include "Editor/EditorContext.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Prefab/EditorPrefabDocument.h"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/MapFactory.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor
{
    World* UndoWorld(const EditorContext& InContext, const EUndoWorld InScope)
    {
        return InScope == EUndoWorld::Prefab ? InContext.PrefabDocument.GetWorld()
                                             : InContext.Worlds.GetActiveWorld();
    }

    EditorSelection& UndoSelection(const EditorContext& InContext, const EUndoWorld InScope)
    {
        return InScope == EUndoWorld::Prefab ? InContext.PrefabDocument.Selection()
                                             : InContext.Selection;
    }

    namespace
    {
        // Put InData's entities back and select them — recreating any that are gone, on their own
        // Guids. MapFactory::Restore answers exactly this question ("be this again"), so create and
        // delete share one body run in opposite directions.
        void RestoreEntities(EditorContext& InContext, const MapData& InData, const EUndoWorld InScope)
        {
            World* const lWorld = UndoWorld(InContext, InScope);
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

            UndoSelection(InContext, InScope).Replace(lWorld, lIds);
        }

        // The inverse, and it clears the selection for the reason DestroySelected does: what was
        // selected no longer exists.
        void DestroyEntities(EditorContext& InContext, const MapData& InData, const EUndoWorld InScope)
        {
            World* const lWorld = UndoWorld(InContext, InScope);
            if (lWorld == nullptr) { return; }

            UndoSelection(InContext, InScope).Clear();

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
        void WriteTransforms(EditorContext& InContext, const EntityTransform& InStep, const bool bInBefore)
        {
            World* const lWorld = UndoWorld(InContext, InStep.Scope);
            if (lWorld == nullptr) { return; }

            for (const EntityTransform::Entry& lEntry : InStep.Entries)
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

    // The level-only verbs name the active world literally; EntityDelete carries its scope, being
    // the one of these the prefab panel records too (P8 V4).
    void EntityCreate::Undo(EditorContext& InContext) { DestroyEntities(InContext, Entities, EUndoWorld::Active); }
    void EntityCreate::Redo(EditorContext& InContext) { RestoreEntities(InContext, Entities, EUndoWorld::Active); }

    void PrefabInstantiate::Undo(EditorContext& InContext) { DestroyEntities(InContext, Entities, Scope); }
    void PrefabInstantiate::Redo(EditorContext& InContext) { RestoreEntities(InContext, Entities, Scope); }

    // Destroy THEN restore, both ways: RestoreEntities selects what it brought back and
    // DestroyEntities clears, so the opposite order would leave an empty selection.
    void PrefabCreateFromSelection::Undo(EditorContext& InContext)
    {
        DestroyEntities(InContext, Instance, EUndoWorld::Active);
        RestoreEntities(InContext, Originals, EUndoWorld::Active);
    }

    void PrefabCreateFromSelection::Redo(EditorContext& InContext)
    {
        DestroyEntities(InContext, Originals, EUndoWorld::Active);
        RestoreEntities(InContext, Instance, EUndoWorld::Active);
    }

    // A revert normally creates and destroys nothing, so "be this again" is the whole inverse
    // (**UN4**) — EXCEPT when it brought a deleted piece back (undo takes it away again) or took
    // an orphaned piece away (Before holds it, so restoring Before brings it back; redo destroys
    // it again). Destroy first, restore second, both ways, for PrefabCreateFromSelection's reason:
    // RestoreEntities selects what it brought back and DestroyEntities clears.
    void PrefabRevert::Undo(EditorContext& InContext)
    {
        DestroyEntities(InContext, Created, EUndoWorld::Active);
        RestoreEntities(InContext, Before, EUndoWorld::Active);
    }

    void PrefabRevert::Redo(EditorContext& InContext)
    {
        DestroyEntities(InContext, Destroyed, EUndoWorld::Active);
        RestoreEntities(InContext, After, EUndoWorld::Active);
    }

    void EntityDelete::Undo(EditorContext& InContext) { RestoreEntities(InContext, Entities, Scope); }
    void EntityDelete::Redo(EditorContext& InContext) { DestroyEntities(InContext, Entities, Scope); }

    void EntityRename::Undo(EditorContext& InContext) { WriteName(InContext, EntityId, Before); }
    void EntityRename::Redo(EditorContext& InContext) { WriteName(InContext, EntityId, After); }

    void EntityTransform::Begin(World& InWorld, const TDynArray<EntityID>& InIds, const char* InName,
                                const EUndoWorld InScope)
    {
        Entries.clear();
        Name  = InName != nullptr ? InName : "Transform";
        Scope = InScope;

        // BY GUID, not by handle: a step outlives the drag, and a handle does not survive an undo
        // that recreated the entity.
        for (const EntityID lId : InIds)
        {
            Entity lEntity{ lId, &InWorld };

            const TransformComponent* const lTransform = lEntity.TryGet<TransformComponent>();
            if (lTransform == nullptr) { continue; }

            Entries.emplace_back(Entry{ lEntity.GetGuid(), *lTransform, *lTransform });
        }
    }

    bool EntityTransform::End(const EditorContext& InContext)
    {
        World* const lWorld = UndoWorld(InContext, Scope);
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

    void EntityTransform::Undo(EditorContext& InContext) { WriteTransforms(InContext, *this, true); }
    void EntityTransform::Redo(EditorContext& InContext) { WriteTransforms(InContext, *this, false); }
}
