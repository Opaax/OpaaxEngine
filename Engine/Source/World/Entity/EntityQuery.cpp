#include "World/Entity/EntityQuery.h"

#include "Core/Maths/Maths.h"   // DegreesToRadians — the transform authors degrees

#include "World/Components/DummyComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        // Below every drawn thing, so an icon never wins a click from a sprite behind it.
        constexpr Int32 RANK_ANCHOR_ONLY = INT32_MIN;

        /**
         * One comparable integer per (layer, order). The layer step is 65536 and OrderInLayer is an
         * Int16, so the fine key can never reach into the next band.
         */
        constexpr Int32 MakeRank(ERenderLayer InLayer, Int16 InOrder) noexcept
        {
            return (static_cast<Int32>(InLayer) << 16) + static_cast<Int32>(InOrder);
        }

        /** Where this entity sits in the renderer's order — the highest of what it draws. */
        Int32 DrawRank(Entity& InEntity)
        {
            Int32 lRank = RANK_ANCHOR_ONLY;

            // The value RendererManager submits a quad with: DrawQuad's own defaults.
            if (InEntity.Has<DummyComponent>())
            {
                lRank = MakeRank(ERenderLayer::Default, 0);
            }

            if (const SpriteComponent* lSprite = InEntity.TryGet<SpriteComponent>())
            {
                const Int32 lSpriteRank = MakeRank(lSprite->Layer, lSprite->OrderInLayer);
                if (lRank == RANK_ANCHOR_ONLY || lSpriteRank > lRank) { lRank = lSpriteRank; }
            }

            return lRank;
        }
    }

    bool EntityQuery::TryGetBounds(Entity InEntity, Bounds2D& OutBounds, float InAnchorHalfExtent)
    {
        if (!InEntity.IsValid())
        {
            return false;
        }

        const TransformComponent* lTransform = InEntity.TryGet<TransformComponent>();
        if (lTransform == nullptr)
        {
            // Only reachable for an entity built outside CreateEntity — every entity gets one.
            return false;
        }

        const float lRotation = Maths::DegreesToRadians(lTransform->Rotation);

        bool     lHasExtent = false;
        Bounds2D lBounds;

        // The SAME multiply RendererManager applies (③) — a scaled entity has to be clickable at the
        // size it draws, and this is the one body that keeps picking, the outline, focus-selected and
        // the marquee agreeing about that (SEL1).
        const Vector2F lScale = lTransform->Scale;

        if (const DummyComponent* lQuad = InEntity.TryGet<DummyComponent>())
        {
            lBounds    = Bounds2D::FromCenterSizeRotated(lTransform->Position, lQuad->Size * lScale, lRotation);
            lHasExtent = true;
        }

        if (const SpriteComponent* lSprite = InEntity.TryGet<SpriteComponent>())
        {
            // Hit-testable whether or not it is VISIBLE: an author has to be able to select a
            // sprite they just hid in order to show it again.
            const Bounds2D lSpriteBounds =
                Bounds2D::FromCenterSizeRotated(lTransform->Position, lSprite->Size * lScale, lRotation);

            if (lHasExtent) { lBounds.Encapsulate(lSpriteBounds); }
            else            { lBounds = lSpriteBounds; lHasExtent = true; }
        }

        if (!lHasExtent)
        {
            if (InAnchorHalfExtent <= 0.f)
            {
                return false;
            }

            lBounds = Bounds2D{ lTransform->Position, { InAnchorHalfExtent, InAnchorHalfExtent } };
        }

        OutBounds = lBounds;
        return true;
    }

    bool EntityQuery::TryGetBounds(World& InWorld, const TDynArray<EntityID>& InIds, Bounds2D& OutBounds,
                                   float InAnchorHalfExtent)
    {
        bool     lAny = false;
        Bounds2D lCombined;

        for (const EntityID lId : InIds)
        {
            Bounds2D lBounds;
            if (!TryGetBounds(Entity{ lId, &InWorld }, lBounds, InAnchorHalfExtent))
            {
                continue;
            }

            if (lAny) { lCombined.Encapsulate(lBounds); }
            else      { lCombined = lBounds; lAny = true; }
        }

        if (lAny) { OutBounds = lCombined; }

        return lAny;
    }

    Entity EntityQuery::PickAt(World& InWorld, const Vector2F& InWorldPoint, float InAnchorHalfExtent)
    {
        EntityID lBest     = ENTITY_NONE;
        Int32    lBestRank = 0;

        // EntityMeta is the complete all-entities view (CreateEntityWithGuid emplaces it), which is
        // what makes "everything under the cursor" mean everything.
        InWorld.Each<EntityMeta>([&](EntityID InId, const EntityMeta&)
        {
            Entity   lEntity{ InId, &InWorld };
            Bounds2D lBounds;

            if (!TryGetBounds(lEntity, lBounds, InAnchorHalfExtent) || !lBounds.Contains(InWorldPoint))
            {
                return;
            }

            const Int32 lRank = DrawRank(lEntity);

            // >= so a tie goes to the LAST iterated: equal sort keys resolve by submission order in
            // the batch, and submission order is this order.
            if (lBest == ENTITY_NONE || lRank >= lBestRank)
            {
                lBest     = InId;
                lBestRank = lRank;
            }
        });

        return lBest == ENTITY_NONE ? Entity{} : Entity{ lBest, &InWorld };
    }

    void EntityQuery::QueryOverlapping(World& InWorld, const Bounds2D& InRegion, TDynArray<EntityID>& OutIds,
                                       float InAnchorHalfExtent)
    {
        InWorld.Each<EntityMeta>([&](EntityID InId, const EntityMeta&)
        {
            Bounds2D lBounds;

            if (TryGetBounds(Entity{ InId, &InWorld }, lBounds, InAnchorHalfExtent)
                && lBounds.Intersects(InRegion))
            {
                OutIds.emplace_back(InId);
            }
        });
    }
}
