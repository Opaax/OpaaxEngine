#include "Editor/Operation/MapOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorLevelDocument.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/PIE/PlayInEditor.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IPaths.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Level.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Components/ComponentRegistry.h"

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogMapOps{"MapOps"};
}

namespace Opaax::Editor
{
    Level* MapOps::ActiveLevel(const EditorContext& InContext)
    {
        World* const lWorld = InContext.Worlds.GetActiveWorld();
        return lWorld != nullptr ? lWorld->GetLevel() : nullptr;
    }

    bool MapOps::CanEdit(const EditorContext& InContext, const char* InVerb)
    {
        if (InContext.PIE.IsEdit() && InContext.Worlds.GetActiveWorld() != nullptr)
        {
            return true;
        }

        OPAAX_LOG(LogMapOps, Warn, "{} ignored — stop the PIE session first", InVerb);
        return false;
    }

    void MapOps::Focus(EditorContext& InContext, const OpaaxString& InAssetRelPath)
    {
        if (!CanEdit(InContext, "Edit Map")) { return; }

        InContext.MapDocument.Focus(InContext.Paths.AssetToAbsolute(InAssetRelPath));
    }

    void MapOps::Save(EditorContext& InContext, MapId InMapId)
    {
        if (!CanEdit(InContext, "Save Map")) { return; }

        InContext.LevelDocument.SaveMap(InMapId, *InContext.Worlds.GetActiveWorld(),
                                        InContext.Engine.GetRegistries().Components());
    }

    void MapOps::SetPersistent(EditorContext& InContext, MapId InMapId)
    {
        if (!CanEdit(InContext, "Set Persistent Map")) { return; }

        Level* const lLevel = ActiveLevel(InContext);
        if (lLevel == nullptr) { return; }

        if (lLevel->SetPersistentMap(InMapId))
        {
            // STRUCTURE GOES TO DISK AS IT CHANGES — see EditorLevelDocument::SaveManifest. Which
            // map is the backdrop is a deliberate one-off choice, not an edit that wants batching.
            InContext.LevelDocument.SaveManifest(*lLevel);
        }
    }

    void MapOps::RemoveFromLevel(EditorContext& InContext, MapId InMapId)
    {
        if (!CanEdit(InContext, "Remove Map")) { return; }

        Level* const lLevel = ActiveLevel(InContext);
        if (lLevel == nullptr) { return; }

        const bool lWasFocused = (InContext.MapDocument.GetMapId() == InMapId);

        // Its entities are about to go, and the selection may be pointing at one of them.
        InContext.Selection.Clear();

        if (!lLevel->RemoveMap(InMapId)) { return; }

        // THE ONE STRUCTURAL CHANGE THAT INVALIDATES HISTORY (⑤): those entities are gone AND their
        // map is unmounted, so undoing a step that names one would recreate it into a world no Save
        // can write it from (**WM2**). Adding a map or choosing the persistent one touches no
        // recorded entity, so neither clears.
        InContext.Undo.Clear();

        // RECONCILE, never re-adopt: a fresh AdoptExisting would re-take every baseline from the
        // world and quietly declare every other map's unsaved edits to be the clean state.
        InContext.LevelDocument.TrackMounted(*lLevel, *InContext.Worlds.GetActiveWorld(),
                                             InContext.Engine.GetRegistries().Components(),
                                             InContext.Paths);

        // The map is out of the level; the FILE is untouched, so Add Map puts it back.
        InContext.LevelDocument.SaveManifest(*lLevel);

        if (!lWasFocused) { return; }

        // The cursor was on what just left the world. Move it onto something still mounted rather
        // than leaving it pointed at a map nothing can save.
        const TDynArray<Level::MountedMap>& lMounted = lLevel->GetMountedMaps();

        if (lMounted.empty()) { InContext.MapDocument.Clear(); }
        else                  { Focus(InContext, lMounted[0].AssetRelPath); }
    }

    void MapOps::RemoveMissingFromLevel(EditorContext& InContext, const OpaaxString& InAssetRelPath)
    {
        if (!CanEdit(InContext, "Remove Missing Map")) { return; }

        Level* const lLevel = ActiveLevel(InContext);
        if (lLevel == nullptr) { return; }

        if (!lLevel->RemoveMissingMap(InAssetRelPath)) { return; }

        // NOTHING to reconcile and no cursor to move: the entry never mounted, so no baseline was
        // taken for it and it can never have been focused. Only the manifest changed — and that
        // goes to disk as it changes (EditorLevelDocument::SaveManifest), which is the whole point:
        // the next boot opens cleanly instead of warning again.
        InContext.LevelDocument.SaveManifest(*lLevel);
    }
}
