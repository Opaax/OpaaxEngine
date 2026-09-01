#pragma once

#include "Core/OpaaxTypes.h"                       
#include "Engine/Modules/ModuleRegistrar.h"        
#include "Editor/Extensions/PanelRegistry.h"       
#include "Editor/Extensions/DrawerRegistry.h"      
#include "Editor/Extensions/ResourceTypeRegistry.h"
#include "Editor/Extensions/ViewportToolbarRegistry.h"
#include "Editor/Menus/MenuRegistry.h"
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
    //   first world. EIGHT routes, one per extension kind, and every one of them is REAL.
    //   (The count read "six" while there were seven; ③b's ViewportTools makes it eight.)
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
        /**
         * Point Menus() at the gui's tree — the WorldSubsystemRoute shape, and for the same reason:
         * the storage belongs to the CONSUMER (the gui draws it), the route belongs here.
         *
         * Called from EditorService's CONSTRUCTOR, where both objects already exist, so "unbound"
         * is not a reachable state rather than one every caller has to check. MR2a is why that
         * matters: a route that shipped unbound dropped every registration with one Error.
         */
        void BindMenus(MenuRegistry& InMenus) noexcept { m_Menus = &InMenus; }

        ComponentDrawerRegistry&   Drawers()          noexcept { return m_Drawers; }
        ConfigDrawerRegistry&      ConfigDrawers()    noexcept { return m_ConfigDrawers; }
        PanelRegistry&             Panels()           noexcept { return m_Panels; }
        ResourceTypeRegistry&      ResourceTypes()    noexcept { return m_ResourceTypes; }
        MenuRegistry&              Menus()            noexcept { return *m_Menus; }
        WorldSubsystemRoute&       EditWorldSystems() noexcept { return m_EditWorldSystems; }
        EditorCommandRegistry&     Commands()         noexcept { return m_EditorCommands; }
        ViewportToolbarRegistry&   ViewportTools()    noexcept { return m_ViewportTools; }

        const ComponentDrawerRegistry& Drawers()       const noexcept { return m_Drawers; }
        const ConfigDrawerRegistry&  ConfigDrawers()   const noexcept { return m_ConfigDrawers; }
        const PanelRegistry&         Panels()          const noexcept { return m_Panels; }
        const ResourceTypeRegistry&  ResourceTypes()   const noexcept { return m_ResourceTypes; }
        const MenuRegistry&          Menus()           const noexcept { return *m_Menus; }
        const WorldSubsystemRoute&   EditWorldSystems() const noexcept { return m_EditWorldSystems; }
        const EditorCommandRegistry& Commands()        const noexcept { return m_EditorCommands; }
        const ViewportToolbarRegistry& ViewportTools() const noexcept { return m_ViewportTools; }

        void Seal()          noexcept { m_Sealed = true; }
        bool IsSealed() const noexcept { return m_Sealed; }

    private:
        // Two instantiations of ONE registry (TDrawerRegistry), not two registries: a component and
        // a config differ only in how "is this drawer yours?" is answered, which is the resolver's
        // job. The route names stay separate so a call site still reads plainly.
        ComponentDrawerRegistry m_Drawers;
        ConfigDrawerRegistry    m_ConfigDrawers;
        PanelRegistry        m_Panels;
        ResourceTypeRegistry m_ResourceTypes;

        // A ROUTE, not storage: the tree lives on the gui, which is what draws it (MR2e). Bound in
        // EditorService's constructor, before anything can register.
        MenuRegistry*        m_Menus = nullptr;

        WorldSubsystemRoute  m_EditWorldSystems;
        EditorCommandRegistry m_EditorCommands;

        // ③b — the strip over the viewport. An ITEM IS A CLOSURE here where a menu node is a tag,
        // because a toolbar item is a widget rather than an invocation; see the registry's header.
        ViewportToolbarRegistry m_ViewportTools;

        bool                 m_Sealed = false;
    };
}
