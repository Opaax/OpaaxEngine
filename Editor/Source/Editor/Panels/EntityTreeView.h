#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "World/Entity/EntityTypes.h"

#include <unordered_map>

namespace Opaax
{
    class Entity;
    class World;

    OPAAX_LOG_CATEGORY(EntityTreeView);
}

namespace Opaax::Editor
{
    class EditorSelection;

    /** What a drag landed on — banked for the caller to spend AFTER the walk (**MP7**). */
    struct EntityTreeDrop
    {
        EntityID Child  = ENTITY_NONE;
        EntityID Parent = ENTITY_NONE;   // invalid = to root
        MapId    ToMap;                  // a map header names its map; invalid = keep the child's
    };

    // =============================================================================
    // EntityTreeView — one world's entities as ImGui tree rows: a parent opens on its children, a
    //   click selects, a row is a drag source and a drop target (§HR). Shared by the Hierarchy and
    //   the prefab panel the way the viewport gestures are (**PF12**): the tree is the same tree
    //   whichever document owns the world; only the verbs around it differ.
    //
    //   NOTHING IS APPLIED HERE. A drop arrives mid-walk, so it is banked and the panel spends it
    //   once its pass is over — the rule every Hierarchy verb already follows.
    //
    //   Children are DERIVED per pass from EntityMeta::Parent — Rebuild once, then draw.
    // =============================================================================
    class EntityTreeView
    {
        // =============================================================================
        // Build
        // =============================================================================
    public:
        /** The child map and the root list, from the world as it is now. Once per draw pass. */
        void Rebuild(World& InWorld);

        /** Entities with no (resolvable) parent, in registry order. The caller buckets these. */
        const TDynArray<EntityID>& Roots() const noexcept { return m_Roots; }

        // =============================================================================
        // Draw
        // =============================================================================
    public:
        using ContextMenuFn = TFunction<void(Entity)>;

        /**
         * InEntity's row and, when open, its subtree. Ctrl toggles, a plain click replaces.
         * InContextMenu runs right after the row so the panel can hang its popup on it.
         */
        void DrawNode(World& InWorld, EntityID InEntity, EditorSelection& InSelection,
                      const ContextMenuFn& InContextMenu);

        /** Make the LAST ITEM a drop target meaning "to root, in InMap" — a map header. */
        void AcceptRootDrop(MapId InMap);

        /** A "drop here to unparent" row, drawn only while an entity is being dragged. */
        void DrawUnparentStrip(MapId InMap);

        /** The drop banked this pass, if any. Cleared. */
        bool TakeDrop(EntityTreeDrop& OutDrop);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static constexpr const char* k_Payload = "OPAAX_ENTITY";

        const TDynArray<EntityID>* ChildrenOf(EntityID InEntity) const;

        std::unordered_map<Uint32, TDynArray<EntityID>> m_Children;   // by entity bits
        TDynArray<EntityID>                             m_Roots;

        EntityTreeDrop m_Drop;
        bool           m_bHasDrop = false;
    };
}
