#include "Editor/Operation/MapOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorLevelDocument.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/PIE/PlayInEditor.h"

#include "Application/Services/IEngine.h"
#include "Core/Log/Logger.h"
#include "Application/Services/IPaths.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Level.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Components/ComponentRegistry.h"

using namespace Opaax;

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
            // Level structure is saved immediately (see EditorLevelDocument::SaveManifest).
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

        // Clears the undo history: those entities are gone and their map unloaded, so undoing a step
        // that names one would recreate it where no Save can write it.
        InContext.Undo.Clear();

        // Reconcile, not re-adopt: re-adopting would reset every other map's baseline and hide its
        // unsaved edits.
        InContext.LevelDocument.TrackMounted(*lLevel, *InContext.Worlds.GetActiveWorld(),
                                             InContext.Engine.GetRegistries().Components(),
                                             InContext.Paths);

        // The map leaves the level; its file is untouched, so Add Map brings it back.
        InContext.LevelDocument.SaveManifest(*lLevel);

        if (!lWasFocused) { return; }

        // The cursor was on the removed map: move it to one still loaded.
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

        // Nothing to reconcile: the entry never loaded, so it has no baseline and was never focused.
        // Only the level file changes, saved immediately, so the next boot stops warning.
        InContext.LevelDocument.SaveManifest(*lLevel);
    }
}
