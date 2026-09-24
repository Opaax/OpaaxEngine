#pragma once

#include "Editor/Imgui/ImGuiEditorWidgets.h"   // held by value — needs the complete type
#include "Editor/Imgui/ImGuiTitleBar.h"        // likewise
#include "Editor/UI/IEditorGui.h"
#include "Editor/UI/IEditorUIBackend.h"   // owned through a TUniquePtr — needs the complete type
#include "Core/OpaaxTypes.h"              // TUniquePtr

namespace Opaax {
    struct EditorImguiConfigData;
}

namespace Opaax::Editor
{
    // =============================================================================
    // ImGuiEditorGui — the ImGui implementation of IEditorGui: the context, the impl backends, the
    //   frame, the dockspace and the UI pass. The one place in the editor that names ImGui for host
    //   chrome; a panel's contents are its own (MR2c).
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

        // Owns the UI backend through a TUniquePtr (I6's corollary: an owner of a move-only member
        // must say so, or the implicit copy is instantiated anyway).
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

        // The menu tree and the panel set are m_Menu/m_Panels on IEditorGui — bound by EditorService,
        // not looked up per frame.

        // The editor's own caption. Composed into the host window's menu bar by Draw; it holds the
        // live border-drag state, which is why it is an object rather than a free function.
        // The ImGui SIDE of the caption. Named apart from the base's m_TitleBar
        // (EditorTitleBar, the backend-agnostic one) so neither shadows the other.
        ImGuiTitleBar                m_ImGuiTitleBar;

        // Stateless; held by value because the gui is what a caller reaches it through.
        ImGuiEditorWidgets           m_Widgets;

        // ImGui stores io.IniFilename as a BORROWED const char* — it never copies the string — so
        // this must stay alive, and unmodified, until DestroyContext() (which saves through that
        // very pointer). Assigned once in Init(); never cleared in Shutdown().
        OpaaxString                  m_LayoutIniPath;
    };
}
