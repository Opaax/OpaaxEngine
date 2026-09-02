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
        // Resolve the (entity, type) pair a step names, or nothing. Both halves can legitimately
        // fail — the entity may have been destroyed by a later step, the type may be gone from a
        // build that dropped it — and neither is worth more than doing nothing.
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

            // Refuses an essential type on its own (**I17**) — the guarantee lives in the entry,
            // not in every caller remembering it.
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
        // One entity's components, as data. CaptureEntities is the same walk the map writer uses, so
        // a step and a saved map cannot disagree about what a component IS.
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

        void WritePayloads(EditorContext& InContext, const Guid& InId,
                           const TDynArray<ComponentData>& InSide)
        {
            World* const lWorld = InContext.Worlds.GetActiveWorld();
            if (lWorld == nullptr) { return; }

            Entity lEntity = lWorld->FindByGuid(InId);
            if (!lEntity.IsValid()) { return; }

            EntityRegistry&          lEntities = lEntity.GetWorld()->GetRegistry();
            const ComponentRegistry& lTypes    = InContext.Engine.GetRegistries().Components();

            for (const ComponentData& lComponent : InSide)
            {
                const IComponentEntry* const lEntry = lTypes.FindByName(lComponent.TypeName);

                // Absent means a later step took the component off. Restoring its VALUES must not
                // put it back — that is ComponentRemove's job, and its own step.
                if (lEntry == nullptr || !lEntry->Has(lEntities, lEntity.GetHandle())) { continue; }

                lEntry->Load(lEntities, lEntity.GetHandle(), lComponent.Payload);
            }

            lWorld->MarkChanged();
        }
    }

    void EntityComponentsEdit::Begin(const EditorContext& InContext, Entity InEntity)
    {
        EntityId = Guid();
        Before.clear();
        After.clear();

        const World* const lWorld = InContext.Worlds.GetActiveWorld();
        if (lWorld == nullptr || !InEntity.IsValid()) { return; }

        Before = CaptureComponents(InContext, *lWorld, InEntity.GetHandle(), EntityId);
    }

    bool EntityComponentsEdit::End(const EditorContext& InContext)
    {
        if (!EntityId.IsValid() || Before.empty()) { return false; }

        World* const lWorld = InContext.Worlds.GetActiveWorld();
        if (lWorld == nullptr) { return false; }

        Entity lEntity = lWorld->FindByGuid(EntityId);
        if (!lEntity.IsValid()) { return false; }

        Guid                           lNowId;
        const TDynArray<ComponentData> lNow = CaptureComponents(InContext, *lWorld,
                                                                lEntity.GetHandle(), lNowId);

        // NARROWED TO WHAT ACTUALLY CHANGED. A type missing from one side was added or removed and
        // has its own step; the rest of the entity is not this step's business.
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

    void EntityComponentsEdit::Undo(EditorContext& InContext) { WritePayloads(InContext, EntityId, Before); }
    void EntityComponentsEdit::Redo(EditorContext& InContext) { WritePayloads(InContext, EntityId, After); }
}
