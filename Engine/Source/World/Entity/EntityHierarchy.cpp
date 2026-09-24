#include "World/Entity/EntityHierarchy.h"

#include "Core/Log/Logger.h"

namespace Opaax
{
    namespace
    {
        constexpr LogCategory LogEntityHierarchy{"EntityHierarchy"};

        bool Contains(const TDynArray<EntityID>& InIds, const EntityID InId)
        {
            for (const EntityID lId : InIds)
            {
                if (lId == InId) { return true; }
            }
            return false;
        }

        const char* NameOf(Entity InEntity)
        {
            const EntityMeta* lMeta = InEntity.IsValid() ? InEntity.TryGet<EntityMeta>() : nullptr;
            return lMeta != nullptr ? lMeta->Name.CStr() : "(invalid)";
        }
    }

    Entity EntityHierarchy::GetParent(Entity InEntity)
    {
        if (!InEntity.IsValid()) { return Entity{}; }

        const EntityMeta* lMeta = InEntity.TryGet<EntityMeta>();
        if (lMeta == nullptr || !lMeta->Parent.IsValid()) { return Entity{}; }

        return InEntity.GetWorld()->FindByGuid(lMeta->Parent);
    }

    bool EntityHierarchy::IsDescendantOf(Entity InEntity, Entity InAncestor)
    {
        if (!InEntity.IsValid() || !InAncestor.IsValid()) { return false; }

        Entity lCursor = GetParent(InEntity);
        for (Uint32 lDepth = 0; lDepth < MAX_DEPTH && lCursor.IsValid(); ++lDepth)
        {
            if (lCursor.GetHandle() == InAncestor.GetHandle()) { return true; }
            lCursor = GetParent(lCursor);
        }

        return false;
    }

    TransformComponent EntityHierarchy::WorldTransform(Entity InEntity)
    {
        return ComposeChain(InEntity, [](Entity InHop)
        {
            const TransformComponent* lLocal = InHop.TryGet<TransformComponent>();
            return lLocal != nullptr ? *lLocal : TransformComponent{};
        });
    }

    void EntityHierarchy::SetWorldTransform(Entity InEntity, const TransformComponent& InWorld)
    {
        if (!InEntity.IsValid()) { return; }

        TransformComponent* lLocal = InEntity.TryGet<TransformComponent>();
        if (lLocal == nullptr) { return; }

        const Entity lParent = GetParent(InEntity);
        *lLocal = lParent.IsValid() ? ToLocal(WorldTransform(lParent), InWorld) : InWorld;
    }

    bool EntityHierarchy::SetParent(Entity InChild, Entity InParent, const bool bInKeepWorld)
    {
        if (!InChild.IsValid())
        {
            OPAAX_LOG(LogEntityHierarchy, Warn, "SetParent ignored — invalid child");
            return false;
        }

        World* const lWorld = InChild.GetWorld();
        EntityMeta&  lMeta  = InChild.Get<EntityMeta>();

        if (!InParent.IsValid())
        {
            if (!lMeta.Parent.IsValid()) { return true; }   // already a root

            if (bInKeepWorld) { InChild.Get<TransformComponent>() = WorldTransform(InChild); }

            lMeta.Parent = Guid{};
            lWorld->MarkChanged();

            OPAAX_LOG(LogEntityHierarchy, Info, "Detached '{}' to root{}", lMeta.Name.CStr(),
                      bInKeepWorld ? " (kept world pose)" : "");
            return true;
        }

        if (InParent.GetWorld() != lWorld)
        {
            OPAAX_LOG(LogEntityHierarchy, Warn, "SetParent refused — '{}' and '{}' are in different worlds",
                      lMeta.Name.CStr(), NameOf(InParent));
            return false;
        }

        if (InParent.GetHandle() == InChild.GetHandle())
        {
            OPAAX_LOG(LogEntityHierarchy, Warn, "SetParent refused — '{}' cannot be its own parent", lMeta.Name.CStr());
            return false;
        }

        if (IsDescendantOf(InParent, InChild))
        {
            OPAAX_LOG(LogEntityHierarchy, Warn, "SetParent refused — '{}' is already under '{}' (cycle)",
                      NameOf(InParent), lMeta.Name.CStr());
            return false;
        }

        const EntityMeta& lParentMeta = InParent.Get<EntityMeta>();
        if (lMeta.Parent == lParentMeta.Id) { return true; }   // already there

        if (bInKeepWorld)
        {
            const TransformComponent lWorldPose = WorldTransform(InChild);
            InChild.Get<TransformComponent>()   = ToLocal(WorldTransform(InParent), lWorldPose);
        }

        lMeta.Parent = lParentMeta.Id;

        // A child lives in its parent's map. Only a mapped parent pulls; only mapped descendants
        // follow — a runtime-spawned entity stays runtime (WM2).
        const MapId lTargetMap = lParentMeta.OwnerMap;
        if (lTargetMap.IsValid() && lMeta.OwnerMap != lTargetMap)
        {
            TDynArray<EntityID> lSubtree;
            CollectSubtree(*lWorld, { InChild.GetHandle() }, lSubtree);

            for (const EntityID lId : lSubtree)
            {
                EntityMeta& lMoved = lWorld->GetRegistry().get<EntityMeta>(lId);
                if (lMoved.OwnerMap.IsValid()) { lMoved.OwnerMap = lTargetMap; }
            }
        }

        lWorld->MarkChanged();

        OPAAX_LOG(LogEntityHierarchy, Info, "Parented '{}' under '{}'{}", lMeta.Name.CStr(),
                  lParentMeta.Name.CStr(), bInKeepWorld ? " (kept world pose)" : "");
        return true;
    }

    void EntityHierarchy::CollectSubtree(World& InWorld, const TDynArray<EntityID>& InRoots,
                                         TDynArray<EntityID>& OutIds)
    {
        // Breadth-first over OutIds itself, so parents land before their children and a root that
        // is also another root's descendant is taken once.
        for (const EntityID lRoot : InRoots)
        {
            if (InWorld.IsValid(lRoot) && !Contains(OutIds, lRoot)) { OutIds.emplace_back(lRoot); }
        }

        for (Uint64 lIndex = 0; lIndex < OutIds.size(); ++lIndex)
        {
            ForEachChild(InWorld, OutIds[lIndex], [&OutIds](const EntityID InChild)
            {
                if (!Contains(OutIds, InChild)) { OutIds.emplace_back(InChild); }
            });
        }
    }

    void EntityHierarchy::TopmostOf(World& InWorld, const TDynArray<EntityID>& InIds,
                                    TDynArray<EntityID>& OutIds)
    {
        for (const EntityID lId : InIds)
        {
            bool lCovered = false;

            Entity lCursor = GetParent(Entity{ lId, &InWorld });
            for (Uint32 lDepth = 0; lDepth < MAX_DEPTH && lCursor.IsValid(); ++lDepth)
            {
                if (Contains(InIds, lCursor.GetHandle())) { lCovered = true; break; }
                lCursor = GetParent(lCursor);
            }

            if (!lCovered) { OutIds.emplace_back(lId); }
        }
    }
}
