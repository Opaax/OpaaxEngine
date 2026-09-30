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

    /** Where a dragged row was dropped, applied by the caller after the walk. */
    struct EntityTreeDrop
    {
        EntityID Child  = ENTITY_NONE;
        EntityID Parent = ENTITY_NONE;   // invalid = root
        MapId    ToMap;                  // a map header's map; invalid = keep the child's
    };

    /** Where a dragged prefab (from the browser) was dropped. */
    struct EntityTreePrefabDrop
    {
        OpaaxString AssetPath;           // asset-relative
        EntityID    OnEntity = ENTITY_NONE;   // a row: instantiate as its child; invalid = a header
        MapId       ToMap;               // the header's map
    };

    // =============================================================================
    // EntityTreeView — one world's entities as tree rows: parents open on their children, a click
    //   selects, a row can be dragged and dropped on. Shared by the Hierarchy and the prefab panel.
    //   Drops are stored and applied by the panel after the walk. Children are rebuilt from
    //   EntityMeta::Parent each pass.
    // =============================================================================
    class EntityTreeView
    {
        // =============================================================================
        // Build
        // =============================================================================
    public:
        /** The child map and the root list, from the world now. Once per pass. */
        void Rebuild(World& InWorld);

        /** Entities with no (resolvable) parent, in registry order. */
        const TDynArray<EntityID>& Roots() const noexcept { return m_Roots; }

        // =============================================================================
        // Draw
        // =============================================================================
    public:
        using ContextMenuFn = TFunction<void(Entity)>;

        /**
         * InEntity's row and, when open, its subtree. Ctrl toggles, a click replaces.
         * InContextMenu runs right after the row (for the panel's popup).
         */
        void DrawNode(World& InWorld, EntityID InEntity, EditorSelection& InSelection,
                      const ContextMenuFn& InContextMenu);

        /**
         * Makes the last item a drop target (a map header): a row goes to the root of InMap;
         * a prefab is instantiated into InMap.
         */
        void AcceptRootDrop(MapId InMap);

        /** A "drop here to unparent" row, only while an entity is dragged. */
        void DrawUnparentStrip(MapId InMap);

        /** The row drop of this pass, if any. Cleared. */
        bool TakeDrop(EntityTreeDrop& OutDrop);

        /** The prefab drop of this pass, if any. Cleared. */
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
