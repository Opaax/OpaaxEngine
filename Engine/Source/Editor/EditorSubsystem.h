#pragma once
#if OPAAX_WITH_EDITOR

#include "Editor/IEditorPanel.h"
#include "Panels/AssetBrowserPanel.h"
#include "Panels/HierarchyPanel.h"
#include "Panels/InspectorPanel.h"
#include "Panels/PlayStopPanel.h"
#include "Panels/ViewportPanel.h"
#include "Core/Event/OpaaxEventTypes.hpp"
#include "Core/Systems/EngineSubsystem.h"


struct GLFWwindow;


namespace Opaax
{
    /**
     * @class EditorSubsystem
     * Owns the ImGui context lifetime and drives the per-frame Begin/End cycle.
     * Registers all IAssetTypeActions and IComponentDrawers at Startup.
     */
    class OPAAX_API EditorSubsystem final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(EditorSubsystem)

        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        EditorSubsystem() = default;
        explicit EditorSubsystem(CoreEngineApp* InEngineApp)
            : EngineSubsystemBase(InEngineApp)
        {}
        ~EditorSubsystem() override = default;

        // =============================================================================
        // Copy - Delete
        // =============================================================================
        EditorSubsystem(const EditorSubsystem&)            = delete;
        EditorSubsystem& operator=(const EditorSubsystem&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        EditorSubsystem(EditorSubsystem&&)                 = default;
        EditorSubsystem& operator=(EditorSubsystem&&)      = default;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void BeginFrame();
        void EndFrame();
        void DrawPanels();

        void RegisterViewportCallbacks();
        void RegisterAssetTypeActions();
        void RegisterComponentDrawers();

        //------------------------------------------------------------------------------
        // Get
    public:
        FORCEINLINE IRenderTarget& GetRenderTarget() noexcept { return m_ViewportPanel; }
        FORCEINLINE bool IsPlaying() const noexcept { return m_PlayStopPanel.IsPlaying(); }

        // =============================================================================
        // Override EngineSubsystemBase
        // =============================================================================
        //~Begin EngineSubsystemBase interface
    public:
        bool Startup()            override;
        void Shutdown()           override;
        void Render(double Alpha) override;

        Uint32 GetEventCategoryFilter() const noexcept override { return EEventCategory_None; }
        //~End EngineSubsystemBase interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Editor::HierarchyPanel    m_HierarchyPanel;
        Editor::InspectorPanel    m_InspectorPanel;
        Editor::ViewportPanel     m_ViewportPanel;
        Editor::AssetBrowserPanel m_AssetBrowserPanel;
        Editor::PlayStopPanel     m_PlayStopPanel;
    };

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR
