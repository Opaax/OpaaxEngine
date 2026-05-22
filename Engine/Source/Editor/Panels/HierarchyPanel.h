#pragma once
#include "Scene/SceneManager.h"

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "World/World.h"
#include "Editor/EditorEventBus.h"
#include "Editor/IEditorPanel.h"

namespace Opaax::Editor
{
    /**
     * @class HierarchyPanel
     * Displays all entities in the active scene.
     * Owns the "selected entity" state — publishes OnEntitySelectedEvent on every
     * change. Other panels subscribe via EditorEventBus (see Inspector).
     *
     * NOTE: Draw(Scene*, World*) is the primary entry point; it is called directly
     * by EditorSubsystem rather than through the IEditorPanel::Draw() interface.
     * Caller supplies the active scene (for name + null-state) and the engine's
     * shared World (entity source).
     */
    class HierarchyPanel : public IEditorPanel
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        HierarchyPanel()  = default;
        ~HierarchyPanel() = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:
        /**
         * API call by EditorSubsystem.
         * @param InActiveScene active scene (null → render placeholder).
         * @param InWorld       engine-shared World (must be non-null when InActiveScene is non-null).
         */
        void Draw(Scene* InActiveScene, World* InWorld);

        //------------------------------------------------------------------------------
        // Set — publish OnEntitySelectedEvent via the bus captured in OnSubscribe.
        void SetSelectedEntity(EntityID InEntity);
        void ClearSelection();

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IEditorPanel interface
        void        OnSubscribe(EditorEventBus& InBus) override;
        const char* GetPanelName() const override { return "Hierarchy"; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Single mutation funnel — early-outs on same-value, then publishes. */
        void SetSelection(EntityID InEntity);

        EntityID          m_SelectedEntity = ENTITY_NONE;
        EditorEventBus*   m_Bus            = nullptr; // captured in OnSubscribe; non-owning
        SubscriptionToken m_NewSceneToken;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR