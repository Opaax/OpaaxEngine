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
    // MapOps — the verbs that act on ONE NAMED MAP of the open level.
    //
    //   EVERY ONE OF THEM TAKES ITS TARGET. The menu bar used to carry "Remove Open Map" and
    //   "Set Open Map Persistent", which acted on whatever map happened to be FOCUSED — and a
    //   level holds several maps (**WM1a**), so an implicit focus is not how an author picks one
    //   of them. Naming the target is what let those two entries leave the menu bar for the
    //   Hierarchy's map headers, where the map you click IS the argument.
    //
    //   They live here rather than on EditorService because there are now two call sites choosing
    //   the target differently — the File menu from the cursor, the Hierarchy from the header —
    //   and a verb duplicated per call site is a verb that drifts.
    // =============================================================================
    namespace MapOps
    {
        /** The ACTIVE world's Level, or null when there is no world (or a bare one). */
        Level* ActiveLevel(const EditorContext& InContext);

        /**
         * False while PIE runs: the active world is then a Play clone, not the authored map, and
         * writing it back would persist simulation state over the file. The rule is about WHICH
         * WORLD is on screen, which is why it reads the PIE state rather than a flag.
         *
         * @param InVerb Named in the refusal, so a rejected command still says which one it was.
         */
        bool CanEdit(const EditorContext& InContext, const char* InVerb);

        /**
         * Move the editing cursor onto a map that is ALREADY MOUNTED — loads nothing (**MP7**),
         * and the selection survives because none of its entities go anywhere.
         *
         * @param InAssetRelPath ASSET-RELATIVE, the shape the manifest names maps in.
         */
        void Focus(EditorContext& InContext, const OpaaxString& InAssetRelPath);

        /** Write ONE map. Save Level is what writes them all (**MP9**). */
        void Save(EditorContext& InContext, MapId InMapId);

        /** Make InMapId the map every other one composes on top of (**WM1a**). */
        void SetPersistent(EditorContext& InContext, MapId InMapId);

        /**
         * Unmount InMapId and drop it from the manifest. Level::RemoveMap REFUSES the persistent
         * map; the cursor only moves when it was pointing at what just left.
         */
        void RemoveFromLevel(EditorContext& InContext, MapId InMapId);
    }
}
