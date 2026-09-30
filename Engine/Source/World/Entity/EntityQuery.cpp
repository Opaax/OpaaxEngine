#include "World/Entity/EntityQuery.h"

#include "Core/Maths/Maths.h"   // DegreesToRadians

#include "Renderer/Text/Text2D.h"   // EstimateExtent

#include "World/Components/DummyComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TextComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        // Below every drawn thing, so an icon never takes a click from a sprite.
        constexpr Int32 RANK_ANCHOR_ONLY = INT32_MIN;

        /**
         * One integer per (layer, order), for sorting.
         */
        constexpr Int32 MakeRank(ERenderLayer InLayer, Int16 InOrder) noexcept
        {
            return (static_cast<Int32>(InLayer) << 16) + static_cast<Int32>(InOrder);
        }

        /** The entity's draw order (the highest of what it draws). */
        Int32 DrawRank(Entity& InEntity)
        {
            Int32 lRank = RANK_ANCHOR_ONLY;

            // Same defaults as DrawQuad.
            if (InEntity.Has<DummyComponent>())
            {
                lRank = MakeRank(ERenderLayer::Default, 0);
            }

            if (const SpriteComponent* lSprite = InEntity.TryGet<SpriteComponent>())
            {
                const Int32 lSpriteRank = MakeRank(lSprite->Layer, lSprite->OrderInLayer);
                if (lRank == RANK_ANCHOR_ONLY || lSpriteRank > lRank) { lRank = lSpriteRank; }
            }

            if (const TextComponent* lText = InEntity.TryGet<TextComponent>())
            {
                const Int32 lTextRank = MakeRank(lText->Layer, lText->OrderInLayer);
                if (lRank == RANK_ANCHOR_ONLY || lTextRank > lRank) { lRank = lTextRank; }
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

        if (InEntity.TryGet<TransformComponent>() == nullptr)
        {
            // Only for an entity not made by CreateEntity (every entity has one).
            return false;
        }

        // World pose: a child is clicked where it is drawn.
        const TransformComponent lWorldXf = EntityHierarchy::WorldTransform(InEntity);

        const float lRotation = Maths::DegreesToRadians(lWorldXf.Rotation);

        bool     lHasExtent = false;
        Bounds2D lBounds;

        // Same scale as the renderer, so the clickable size matches the drawn size.
        const Vector2F lScale = lWorldXf.Scale;

        if (const DummyComponent* lQuad = InEntity.TryGet<DummyComponent>())
        {
            lBounds    = Bounds2D::FromCenterSizeRotated(lWorldXf.Position, lQuad->Size * lScale, lRotation);
            lHasExtent = true;
        }

        if (const SpriteComponent* lSprite = InEntity.TryGet<SpriteComponent>())
        {
            // Clickable even when hidden, so it can be selected and shown again.
            const Bounds2D lSpriteBounds =
                Bounds2D::FromCenterSizeRotated(lWorldXf.Position, lSprite->Size * lScale, lRotation);

            if (lHasExtent) { lBounds.Encapsulate(lSpriteBounds); }
            else            { lBounds = lSpriteBounds; lHasExtent = true; }
        }

        if (const TextComponent* lText = InEntity.TryGet<TextComponent>())
        {
            // Text is anchored top-left, so the box is offset by half its size.
            // The size is an estimate (no font access here), on the generous side.
            TextDrawParams lParams;
            lParams.Size            = lText->Size * lScale.x;
            lParams.LineHeightScale = lText->LineHeightScale;

            const Vector2F lExtent = Text2D::EstimateExtent(lText->Text.CStr(), lParams);

            if (lExtent.x > 0.f && lExtent.y > 0.f)
            {
                const Vector2F lCentre{ lWorldXf.Position.x + lExtent.x * 0.5f,
                                        lWorldXf.Position.y - lExtent.y * 0.5f };

                const Bounds2D lTextBounds = Bounds2D::FromCenterSizeRotated(lCentre, lExtent, lRotation);

                if (lHasExtent) { lBounds.Encapsulate(lTextBounds); }
                else            { lBounds = lTextBounds; lHasExtent = true; }
            }
        }

        if (!lHasExtent)
        {
            if (InAnchorHalfExtent <= 0.f)
            {
                return false;
            }

            lBounds = Bounds2D{ lWorldXf.Position, { InAnchorHalfExtent, InAnchorHalfExtent } };
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

        // Every entity has an EntityMeta.
        InWorld.Each<EntityMeta>([&](EntityID InId, const EntityMeta&)
        {
            Entity   lEntity{ InId, &InWorld };
            Bounds2D lBounds;

            if (!TryGetBounds(lEntity, lBounds, InAnchorHalfExtent) || !lBounds.Contains(InWorldPoint))
            {
                return;
            }

            const Int32 lRank = DrawRank(lEntity);

            // >= : a tie goes to the last one, like draw order.
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
