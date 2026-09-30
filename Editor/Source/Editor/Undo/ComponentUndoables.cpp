#include "Editor/Undo/ComponentUndoables.h"

#include "Editor/EditorContext.h"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"
#include "World/WorldManager.h"

namespace Opaax::Editor
{
    namespace
    {
        // Resolves the (entity, type) pair a step names, or nothing (the entity may be destroyed, the
        // type may be gone).
        struct Target
        {
            EntityRegistry*        Entities = nullptr;
            const IComponentEntry* Entry    = nullptr;
            EntityID               Handle   = ENTITY_NONE;

            // Named Owner, not World: a member sharing its type's name shadows it in-struct.
            World*                 Owner    = nullptr;

            bool IsValid() const noexcept { return Entities != nullptr && Entry != nullptr; }
        };

        Target Resolve(EditorContext& InContext, const Guid& InId, const OpaaxStringID InTypeName)
        {
            World* const lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr) { return {}; }

            Entity lEntity = lWorld->FindByGuid(InId);
            if (!lEntity.IsValid()) { return {}; }

            const IComponentEntry* const lEntry =
                InContext.Engine.GetRegistries().Components().FindByName(InTypeName);

            if (lEntry == nullptr) { return {}; }

            return { &lEntity.GetWorld()->GetRegistry(), lEntry, lEntity.GetHandle(), lWorld };
        }

        void Put(EditorContext& InContext, const Guid& InId, const OpaaxStringID InTypeName,
                 const nlohmann::json* InData)
        {
            const Target lTarget = Resolve(InContext, InId, InTypeName);
            if (!lTarget.IsValid()) { return; }

            lTarget.Entry->Add(*lTarget.Entities, lTarget.Handle);

            if (InData != nullptr)
            {
                lTarget.Entry->Load(*lTarget.Entities, lTarget.Handle, *InData);
            }

            lTarget.Owner->MarkChanged();
        }

        void Take(EditorContext& InContext, const Guid& InId, const OpaaxStringID InTypeName)
        {
            const Target lTarget = Resolve(InContext, InId, InTypeName);
            if (!lTarget.IsValid()) { return; }

            // Essential types are refused by the entry itself.
            if (lTarget.Entry->Remove(*lTarget.Entities, lTarget.Handle))
            {
                lTarget.Owner->MarkChanged();
            }
        }
    }

    void ComponentAdd::Undo(EditorContext& InContext) { Take(InContext, EntityId, TypeName); }
    void ComponentAdd::Redo(EditorContext& InContext) { Put(InContext, EntityId, TypeName, nullptr); }

    void ComponentRemove::Undo(EditorContext& InContext) { Put(InContext, EntityId, TypeName, &Data); }
    void ComponentRemove::Redo(EditorContext& InContext) { Take(InContext, EntityId, TypeName); }

    namespace
    {
        // One entity's components as data. Same walk as the map writer, so a step and a saved map agree.
        TDynArray<ComponentData> CaptureComponents(const EditorContext& InContext, const World& InWorld,
                                                   const EntityID InHandle, Guid& OutId)
        {
            MapData lData = MapSerializer::CaptureEntities(
                InWorld, InContext.Engine.GetRegistries().Components(), { InHandle });

            if (lData.Entities.empty()) { return {}; }

            OutId = lData.Entities[0].Id;

            return Move(lData.Entities[0].Components);
        }

        const ComponentData* FindComponent(const TDynArray<ComponentData>& InComponents,
                                           const OpaaxStringID InTypeName)
        {
            for (const ComponentData& lComponent : InComponents)
            {
                if (lComponent.TypeName == InTypeName) { return &lComponent; }
            }

            return nullptr;
        }

        void WritePayloads(EditorContext& InContext, const EntityComponentsEdit& InStep,
                           const TDynArray<ComponentData>& InSide)
        {
            World* const lWorld = UndoWorld(InContext, InStep.Scope);
            if (lWorld == nullptr) { return; }

            Entity lEntity = lWorld->FindByGuid(InStep.EntityId);
            if (!lEntity.IsValid()) { return; }

            EntityRegistry&          lEntities = lEntity.GetWorld()->GetRegistry();
            const ComponentRegistry& lTypes    = InContext.Engine.GetRegistries().Components();

            for (const ComponentData& lComponent : InSide)
            {
                const IComponentEntry* const lEntry = lTypes.FindByName(lComponent.TypeName);

                // Absent means a later step removed the component. Restoring values must not add it back.
                if (lEntry == nullptr || !lEntry->Has(lEntities, lEntity.GetHandle())) { continue; }

                lEntry->Load(lEntities, lEntity.GetHandle(), lComponent.Payload);
            }

            lWorld->MarkChanged();
        }
    }

    void EntityComponentsEdit::Begin(const EditorContext& InContext, Entity InEntity, const EUndoWorld InScope)
    {
        EntityId = Guid();
        Before.clear();
        After.clear();
        Scope = InScope;

        // The entity's own world, not the active one (prefab panel entities live elsewhere).
        if (!InEntity.IsValid()) { return; }

        Before = CaptureComponents(InContext, *InEntity.GetWorld(), InEntity.GetHandle(), EntityId);
    }

    bool EntityComponentsEdit::End(const EditorContext& InContext)
    {
        if (!EntityId.IsValid() || Before.empty()) { return false; }

        World* const lWorld = UndoWorld(InContext, Scope);
        if (lWorld == nullptr) { return false; }

        Entity lEntity = lWorld->FindByGuid(EntityId);
        if (!lEntity.IsValid()) { return false; }

        Guid                           lNowId;
        const TDynArray<ComponentData> lNow = CaptureComponents(InContext, *lWorld,
                                                                lEntity.GetHandle(), lNowId);

        // Keep only what changed. A type missing on one side was added or removed (its own step).
        TDynArray<ComponentData> lBefore;
        TDynArray<ComponentData> lAfter;

        for (const ComponentData& lWas : Before)
        {
            const ComponentData* const lIs = FindComponent(lNow, lWas.TypeName);

            if (lIs == nullptr || lIs->Payload == lWas.Payload) { continue; }

            lBefore.emplace_back(lWas);
            lAfter.emplace_back(*lIs);
        }

        Before = Move(lBefore);
        After  = Move(lAfter);

        return !Before.empty();
    }

    void EntityComponentsEdit::Undo(EditorContext& InContext) { WritePayloads(InContext, *this, Before); }
    void EntityComponentsEdit::Redo(EditorContext& InContext) { WritePayloads(InContext, *this, After); }
}
