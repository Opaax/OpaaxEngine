#pragma once

#include "Core/String/OpaaxString.hpp"
#include "World/Entity/EntityTypes.h"   // MapId

namespace Opaax
{
    class Level;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // MapOps — actions on one given map of the open level. Each takes its target map explicitly
    //   (a level holds several maps): the File menu passes the focused map, the Hierarchy passes the
    //   clicked header.
    // =============================================================================
    namespace MapOps
    {
        /** The active world's Level, or null when there is no world (or a bare one). */
        Level* ActiveLevel(const EditorContext& InContext);

        /**
         * False during Play: the active world is then a Play copy, and writing it back would save
         * simulation state over the file.
         * @param InVerb The action name, used in the refusal message
         */
        bool CanEdit(const EditorContext& InContext, const char* InVerb);

        /**
         * Moves the editing cursor onto an already loaded map. Loads nothing; the selection is kept.
         * @param InAssetRelPath Asset-relative, as the level file names maps
         */
        void Focus(EditorContext& InContext, const OpaaxString& InAssetRelPath);

        /** Writes one map. Save Level writes them all. */
        void Save(EditorContext& InContext, MapId InMapId);

        /** Makes InMapId the persistent map (the one the others are composed on). */
        void SetPersistent(EditorContext& InContext, MapId InMapId);

        /**
         * Unloads InMapId and removes it from the level. The persistent map is refused. The cursor only
         * moves if it was on this map.
         */
        void RemoveFromLevel(EditorContext& InContext, MapId InMapId);

        /**
         * Removes a level entry whose file never loaded (missing, renamed or moved). Takes a path because
         * there is no file to get a MapId from.
         */
        void RemoveMissingFromLevel(EditorContext& InContext, const OpaaxString& InAssetRelPath);
    }
}
