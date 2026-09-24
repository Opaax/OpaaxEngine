#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
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

    /** What a dragged ROW landed on — banked for the caller to spend AFTER the walk (**MP7**). */
    struct EntityTreeDrop
    {
        EntityID Child  = ENTITY_NONE;
        EntityID Parent = ENTITY_NONE;   // invalid = to root
        MapId    ToMap;                  // a map header names its map; invalid = keep the child's
    };

    /** What a dragged PREFAB (from the browser) landed on — the same bank, one type over. */
    struct EntityTreePrefabDrop
    {
        OpaaxString AssetPath;           // asset-relative, as the browser drags it
        EntityID    OnEntity = ENTITY_NONE;   // a row: instantiate as its child; invalid = a header
        MapId       ToMap;               // the header's map
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

        /**
         * Make the LAST ITEM a drop target — a map header: a row dropped there goes to root in
         * InMap; a prefab dropped there is instantiated into InMap.
         */
        void AcceptRootDrop(MapId InMap);

        /** A "drop here to unparent" row, drawn only while an entity is being dragged. */
        void DrawUnparentStrip(MapId InMap);

        /** The row drop banked this pass, if any. Cleared. */
        bool TakeDrop(EntityTreeDrop& OutDrop);

        /** The prefab drop banked this pass, if any. Cleared. */
        bool TakePrefabDrop(EntityTreePrefabDrop& OutDrop);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static constexpr const char* k_Payload = "OPAAX_ENTITY";

        const TDynArray<EntityID>* ChildrenOf(EntityID InEntity) const;

        std::unordered_map<Uint32, TDynArray<EntityID>> m_Children;   // by entity bits
        TDynArray<EntityID>                             m_Roots;

        EntityTreeDrop       m_Drop;
        bool                 m_bHasDrop = false;

        EntityTreePrefabDrop m_PrefabDrop;
        bool                 m_bHasPrefabDrop = false;
    };
}
