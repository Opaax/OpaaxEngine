#pragma once

#include "Editor/UI/IEditorGui.h"
#include "Editor/UI/IEditorUIBackend.h"   // owned through a TUniquePtr — needs the complete type
#include "Core/OpaaxTypes.h"              // TUniquePtr

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
        // Override
        // =============================================================================
    public:
        //~Begin IEditorGui interface
        bool Init(Window& InWindow, OpaaxString InLayoutIniPath) override;
        void Shutdown() override;
        bool IsReady() const noexcept override { return m_Backend != nullptr; }

        void BeginFrame() override;
        void EndFrame() override;
        void Draw(EditorContext& InContext) override;

        bool BeginMainMenuBar() override;
        void EndMainMenuBar() override;
        bool BeginMenu(const char* InLabel, bool bInEnabled) override;
        void EndMenu() override;
        bool MenuItem(const char* InLabel, bool bInChecked, bool bInEnabled) override;
        void MenuSeparator() override;
        bool BeginPanelWindow(const char* InLabel, const PanelWindowStyle& InStyle, bool& bOutWantOpen) override;
        void EndPanelWindow() override;

        double GetTime() const override;
        bool   IsPointerOverUI() const override;
        bool   IsKeyboardOwnedByUI() const override;
        bool   Shortcut(EKeyCode InModifier, EKeyCode InKey) const override;

        IEditorUIBackend& Backend() const noexcept override { return *m_Backend; }
        //~End IEditorGui interface

        using IEditorGui::Shortcut;   // the unmodified overload, hidden by the override above

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<IEditorUIBackend> m_Backend;

        // ImGui stores io.IniFilename as a BORROWED const char* — it never copies the string — so
        // this must stay alive, and unmodified, until DestroyContext() (which saves through that
        // very pointer). Assigned once in Init(); never cleared in Shutdown().
        OpaaxString                  m_LayoutIniPath;
    };
}
