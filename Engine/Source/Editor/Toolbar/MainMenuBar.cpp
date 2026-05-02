#include "MainMenuBar.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#include "Editor/EditorState.h"
#include "Editor/EditorSubsystem.h"
#include "Core/CoreEngineApp.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    // TODO: M8 polish — wire CMake-generated version + commit hash.
    static constexpr const char* OPAAX_ENGINE_VERSION = "0.1.0-dev";
    static constexpr const char* ABOUT_POPUP_ID       = "About OpaaxEngine";

    // =============================================================================
    // Draw
    // =============================================================================
    void MainMenuBar::Draw(EditorSubsystem& Owner)
    {
        if (ImGui::BeginMainMenuBar())
        {
            DrawFileMenu(Owner);
            DrawEditMenu();
            DrawViewMenu(Owner);
            DrawWindowMenu();
            DrawHelpMenu();

            DrawPieControls(Owner);

            ImGui::EndMainMenuBar();
        }

        DrawAboutModal();
    }

    // =============================================================================
    // File
    // =============================================================================
    void MainMenuBar::DrawFileMenu(EditorSubsystem& Owner)
    {
        if (!ImGui::BeginMenu("File")) { return; }

        if (ImGui::MenuItem("New Scene"))
        {
            OPAAX_CORE_INFO("MainMenuBar: New Scene — TBD M2 Step 3");
        }

        if (ImGui::MenuItem("Open Scene..."))
        {
            OPAAX_CORE_INFO("MainMenuBar: Open Scene — TBD M2 Step 3");
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
        {
            OPAAX_CORE_INFO("MainMenuBar: Save Scene — TBD M2 Step 3");
        }

        if (ImGui::MenuItem("Save Scene As..."))
        {
            OPAAX_CORE_INFO("MainMenuBar: Save Scene As — TBD M2 Step 3");
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Recent", false /* enabled */))
        {
            // Recent files persistence is out of scope for M2.
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit"))
        {
            if (CoreEngineApp* lApp = Owner.GetEngineApp())
            {
                lApp->RequestQuit();
            }
        }

        ImGui::EndMenu();
    }

    // =============================================================================
    // Edit
    // =============================================================================
    void MainMenuBar::DrawEditMenu()
    {
        if (!ImGui::BeginMenu("Edit")) { return; }

        // No command system yet — surfaces are visible but disabled.
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false);

        ImGui::EndMenu();
    }

    // =============================================================================
    // View
    // =============================================================================
    void MainMenuBar::DrawViewMenu(EditorSubsystem& Owner)
    {
        if (!ImGui::BeginMenu("View")) { return; }

        ImGui::MenuItem("Hierarchy",     nullptr, &Owner.GetShowHierarchyRef());
        ImGui::MenuItem("Inspector",     nullptr, &Owner.GetShowInspectorRef());
        ImGui::MenuItem("Asset Browser", nullptr, &Owner.GetShowAssetBrowserRef());
        ImGui::MenuItem("Viewport",      nullptr, &Owner.GetShowViewportRef());

        ImGui::EndMenu();
    }

    // =============================================================================
    // Window
    // =============================================================================
    void MainMenuBar::DrawWindowMenu()
    {
        if (!ImGui::BeginMenu("Window")) { return; }

        if (ImGui::MenuItem("Reset Layout"))
        {
            OPAAX_CORE_INFO("MainMenuBar: Reset Layout — TBD (stretch goal)");
        }

        ImGui::EndMenu();
    }

    // =============================================================================
    // Help
    // =============================================================================
    void MainMenuBar::DrawHelpMenu()
    {
        if (!ImGui::BeginMenu("Help")) { return; }

        if (ImGui::MenuItem("About"))
        {
            m_bOpenAboutModal = true;
        }

        ImGui::EndMenu();
    }

    // =============================================================================
    // PIE controls (right-aligned Play / Pause / Stop)
    // =============================================================================
    void MainMenuBar::DrawPieControls(EditorSubsystem& Owner)
    {
        const Editor::EEditorState lState = Owner.GetEditorState();

        const float lBtnW       = 28.f;
        const float lSpacing    = ImGui::GetStyle().ItemSpacing.x;
        const float lTotalWidth = (lBtnW * 3.f) + (lSpacing * 2.f);
        const float lMargin     = 8.f;

        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - lTotalWidth - lMargin);

        // Play — enabled in Editing only (Resume from Paused goes through Pause toggle).
        const bool bCanPlay = (lState == Editor::EEditorState::Editing);
        ImGui::BeginDisabled(!bCanPlay);
        if (ImGui::Button("|>", ImVec2(lBtnW, 0.f)))
        {
            Owner.EnterPlayMode();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        // Pause — toggles Playing <-> Paused.
        const bool bCanPause = (lState == Editor::EEditorState::Playing
                             || lState == Editor::EEditorState::Paused);
        ImGui::BeginDisabled(!bCanPause);
        if (ImGui::Button("||", ImVec2(lBtnW, 0.f)))
        {
            Owner.PauseToggle();
        }
        ImGui::EndDisabled();
        ImGui::SameLine();

        // Stop — exits PIE from Playing or Paused.
        const bool bCanStop = (lState == Editor::EEditorState::Playing
                            || lState == Editor::EEditorState::Paused);
        ImGui::BeginDisabled(!bCanStop);
        if (ImGui::Button("[]", ImVec2(lBtnW, 0.f)))
        {
            Owner.ExitPlayMode();
        }
        ImGui::EndDisabled();
    }

    // =============================================================================
    // About modal
    // =============================================================================
    void MainMenuBar::DrawAboutModal()
    {
        if (m_bOpenAboutModal)
        {
            ImGui::OpenPopup(ABOUT_POPUP_ID);
            m_bOpenAboutModal = false;
        }

        // Center on appearance
        const ImVec2 lCenter = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(lCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(ABOUT_POPUP_ID, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Text("OpaaxEngine");
            ImGui::Text("Version: %s", OPAAX_ENGINE_VERSION);
            ImGui::Separator();

            if (ImGui::Button("Close", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
