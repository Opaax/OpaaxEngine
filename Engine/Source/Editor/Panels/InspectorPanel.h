#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "World/World.h"
#include "Editor/EditorEventBus.h"
#include "Editor/IEditorPanel.h"

namespace Opaax::Editor
{
    /**
     * @class InspectorPanel
     * Displays and edits components of the currently selected entity.
     *
     * Selection arrives via OnEntitySelectedEvent on the EditorEventBus (subscribed
     * in OnSubscribe). The panel caches the selection in m_SelectedEntity and reads
     * from the cache each frame — Draw takes only the World.
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
         * API — parameterised draw called directly by EditorSubsystem.
         * @param InWorld engine-shared World (entity source).
         */
        void Draw(World* InWorld);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin  IEditorPanel interface
        void        OnSubscribe(EditorEventBus& InBus) override;
        const char* GetPanelName() const override { return "Inspector"; }
        //~End  IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EntityID          m_SelectedEntity = ENTITY_NONE;
        SubscriptionToken m_SelectionToken;
        SubscriptionToken m_NewSceneToken;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
