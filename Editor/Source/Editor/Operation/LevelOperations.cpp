#include "Editor/Operation/LevelOperations.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorLevelDocument.h"
#include "Editor/EditorMapDocument.h"
#include "Editor/Operation/MapOperations.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"
#include "Application/Services/IPaths.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Level.h"
#include "World/World.h"
#include "World/WorldManager.h"
#include "World/Components/ComponentRegistry.h"

#include <tinyfiledialogs.h>

using namespace Opaax;   // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogLevelOps{"LevelOps"};
}

namespace Opaax::Editor
{
    void LevelOps::AdoptOpen(EditorContext& InContext, const OpaaxString& InLevelAbsPath)
    {
        World* const lWorld = InContext.Worlds.GetActiveWorld();
        Level* const lLevel = MapOps::ActiveLevel(InContext);

        if (lWorld == nullptr || lLevel == nullptr) { return; }

        InContext.LevelDocument.AdoptExisting(InLevelAbsPath, *lLevel, *lWorld,
                                              InContext.Engine.GetRegistries().Components(),
                                              InContext.Paths);

        const TDynArray<Level::MountedMap>& lMounted = lLevel->GetMountedMaps();
        if (lMounted.empty())
        {
            InContext.MapDocument.Clear();
            OPAAX_LOG(LogLevelOps, Info, "World '{}' has no map mounted — nothing to edit",
                      lWorld->GetName().CStr());
            return;
        }

        const MapId lPersistent = lLevel->GetPersistentMapId();
        const Level::MountedMap* lEdited = &lMounted[0];

        for (const Level::MountedMap& lCandidate : lMounted)
        {
            if (lCandidate.Id != lPersistent)
            {
                lEdited = &lCandidate;
                break;
            }
        }

        OPAAX_LOG(LogLevelOps, Info, "Level '{}': {} map(s) mounted, focused on '{}'",
                  lLevel->GetData().Name.CStr(), lMounted.size(), lEdited->AssetRelPath.CStr());

        InContext.MapDocument.Focus(InContext.Paths.AssetToAbsolute(lEdited->AssetRelPath));
    }

    bool LevelOps::ConfirmDiscardingEdits(EditorContext& InContext)
    {
        World* const lWorld = InContext.Worlds.GetActiveWorld();
        Level* const lLevel = MapOps::ActiveLevel(InContext);

        if (lWorld == nullptr || lLevel == nullptr) { return true; }

        if (!InContext.LevelDocument.IsDirty(*lWorld, InContext.Engine.GetRegistries().Components(), *lLevel))
        {
            return true;
        }

        // UNSAVED WORK IS CONFIRMED, NOT DISCARDED. tinyfiledialogs is already the editor's
        // file-dialog vendor, so the modal costs no new dependency.
        const int lAnswer = tinyfd_messageBox(
            "Unsaved changes",
            "This level has unsaved changes.\nContinue and lose them?",
            "yesno", "warning", /*defaultButton*/0); // default NO — the safe answer

        if (lAnswer != 1)
        {
            OPAAX_LOG(LogLevelOps, Info, "Cancelled — unsaved changes kept");
            return false;
        }

        return true;
    }
}
