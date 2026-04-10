#pragma once
#include "Scene/SceneManager.h"

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/World/World.h"

namespace Opaax::Editor
{
    // =============================================================================
    // HierarchyPanel
    //
    // Displays all entities in the active scene.
    // Owns the "selected entity" state — other panels read from it.
    //
    // NOTE: HierarchyPanel does not own the World — it holds a raw ptr.
    //   The ptr is refreshed every frame from the active scene.
    //   If no scene is active, the panel renders nothing.
    // =============================================================================
    class HierarchyPanel
    {
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        HierarchyPanel()  = default;
        ~HierarchyPanel() = default;

        // =============================================================================
        // API
        // =============================================================================
    public:
        void Draw(SceneManager* InSceneManager);

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
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
        // Members
        // =============================================================================
    private:
        EntityID m_SelectedEntity = ENTITY_NONE;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR