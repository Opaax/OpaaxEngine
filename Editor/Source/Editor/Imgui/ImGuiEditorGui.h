#pragma once

#include "Editor/Imgui/ImGuiEditorWidgets.h"   // held by value (needs the complete type)
#include "Editor/Imgui/ImGuiTitleBar.h"        // same
#include "Editor/UI/IEditorGui.h"
#include "Editor/UI/IEditorUIBackend.h"   // owned by TUniquePtr (needs the complete type)
#include "Core/OpaaxTypes.h"              // TUniquePtr

namespace Opaax {
    struct EditorImguiConfigData;
}

namespace Opaax::Editor
{
    // =============================================================================
    // ImGuiEditorGui — the ImGui implementation of IEditorGui: the context, the backends, the frame,
    //   the dockspace and the UI pass.
    // =============================================================================
    class ImGuiEditorGui final : public IEditorGui
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        ImGuiEditorGui()           = default;
        ~ImGuiEditorGui() override = default;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================

        // Owns the UI backend through a TUniquePtr (non-copyable).
        ImGuiEditorGui(const ImGuiEditorGui&)            = delete;
        ImGuiEditorGui& operator=(const ImGuiEditorGui&) = delete;
        
        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void CheckStyle();
        void UpdateStyle(const EditorImguiConfigData& InCFG);
        
        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorGui interface
        bool Init(Window& InWindow, OpaaxString InLayoutIniPath) override;
        void SetUIFont(const EditorUIFont& InFont) override;
        void Shutdown() override;
        bool IsReady() const noexcept override { return m_Backend != nullptr; }

        void BeginFrame() override;
        void EndFrame() override;
        void Draw(EditorContext& InContext) override;

        bool BeginMenu(const char* InLabel, bool bInEnabled) override;
        void EndMenu() override;
        bool MenuItem(const char* InLabel, bool bInChecked, bool bInEnabled) override;
        void MenuSeparator() override;
        TitleBarDrag TitleBarDragRegion(Uint32 InTrailingButtons) override;
        bool TitleBarButton(EWindowButtonKind InKind) override;
        bool BeginPanelWindow(const char* InLabel, const PanelWindowStyle& InStyle, bool& bOutWantOpen) override;
        bool IsPanelWindowFocused() const override;
        void EndPanelWindow() override;

        double GetTime() const override;
        bool   IsPointerOverUI() const override;
        bool   IsKeyboardOwnedByUI() const override;
        bool   Shortcut(EKeyCode InModifier, EKeyCode InKey) const override;

        IEditorUIBackend& Backend() const noexcept override { return *m_Backend; }
        IEditorWidgets&   Widgets() noexcept override { return m_Widgets; }
        //~End IEditorGui interface

        using IEditorGui::Shortcut;   // the unmodified overload, hidden by the override above

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<IEditorUIBackend> m_Backend;

        // The title bar and the panels are m_TitleBar/m_Panels on IEditorGui, bound by EditorService.

        // The ImGui side of the caption (holds the border-drag state). Named apart from the base's
        // m_TitleBar so neither hides the other.
        ImGuiTitleBar                m_ImGuiTitleBar;

        // Stateless; held by value.
        ImGuiEditorWidgets           m_Widgets;

        // ImGui keeps io.IniFilename as a borrowed pointer, so this must stay alive and unchanged until
        // DestroyContext() (which saves through it). Set once in Init(); never cleared.
        OpaaxString                  m_LayoutIniPath;
    };
}
