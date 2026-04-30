#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/World/World.h"
#include "Editor/IEditorPanel.h"

namespace Opaax::Editor
{
    /**
     * @class InspectorPanel
     * Displays and edits components of the selected entity.
     *
     * Extension: register an IComponentDrawer via ComponentDrawerRegistry::Register()
     * to support a new component type. No changes required here.
     */
    class InspectorPanel : public IEditorPanel
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        InspectorPanel()  = default;
        ~InspectorPanel() = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:
        /**
         * API — parameterised draw called directly by EditorSubsystem
         * @param InWorld 
         * @param InSelected 
         */
        void Draw(World* InWorld, EntityID InSelected);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin  IEditorPanel interface
        const char* GetPanelName() const override { return "Inspector"; }
        //~End  IEditorPanel interface
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
