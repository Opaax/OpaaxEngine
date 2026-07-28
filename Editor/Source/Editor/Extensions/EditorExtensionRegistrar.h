#pragma once

#include "Core/OpaaxTypes.h"                         // Uint64
#include "Editor/Extensions/PanelRegistry.h"         // Panels() graduated from EditorRoute to real storage (M2a)
#include "Editor/Extensions/DrawerRegistry.h"        // Drawers() likewise (M2b)
#include "Editor/Extensions/ResourceTypeRegistry.h"  // ResourceTypes() likewise (M2d)

namespace Opaax::Editor
{
    // =============================================================================
    // EditorRoute — one editor-extension channel exposed by EditorExtensionRegistrar (Editor.md D10).
    //
    // M0 SKELETON: Register(...) only records that an extension was offered (a count), so the boot
    // ordering (engine natives -> game module -> editor module -> seal) is observable and testable before
    // the real machinery exists. Mirrors ModuleRoute (Application/ModuleRegistrar.h). The remaining
    // Register overloads cover D10's call-site shapes:
    //   EditWorldSystems().Register<TSystem>();        Menus().Register("Tools/Validate", command);
    //
    // Panels() (M2a), Drawers() (M2b) and ResourceTypes() (M2d) have GRADUATED off this type — see
    // PanelRegistry / DrawerRegistry / ResourceTypeRegistry. Each remaining route leaves the same way, in
    // the slice that gives it a real consumer; when the last one goes (M4 / M5), EditorRoute goes with it.
    // =============================================================================
    class EditorRoute
    {
    public:
        template<typename T>             void Register() noexcept { ++m_Count; }   // <TSystem>
        template<typename A, typename B> void Register() noexcept { ++m_Count; }   // <TComponent,TDrawer> / <TAsset,TActions>

        template<typename TFactory>
        void Register(const char* /*InName*/, TFactory&& /*InFactory*/) noexcept { ++m_Count; }   // (name, factory/command)

        Uint64 Count() const noexcept { return m_Count; }

    private:
        Uint64 m_Count = 0;
    };

    // =============================================================================
    // EditorExtensionRegistrar — the single object an IEditorModule registers INTO (Editor.md D10).
    //   Owned by EditorService, handed to editor modules BEFORE it seals (§2 ordering), which is before the
    //   first world. Five routes, one per extension kind; real storage lands M2/M4/M5. Symmetric with the
    //   game-side ModuleRegistrar (D9).
    // =============================================================================
    class EditorExtensionRegistrar
    {
    public:
        DrawerRegistry&       Drawers()                noexcept { return m_Drawers; }
        PanelRegistry&        Panels()                 noexcept { return m_Panels; }
        ResourceTypeRegistry& ResourceTypes()          noexcept { return m_ResourceTypes; }
        EditorRoute&          Menus()                  noexcept { return m_Menus; }
        EditorRoute&          EditWorldSystems()       noexcept { return m_EditWorldSystems; }

        const DrawerRegistry&       Drawers()       const noexcept { return m_Drawers; }
        const PanelRegistry&        Panels()        const noexcept { return m_Panels; }
        const ResourceTypeRegistry& ResourceTypes() const noexcept { return m_ResourceTypes; }
        const EditorRoute&   Menus()            const noexcept { return m_Menus; }
        const EditorRoute&   EditWorldSystems() const noexcept { return m_EditWorldSystems; }

        void Seal()          noexcept { m_Sealed = true; }
        bool IsSealed() const noexcept { return m_Sealed; }

    private:
        DrawerRegistry       m_Drawers;
        PanelRegistry        m_Panels;
        ResourceTypeRegistry m_ResourceTypes;
        EditorRoute          m_Menus;
        EditorRoute          m_EditWorldSystems;
        bool                 m_Sealed = false;
    };
}
