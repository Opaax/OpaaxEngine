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

#include "Editor/UI/IEditorDialogs.h"

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

    void LevelOps::ConfirmDiscardingEdits(EditorContext& InContext, TFunction<void()> InOnConfirmed)
    {
        if (!InOnConfirmed) { return; }

        World* const lWorld = InContext.Worlds.GetActiveWorld();
        Level* const lLevel = MapOps::ActiveLevel(InContext);

        // Nothing at risk — no dialog at all, which is why the continuation is the only route
        // through here rather than one branch of two.
        if (lWorld == nullptr || lLevel == nullptr
            || !InContext.LevelDocument.IsDirty(*lWorld, InContext.Engine.GetRegistries().Components(), *lLevel))
        {
            InOnConfirmed();
            return;
        }

        // UNSAVED WORK IS CONFIRMED, NOT DISCARDED.
        InContext.Dialogs.Confirm(
            OpaaxString("Unsaved changes"),
            OpaaxString("This level has unsaved changes.\nContinue and lose them?"),
            [InOnConfirmed = Move(InOnConfirmed)](const EDialogAnswer InAnswer)
            {
                if (InAnswer != EDialogAnswer::Yes)
                {
                    OPAAX_LOG(LogLevelOps, Info, "Cancelled — unsaved changes kept");
                    return;
                }

                InOnConfirmed();
            });
    }
}
