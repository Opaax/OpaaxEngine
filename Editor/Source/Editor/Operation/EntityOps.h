#pragma once

#include "Core/Maths/MathTypes.h"       // Vector2F / Matrix44F
#include "Core/OpaaxTypes.h"            // Uint8
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/Undo/UndoWorld.h"      // EUndoWorld
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    class  ComponentRegistry;
    class  Entity;
    struct MapData;
    class  World;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // EntityOps — the actions that create, destroy, move or frame entities. The Hierarchy's menus,
    //   the Edit menu and the viewport keys all use these (never the World directly), and each records
    //   its own undo step. The Inspector's drawers are the exception (they write components directly,
    //   hence World::GetRevision). Every mutator checks MapOps::CanEdit (refused during Play).
    // =============================================================================
    namespace EntityOps
    {
        /**
         * Creates an entity in InOwnerMap, selects it, and marks the world changed.
         * The map is required, so the entity can be saved. No components (add them in the Inspector).
         * InName is made unique ("Entity", "Entity 1", ...).
         * @return The new entity, or an invalid one if refused (no world, Play running, no map)
         */
        Entity Create(EditorContext& InContext, MapId InOwnerMap, const OpaaxString& InName);

        /**
         * Places one instance of the prefab at InAbsPath into InOwnerMap, selects all of it, and
         * records one undo step. The marker stores the asset-relative path.
         * @param InAtWorld Where to put it (its first entity lands there), or null for the authored
         *   positions. Applied before the undo capture, so a drop is one step.
         * @param InParent An entity to hang the instance's roots under (the authored root pose becomes
         *   the local). ENTITY_NONE places at root.
         * @return Number of entities created. 0 = refused (the log says why)
         */
        Uint64 InstantiatePrefab(EditorContext& InContext, const OpaaxString& InAbsPath, MapId InOwnerMap,
                                 const Vector2F* InAtWorld = nullptr, EntityID InParent = ENTITY_NONE);

        /**
         * Hangs the roots among InHandles under InParent, keeping their pose as the local.
         */
        void ParentPlaced(World& InWorld, const TDynArray<EntityID>& InHandles, EntityID InParent);

        /**
         * Puts a built instance into a given world, anchored at InAtWorld. The world-agnostic part of
         * InstantiatePrefab: no Play guard, no undo step, no selection.
         * @return The created handles, anchor first. Empty when nothing was created.
         */
        TDynArray<EntityID> PlaceInstance(World& InWorld, const MapData& InInstance, const Vector2F* InAtWorld,
                                          const ComponentRegistry& InRegistry);

        /**
         * Writes the selection as a new prefab at InAbsPath, then replaces it with an instance of that
         * prefab (so the originals stay linked). The instance goes into the originals' map.
         * The file is written first and is not part of undo.
         * @param InAbsPath Where to write. Refused outside the project's and engine's asset trees
         *   (checked before writing)
         * @return True if the prefab was written and the selection replaced
         */
        bool CreatePrefabFromSelection(EditorContext& InContext, const OpaaxString& InAbsPath);

        /**
         * Puts the selected instance entities back to their prefab's values (MapFactory::Restore: fields
         * overwritten, added components removed, deleted ones restored).
         * @param bInWholeInstance False: only the selected entities. True: every entity of their instances.
         * @return Number of entities reverted. 0 = no prefab link, or the prefab could not be resolved
         */
        Uint64 RevertToPrefab(EditorContext& InContext, bool bInWholeInstance);


        /**
         * Renames one entity. Empty names are refused. Not made unique (the author chose it).
         */
        void Rename(EditorContext& InContext, Entity InEntity, const OpaaxString& InName);

        /**
         * Puts InChild's subtree under InParent (an invalid InParent detaches it). The world pose is kept.
         * A drop on a map header passes InToMap. Used by the level Hierarchy and the prefab panel, each
         * on its own stack and world.
         * @return True if something changed and a step was recorded
         */
        bool Reparent(EditorContext& InContext, EUndoWorld InScope, EntityID InChild, EntityID InParent,
                      MapId InToMap = {});

        /** Detaches every selected entity that has a parent, one step each. */
        void DetachSelected(EditorContext& InContext, EUndoWorld InScope);

        /**
         * Destroys every selected entity and its descendants, and clears the selection. Refused during Play.
         */
        void DestroySelected(EditorContext& InContext);

        /**
         * Destroys given entities of a given world. World-agnostic: no Play guard, no undo, no selection
         * (the caller handles those).
         * @return Number destroyed
         */
        Uint64 DestroyEntities(World& InWorld, const TDynArray<EntityID>& InEntities);

        /**
         * Adds a registered component to InEntity, by name (a command can carry a name, not a pointer).
         * @return False if the type is unknown, already present, or the edit was refused
         */
        bool AddComponent(EditorContext& InContext, Entity InEntity, OpaaxStringID InTypeName);

        /**
         * Removes a component from InEntity. Essential types are refused by the registry entry.
         * @return False if the type is unknown, absent, essential, or the edit was refused
         */
        bool RemoveComponent(EditorContext& InContext, Entity InEntity, OpaaxStringID InTypeName);

        /**
         * Where rotation and scale act on a multi-selection. Shared: entities orbit the pivot and keep
         * their formation. Individual: each turns about itself and does not move.
         */
        enum class ETransformOrigin : Uint8
        {
            Shared,
            Individual
        };

        /**
         * One drag frame: what moved, in which frame, and about what.
         */
        struct TransformDelta
        {
            /** World-space map from each entity's old placement to its new one. */
            Matrix44F Matrix = Matrix44F(1.f);

            /**
             * The frame Matrix's linear part is in (the gizmo pose, radians). Needed to recover a scale.
             */
            float FrameRad = 0.f;

            ETransformOrigin Origin = ETransformOrigin::Shared;
        };

        /**
         * Applies a world-space transform delta to the selection (gizmo drag). One matrix covers
         * translate, rotate and scale about the pivot, so multi-selections keep their layout.
         * Incremental (deltas compose), and self-contained, so a recorded drag can be replayed.
         */
        void TransformSelected(EditorContext& InContext, const TransformDelta& InDelta);

        /**
         * Applies a gizmo delta to given entities of a given world (also used by the prefab viewport).
         * No Play guard, no undo, no selection (the caller handles those).
         * @return True if anything moved
         */
        bool TransformEntities(World& InWorld, const TDynArray<EntityID>& InEntities,
                               const TransformDelta& InDelta);

        /**
         * Frames the selection with the editor camera. Refused outside Edit (a Play world uses its
         * CameraComponent).
         */
        void FocusSelected(EditorContext& InContext);
    }
}
