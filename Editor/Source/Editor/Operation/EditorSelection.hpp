#pragma once

#include "World/Entity/Entity.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorSelection — the editor's single selected entity (Editor.md D3 growth). Owned by
    //   EditorService, referenced by EditorContext so panels/drawers read and write it without a
    //   locator. Hierarchy (M2a) is the first writer; Inspector (M2b) reads it.
    //
    //   A default-constructed Entity is already the invalid state (IsValid() == m_World != nullptr
    //   && m_World->IsValid(m_Handle)), so no separate sentinel is needed.
    // =============================================================================
    class EditorSelection
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void   Select(Entity InEntity) noexcept { m_Selected = InEntity; }
        void   Clear()                 noexcept { m_Selected = Entity{}; }
        Entity Get()             const noexcept { return m_Selected; }
        bool   HasSelection()    const noexcept { return m_Selected.IsValid(); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Entity holds a raw World*, so this is invalidated from outside: EditorService subscribes to
        // WorldManager and retargets (by GUID) or clears on every world change.
        Entity m_Selected;
    };
}
