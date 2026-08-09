#include "Editor/Operation/MapOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorLevelDocument.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/EditorSelection.hpp"
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

        lLevel->SetPersistentMap(InMapId);
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

        // RECONCILE, never re-adopt: a fresh AdoptExisting would re-take every baseline from the
        // world and quietly declare every other map's unsaved edits to be the clean state.
        InContext.LevelDocument.TrackMounted(*lLevel, *InContext.Worlds.GetActiveWorld(),
                                             InContext.Engine.GetRegistries().Components(),
                                             InContext.Paths);

        if (!lWasFocused) { return; }

        // The cursor was on what just left the world. Move it onto something still mounted rather
        // than leaving it pointed at a map nothing can save.
        const TDynArray<Level::MountedMap>& lMounted = lLevel->GetMountedMaps();

        if (lMounted.empty()) { InContext.MapDocument.Clear(); }
        else                  { Focus(InContext, lMounted[0].AssetRelPath); }
    }
}
