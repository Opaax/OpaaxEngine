#pragma once

#include "Core/OpaaxTypes.h"                         // Uint64
#include "Engine/Modules/ModuleRegistrar.h"          // WorldSubsystemRoute — EditWorldSystems() reuses it (M4 S5)
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
    // the real machinery exists. Mirrors ModuleRoute (Application/ModuleRegistrar.h).
    //
    // Panels() (M2a), Drawers() (M2b), ResourceTypes() (M2d) and EditWorldSystems() (M4 S5) have all
    // GRADUATED off this type. **Menus() is the LAST user**, and it leaves with M5 — at which point this
    // class goes with it. Only its call-site shape survives here; the <T> / <A,B> overloads left with the
    // routes that used them.
    // =============================================================================
    class EditorRoute
    {
    public:
        template<typename TFactory>
        void Register(const char* /*InName*/, TFactory&& /*InFactory*/) noexcept { ++m_Count; }   // (name, factory/command)

        Uint64 Count() const noexcept { return m_Count; }

    private:
        Uint64 m_Count = 0;
    };

    // =============================================================================
    // EditorExtensionRegistrar — the single object an IEditorModule registers INTO (Editor.md D10).
    //   Owned by EditorService, handed to editor modules BEFORE it seals (§2 ordering), which is before the
    //   first world. Five routes, one per extension kind; only Menus() is still a skeleton (M5).
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
        EditorRoute&               Menus()            noexcept { return m_Menus; }
        WorldSubsystemRoute&       EditWorldSystems() noexcept { return m_EditWorldSystems; }

        const DrawerRegistry&       Drawers()          const noexcept { return m_Drawers; }
        const PanelRegistry&        Panels()           const noexcept { return m_Panels; }
        const ResourceTypeRegistry& ResourceTypes()    const noexcept { return m_ResourceTypes; }
        const EditorRoute&          Menus()            const noexcept { return m_Menus; }
        const WorldSubsystemRoute&  EditWorldSystems() const noexcept { return m_EditWorldSystems; }

        void Seal()          noexcept { m_Sealed = true; }
        bool IsSealed() const noexcept { return m_Sealed; }

    private:
        DrawerRegistry       m_Drawers;
        PanelRegistry        m_Panels;
        ResourceTypeRegistry m_ResourceTypes;
        EditorRoute          m_Menus;
        WorldSubsystemRoute  m_EditWorldSystems;
        bool                 m_Sealed = false;
    };
}
