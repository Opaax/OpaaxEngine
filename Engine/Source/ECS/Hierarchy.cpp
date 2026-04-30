#include "Hierarchy.h"

#include <cmath>

#include "Core/Log/OpaaxLog.h"
#include "Core/World/World.h"
#include "ECS/Components/ParentComponent.h"
#include "ECS/Components/TransformComponent.h"

namespace Opaax::ECS::Hierarchy
{
    namespace
    {
        // Hard cap on chain depth to defend GetWorldTransform / IsDescendantOf against
        // a corrupted hierarchy. SetParent prevents cycles for normal mutations.
        constexpr int MAX_HIERARCHY_DEPTH = 256;

        EntityID GetParent(const World& InWorld, EntityID InEntity)
        {
            if (!InWorld.IsValid(InEntity)) { return ENTITY_NONE; }
            if (const auto* lP = InWorld.GetComponent<ParentComponent>(InEntity))
            {
                return lP->Parent;
            }
            return ENTITY_NONE;
        }
    }

    bool IsDescendantOf(const World& InWorld, EntityID InEntity, EntityID InAncestor)
    {
        if (InEntity == ENTITY_NONE || InAncestor == ENTITY_NONE) { return false; }

        EntityID lCursor = GetParent(InWorld, InEntity);
        for (int i = 0; i < MAX_HIERARCHY_DEPTH && lCursor != ENTITY_NONE; ++i)
        {
            if (lCursor == InAncestor) { return true; }
            lCursor = GetParent(InWorld, lCursor);
        }
        return false;
    }

    bool SetParent(World& InWorld, EntityID InChild, EntityID InNewParent)
    {
        if (!InWorld.IsValid(InChild))
        {
            OPAAX_CORE_WARN("Hierarchy::SetParent — invalid child, ignored.");
            return false;
        }

        if (InNewParent == ENTITY_NONE)
        {
            ClearParent(InWorld, InChild);
            return true;
        }

        if (InChild == InNewParent)
        {
            OPAAX_CORE_WARN("Hierarchy::SetParent — self-parenting rejected.");
            return false;
        }

        if (!InWorld.IsValid(InNewParent))
        {
            OPAAX_CORE_WARN("Hierarchy::SetParent — invalid parent, ignored.");
            return false;
        }

        // Cycle check: would the prospective parent end up under the child?
        if (IsDescendantOf(InWorld, InNewParent, InChild))
        {
            OPAAX_CORE_WARN("Hierarchy::SetParent — cycle rejected.");
            return false;
        }

        if (auto* lExisting = InWorld.GetComponent<ParentComponent>(InChild))
        {
            lExisting->Parent = InNewParent;
        }
        else
        {
            InWorld.AddComponent<ParentComponent>(InChild, InNewParent);
        }
        return true;
    }

    void ClearParent(World& InWorld, EntityID InChild)
    {
        if (!InWorld.IsValid(InChild)) { return; }
        if (InWorld.HasComponent<ParentComponent>(InChild))
        {
            InWorld.RemoveComponent<ParentComponent>(InChild);
        }
    }

    WorldTransform GetWorldTransform(const World& InWorld, EntityID InEntity)
    {
        WorldTransform lOut;

        if (!InWorld.IsValid(InEntity)) { return lOut; }

        // Walk to root, collecting up to MAX_HIERARCHY_DEPTH ancestors (root last).
        EntityID lChain[MAX_HIERARCHY_DEPTH];
        int lCount = 0;

        EntityID lCursor = InEntity;
        while (lCursor != ENTITY_NONE && lCount < MAX_HIERARCHY_DEPTH)
        {
            lChain[lCount++] = lCursor;
            lCursor = GetParent(InWorld, lCursor);
        }

        // Compose root → leaf.
        for (int i = lCount - 1; i >= 0; --i)
        {
            const auto* lLocal = InWorld.GetComponent<TransformComponent>(lChain[i]);
            if (!lLocal) { continue; }

            // Apply parent rotation + scale to the local position before translating.
            const float lCos = std::cos(lOut.Rotation);
            const float lSin = std::sin(lOut.Rotation);
            const float lLx  = lLocal->Position.x * lOut.Scale.x;
            const float lLy  = lLocal->Position.y * lOut.Scale.y;

            lOut.Position.x += lCos * lLx - lSin * lLy;
            lOut.Position.y += lSin * lLx + lCos * lLy;

            lOut.Scale.x   *= lLocal->Scale.x;
            lOut.Scale.y   *= lLocal->Scale.y;
            lOut.Rotation  += lLocal->Rotation;
            lOut.ZOrder    += lLocal->ZOrder;
        }

        return lOut;
    }
}
