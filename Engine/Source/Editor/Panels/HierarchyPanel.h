#pragma once
#include "Scene/SceneManager.h"

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/World/World.h"
#include "Editor/IEditorPanel.h"

namespace Opaax::Editor
{
    /**
     * @class HierarchyPanel
     * Displays all entities in the active scene.
     * Owns the "selected entity" state — other panels read from it via GetSelectedEntity().
     *
     * NOTE: Draw(SceneManager*) is the primary entry point; it is called directly
     * by EditorSubsystem rather than through the IEditorPanel::Draw() interface.
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
         * API call by EditorSubsystem
         * @param InSceneManager 
         */
        void Draw(SceneManager* InSceneManager);

        //------------------------------------------------------------------------------
        // Get - Set
        FORCEINLINE EntityID GetSelectedEntity() const noexcept
        {
            return m_SelectedEntity;
        }

        FORCEINLINE void SetSelectedEntity(EntityID InEntity) noexcept
        {
            m_SelectedEntity = InEntity;
        }

        FORCEINLINE void ClearSelection() noexcept
        {
            m_SelectedEntity = ENTITY_NONE;
        }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IEditorPanel interface
        const char* GetPanelName() const override { return "Hierarchy"; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EntityID m_SelectedEntity = ENTITY_NONE;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR