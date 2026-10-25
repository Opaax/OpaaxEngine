#pragma once

#include "Core/OpaaxTypes.h"                       
#include "Editor/Extensions/PanelRegistry.h"       
#include "Editor/Extensions/DrawerRegistry.h"      
#include "Editor/Extensions/ResourceTypeRegistry.h"
#include "Editor/Extensions/AssetTypeRegistry.h"
#include "Editor/Extensions/ViewportToolbarRegistry.h"
#include "Editor/TitleBar/TitleBarRegistry.h"
#include "Editor/Commands/EditorCommandRegistry.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorExtensionRegistrar — what an IEditorModule registers into. Owned by EditorService, given
    //   to editor modules before it seals (before the first world). One route per extension kind.
    //   Mirrors the game-side ModuleRegistrar. An Edit-world subsystem is an Object class like any
    //   other: it registers with OPAAX_REGISTER_OBJECT and its ShouldCreate picks Edit worlds.
    // =============================================================================
    class EditorExtensionRegistrar
    {
    public:
        ComponentDrawerRegistry&   Drawers()          noexcept { return m_Drawers; }
        PanelRegistry&             Panels()           noexcept { return m_Panels; }
        ResourceTypeRegistry&      ResourceTypes()    noexcept { return m_ResourceTypes; }
        AssetTypeRegistry&         AssetTypes()       noexcept { return m_AssetTypes; }
        TitleBarRegistry&          TitleBar()         noexcept { return m_TitleBar; }
        EditorCommandRegistry&     Commands()         noexcept { return m_EditorCommands; }
        ViewportToolbarRegistry&   ViewportTools()    noexcept { return m_ViewportTools; }

        const ComponentDrawerRegistry& Drawers()       const noexcept { return m_Drawers; }
        const PanelRegistry&         Panels()          const noexcept { return m_Panels; }
        const ResourceTypeRegistry&  ResourceTypes()   const noexcept { return m_ResourceTypes; }
        const AssetTypeRegistry&     AssetTypes()      const noexcept { return m_AssetTypes; }
        const TitleBarRegistry&      TitleBar()        const noexcept { return m_TitleBar; }
        const EditorCommandRegistry& Commands()        const noexcept { return m_EditorCommands; }
        const ViewportToolbarRegistry& ViewportTools() const noexcept { return m_ViewportTools; }

        void Seal()          noexcept { m_Sealed = true; }
        bool IsSealed() const noexcept { return m_Sealed; }

    private:
        ComponentDrawerRegistry m_Drawers;
        PanelRegistry        m_Panels;
        ResourceTypeRegistry m_ResourceTypes;

        // How each asset class looks and what edits it (its window's body).
        AssetTypeRegistry    m_AssetTypes;

        TitleBarRegistry     m_TitleBar;

        EditorCommandRegistry m_EditorCommands;

        // The strip over the viewport. An item is a closure (a widget), not a command tag like a menu node.
        ViewportToolbarRegistry m_ViewportTools;

        bool                 m_Sealed = false;
    };
}
