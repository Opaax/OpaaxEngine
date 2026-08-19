#pragma once

#include "Core/OpaaxTypes.h"                       
#include "Engine/Modules/ModuleRegistrar.h"        
#include "Editor/Extensions/PanelRegistry.h"       
#include "Editor/Extensions/DrawerRegistry.h"      
#include "Editor/Extensions/ResourceTypeRegistry.h"
#include "Editor/Menus/EditorMenu.h"
#include "Editor/Commands/EditorCommandRegistry.h"

namespace Opaax::Editor
{
    // NOTE: `EditorRoute` — the M0 counts-only skeleton every route started life as — is GONE as of
    // M5 S4. Panels() (M2a), Drawers() (M2b), ResourceTypes() (M2d) and EditWorldSystems() (M4 S5)
    // had each graduated to real storage already; Menus() was its last user, so the type left with
    // it. All five routes now do something, and none of them counts for its own sake.

    // =============================================================================
    // EditorExtensionRegistrar — the single object an IEditorModule registers INTO (Editor.md D10).
    //   Owned by EditorService, handed to editor modules BEFORE it seals (§2 ordering), which is before the
    //   first world. Six routes, one per extension kind, and every one of them is REAL.
    //   Symmetric with the game-side ModuleRegistrar (D9).
    //
    //   EditWorldSystems() is the game-side WorldSubsystemRoute REUSED, not a parallel editor type: the
    //   editor module adds Edit-world candidates to the very same WorldSubsystemRegistry the game module
    //   registers into (Editor.md §2). One registry, two routes — which is what makes an editor overlay
    //   and a gameplay system indistinguishable to the World that creates them.
    // =============================================================================
    class EditorExtensionRegistrar
    {
    public:
        DrawerRegistry&            Drawers()          noexcept { return m_Drawers; }
        PanelRegistry&             Panels()           noexcept { return m_Panels; }
        ResourceTypeRegistry&      ResourceTypes()    noexcept { return m_ResourceTypes; }
        EditorMenu&                Menus()            noexcept { return m_Menus; }
        WorldSubsystemRoute&       EditWorldSystems() noexcept { return m_EditWorldSystems; }
        EditorCommandRegistry&     Commands()         noexcept { return m_EditorCommands; }

        const DrawerRegistry&        Drawers()          const noexcept { return m_Drawers; }
        const PanelRegistry&         Panels()           const noexcept { return m_Panels; }
        const ResourceTypeRegistry&  ResourceTypes()    const noexcept { return m_ResourceTypes; }
        const EditorMenu&            Menus()            const noexcept { return m_Menus; }
        const WorldSubsystemRoute&   EditWorldSystems() const noexcept { return m_EditWorldSystems; }
        const EditorCommandRegistry& Commands()         const noexcept { return m_EditorCommands; }

        void Seal()          noexcept { m_Sealed = true; }
        bool IsSealed() const noexcept { return m_Sealed; }

    private:
        DrawerRegistry       m_Drawers;
        PanelRegistry        m_Panels;
        ResourceTypeRegistry m_ResourceTypes;
        EditorMenu           m_Menus;
        WorldSubsystemRoute  m_EditWorldSystems;
        EditorCommandRegistry m_EditorCommands;
        bool                 m_Sealed = false;
    };
}
