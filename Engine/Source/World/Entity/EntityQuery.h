#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/Bounds2D.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class Entity;
    class World;

    // =============================================================================
    // EntityQuery — WHERE an entity is in world space, and what is at a given point or region.
    //
    //   THE ONE ENTITY-AABB RULE. Picking, the selection outline, focus-selected, the marquee and
    //   later the gizmo all ask the same question, and every one of them asks it here — so when a
    //   component's extent moves, ONE function body changes instead of five call sites.
    //
    //   Exported because SandboxEditor.exe calls all of it (I6 — the tell that has produced LNK2019
    //   four times in this tree; paid at authoring time, as CameraView's helpers now are). Out of
    //   line for the second reason too: the bodies name every extent-bearing component, and no
    //   consumer should have to include them to ask where something is.
    // =============================================================================
    namespace EntityQuery
    {
        /**
         * The axis-aligned box covering everything this entity draws, in world units.
         *
         * TWO TIERS. A component with an extent (sprite, quad) gives a real box, rotated by the
         * transform; an entity with none has only its transform, and gets a box of
         * InAnchorHalfExtent so it can still be clicked — the editor icon's job, and Unreal's
         * billboard / Godot's origin grab-area in this engine's terms.
         *
         * @param InAnchorHalfExtent Half-size of that fallback box. ZERO (the default) means "no
         *   fallback": a game or a test asking what an entity DRAWS wants false, not a placeholder.
         *   The editor passes the world-unit equivalent of a fixed pixel size.
         * @return False when the entity is invalid, has no transform, or has no extent and no
         *   anchor was asked for. OutBounds is untouched in that case.
         */
        OPAAX_API bool TryGetBounds(Entity InEntity, Bounds2D& OutBounds, float InAnchorHalfExtent = 0.f);

        /** The box covering all of InIds. Entities with no bounds are skipped; false if none had any. */
        OPAAX_API bool TryGetBounds(World& InWorld, const TDynArray<EntityID>& InIds, Bounds2D& OutBounds,
                                    float InAnchorHalfExtent = 0.f);

        /**
         * The TOPMOST entity whose bounds contain InWorldPoint, or an invalid Entity for a miss.
         *
         * Topmost is the renderer's own order — (ERenderLayer, OrderInLayer) — so what you click is
         * what you see on top. A quad reads as Default/0 (what RendererManager submits it with) and
         * an anchor-only entity sorts below everything drawn, so an icon never steals a click from
         * a sprite it happens to sit under. Ties go to the last iterated, matching the way equal
         * sort keys resolve by submission order.
         */
        OPAAX_API Entity PickAt(World& InWorld, const Vector2F& InWorldPoint, float InAnchorHalfExtent = 0.f);

        /**
         * Every entity whose bounds overlap InRegion — the marquee. Appends to OutIds in iteration
         * order and never clears it, so callers may accumulate.
         */
        OPAAX_API void QueryOverlapping(World& InWorld, const Bounds2D& InRegion, TDynArray<EntityID>& OutIds,
                                        float InAnchorHalfExtent = 0.f);
    }
}
