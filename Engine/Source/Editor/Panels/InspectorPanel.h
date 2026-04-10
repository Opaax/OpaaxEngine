#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/World/World.h"

namespace Opaax::Editor
{
    // =============================================================================
    // InspectorPanel
    //
    // Displays and edits components of the selected entity.
    // Reads selected entity from HierarchyPanel each frame.
    //
    // NOTE: Each component type has a dedicated DrawXxx() method.
    //   Adding a new component type = adding a DrawXxx() + a check in Draw().
    // =============================================================================
    class InspectorPanel
    {
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        InspectorPanel()  = default;
        ~InspectorPanel() = default;

        // =============================================================================
        // API
        // =============================================================================
    public:
        void Draw(World* InWorld, EntityID InSelected);

        // =============================================================================
        // Private — per-component drawers
        // =============================================================================
    private:
        void DrawTagComponent       (World& InWorld, EntityID InEntity);
        void DrawTransformComponent (World& InWorld, EntityID InEntity);
        void DrawSpriteComponent    (World& InWorld, EntityID InEntity);
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR