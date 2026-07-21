#pragma once

#include "Core/OpaaxTypes.h"   // Uint64

namespace Opaax::Editor
{
    // =============================================================================
    // EditorRoute — one editor-extension channel exposed by EditorExtensionRegistrar (Editor.md D10).
    //
    // M0 SKELETON: Register(...) only records that an extension was offered (a count), so the boot
    // ordering (engine natives -> game module -> editor module -> seal) is observable and testable before
    // the real drawer/panel/asset machinery exists. The call-site API is the FINAL shape now; only the
    // bodies change later (M2 drawers/panels/assets, M4 edit-world-systems, M5 menus). Mirrors ModuleRoute
    // (Application/ModuleRegistrar.h). The three Register overloads cover D10's call-site shapes:
    //   Drawers().Register<TComponent, TDrawer>();      AssetTypes().Register<TAsset, TActions>();
    //   EditWorldSystems().Register<TSystem>();
    //   Panels().Register("Wave Designer", factory);     Menus().Register("Tools/Validate", command);
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
        EditorRoute&       Drawers()                noexcept { return m_Drawers; }
        EditorRoute&       Panels()                 noexcept { return m_Panels; }
        EditorRoute&       AssetTypes()             noexcept { return m_AssetTypes; }
        EditorRoute&       Menus()                  noexcept { return m_Menus; }
        EditorRoute&       EditWorldSystems()       noexcept { return m_EditWorldSystems; }

        const EditorRoute& Drawers()          const noexcept { return m_Drawers; }
        const EditorRoute& Panels()           const noexcept { return m_Panels; }
        const EditorRoute& AssetTypes()       const noexcept { return m_AssetTypes; }
        const EditorRoute& Menus()            const noexcept { return m_Menus; }
        const EditorRoute& EditWorldSystems() const noexcept { return m_EditWorldSystems; }
        
        void Seal()          noexcept { m_Sealed = true; }
        bool IsSealed() const noexcept { return m_Sealed; }

    private:
        EditorRoute m_Drawers;
        EditorRoute m_Panels;
        EditorRoute m_AssetTypes;
        EditorRoute m_Menus;
        EditorRoute m_EditWorldSystems;
        bool        m_Sealed = false;
    };
}
