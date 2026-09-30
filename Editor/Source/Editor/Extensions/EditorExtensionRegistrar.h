#pragma once

#include "Core/OpaaxTypes.h"                       
#include "Engine/Modules/ModuleRegistrar.h"        
#include "Editor/Extensions/PanelRegistry.h"       
#include "Editor/Extensions/DrawerRegistry.h"      
#include "Editor/Extensions/ResourceTypeRegistry.h"
#include "Editor/Extensions/ViewportToolbarRegistry.h"
#include "Editor/TitleBar/TitleBarRegistry.h"
#include "Editor/Commands/EditorCommandRegistry.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorExtensionRegistrar — what an IEditorModule registers into. Owned by EditorService, given
    //   to editor modules before it seals (before the first world). One route per extension kind.
    //   Mirrors the game-side ModuleRegistrar.
    //   EditWorldSystems() is the game-side WorldSubsystemRoute: editor modules add Edit-world systems
    //   to the same WorldSubsystemRegistry the game module uses.
    // =============================================================================
    class EditorExtensionRegistrar
    {
    public:
        ComponentDrawerRegistry&   Drawers()          noexcept { return m_Drawers; }
        UIWidgetDrawerRegistry&    UIWidgetDrawers()  noexcept { return m_UIWidgetDrawers; }
        ConfigDrawerRegistry&      ConfigDrawers()    noexcept { return m_ConfigDrawers; }
        PanelRegistry&             Panels()           noexcept { return m_Panels; }
        ResourceTypeRegistry&      ResourceTypes()    noexcept { return m_ResourceTypes; }
        TitleBarRegistry&          TitleBar()         noexcept { return m_TitleBar; }
        WorldSubsystemRoute&       EditWorldSystems() noexcept { return m_EditWorldSystems; }
        EditorCommandRegistry&     Commands()         noexcept { return m_EditorCommands; }
        ViewportToolbarRegistry&   ViewportTools()    noexcept { return m_ViewportTools; }

        const ComponentDrawerRegistry& Drawers()       const noexcept { return m_Drawers; }
        const UIWidgetDrawerRegistry&  UIWidgetDrawers() const noexcept { return m_UIWidgetDrawers; }
        const ConfigDrawerRegistry&  ConfigDrawers()   const noexcept { return m_ConfigDrawers; }
        const PanelRegistry&         Panels()          const noexcept { return m_Panels; }
        const ResourceTypeRegistry&  ResourceTypes()   const noexcept { return m_ResourceTypes; }
        const TitleBarRegistry&      TitleBar()        const noexcept { return m_TitleBar; }
        const WorldSubsystemRoute&   EditWorldSystems() const noexcept { return m_EditWorldSystems; }
        const EditorCommandRegistry& Commands()        const noexcept { return m_EditorCommands; }
        const ViewportToolbarRegistry& ViewportTools() const noexcept { return m_ViewportTools; }

        void Seal()          noexcept { m_Sealed = true; }
        bool IsSealed() const noexcept { return m_Sealed; }

    private:
        // Instances of one registry type (TDrawerRegistry); only the resolver differs.
        ComponentDrawerRegistry m_Drawers;
        ConfigDrawerRegistry    m_ConfigDrawers;

        // Drawers for UI widgets.
        UIWidgetDrawerRegistry  m_UIWidgetDrawers;
        PanelRegistry        m_Panels;
        ResourceTypeRegistry m_ResourceTypes;

        TitleBarRegistry     m_TitleBar;

        WorldSubsystemRoute  m_EditWorldSystems;
        EditorCommandRegistry m_EditorCommands;

        // The strip over the viewport. An item is a closure (a widget), not a command tag like a menu node.
        ViewportToolbarRegistry m_ViewportTools;

        bool                 m_Sealed = false;
    };
}
