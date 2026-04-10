#pragma once
#if OPAAX_WITH_EDITOR
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
    // =============================================================================
    // EditorSubsystem
    //
    // Owns the ImGui context lifetime.
    // Drives the per-frame ImGui Begin/End cycle.
    //
    // Lifecycle:
    //   Startup()  — ImGui context creation, GLFW + OpenGL3 backend init.
    //   Shutdown() — ImGui context destruction.
    //   Update()   — nothing (editor is render-driven, not update-driven).
    //   Render()   — NewFrame → all panels → Render → swap.
    //
    // NOTE: EditorSubsystem must be registered AFTER RenderSubsystem —
    //   glad must be loaded before ImGui OpenGL backend initializes.
    //
    // NOTE: All editor panels are driven from Render().
    //   Panels are not subsystems — they are owned by EditorSubsystem directly.
    // =============================================================================
    class OPAAX_API EditorSubsystem final : public EngineSubsystemBase
    {
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(EditorSubsystem)

    public:
        EditorSubsystem() = default;
        explicit EditorSubsystem(CoreEngineApp* InEngineApp)
            : EngineSubsystemBase(InEngineApp)
        {}
        ~EditorSubsystem() override = default;

        EditorSubsystem(const EditorSubsystem&)            = delete;
        EditorSubsystem& operator=(const EditorSubsystem&) = delete;
        EditorSubsystem(EditorSubsystem&&)                 = default;
        EditorSubsystem& operator=(EditorSubsystem&&)      = default;

    public:
        bool Startup()            override;
        void Shutdown()           override;
        void Render(double Alpha) override;

        Uint32 GetEventCategoryFilter() const noexcept override
        {
            return EEventCategory_None;
        }

        // Viewport FBO accessors — used by MyGame::OnRender
        FORCEINLINE Editor::ViewportPanel& GetViewport() noexcept { return m_ViewportPanel; }
        FORCEINLINE bool IsPlaying() const noexcept { return m_PlayStopPanel.IsPlaying(); }

    private:
        void BeginFrame();
        void EndFrame();
        void DrawPanels();

        Editor::HierarchyPanel   m_HierarchyPanel;
        Editor::InspectorPanel   m_InspectorPanel;
        Editor::ViewportPanel    m_ViewportPanel;
        Editor::AssetBrowserPanel m_AssetBrowserPanel;
        Editor::PlayStopPanel    m_PlayStopPanel;
    };

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR