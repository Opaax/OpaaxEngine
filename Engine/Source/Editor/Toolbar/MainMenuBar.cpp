#include "MainMenuBar.h"

#if OPAAX_WITH_EDITOR

#include <filesystem>
#include <imgui.h>
#include <tinyfiledialogs.h>

#include "Editor/EditorState.h"
#include "Editor/EditorSubsystem.h"
#include "Core/CoreEngineApp.h"
#include "Core/Log/OpaaxLog.h"
#include "Core/OpaaxPath.h"
#include "Core/Config/EngineConfig.h"
#include "Scene/SceneManager.h"

namespace Opaax::Editor
{
    // TODO: M8 polish — wire CMake-generated version + commit hash.
    static constexpr const char* OPAAX_ENGINE_VERSION = "0.1.0-dev";
    static constexpr const char* ABOUT_POPUP_ID       = "About OpaaxEngine";
    static constexpr const char* DISCARD_POPUP_ID     = "Discard current scene?";

    static const char* SCENE_FILTER_PATTERN = "*.scene.json";
    static const char* SCENE_FILTER_DESC    = "OpaaxEngine Scene (*.scene.json)";

    static SceneManager* GetSceneManager(EditorSubsystem& Owner)
    {
        CoreEngineApp* lApp = Owner.GetEngineApp();
        return lApp ? lApp->GetSceneManager() : nullptr;
    }

    // Returns the directory the next file dialog should open in:
    // - last-used dir from this session if the user already opened/saved once, else
    // - the configured editor.defaultScenePath resolved against the project root
    //   (or exe-relative when no project root is baked in — release fallback).
    // Trailing slash matters on Windows: tinyfiledialogs treats it as a directory hint.
    static OpaaxString GetDialogDefaultDir(EditorSubsystem& Owner)
    {
        const OpaaxString& lLast = Owner.GetLastDialogDir();
        OpaaxString lDir = lLast.IsEmpty()
            ? OpaaxPath::ResolveFromProject(EngineConfig::EditorDefaultScenePath())
            : lLast;

        // Ensure the dir exists so tinyfiledialogs has something to open.
        std::error_code lEc;
        std::filesystem::create_directories(std::filesystem::path(lDir.CStr()), lEc);

        // Append trailing slash for tinyfd "directory" semantics.
        if (!lDir.IsEmpty() &&
            lDir.Data()[lDir.GetLength() - 1] != '/' &&
            lDir.Data()[lDir.GetLength() - 1] != '\\')
        {
            lDir += "/";
        }
        return lDir;
    }

    // Stores the parent directory of InAbsPath into EditorSubsystem so the next dialog
    // opens at the same location.
    static void RememberDialogDir(EditorSubsystem& Owner, const char* InAbsPath)
    {
        if (!InAbsPath || InAbsPath[0] == '\0') { return; }
        const std::filesystem::path lParent =
            std::filesystem::path(InAbsPath).parent_path();
        Owner.SetLastDialogDir(lParent.generic_string().c_str());
    }

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

        // Global Ctrl+S — fires regardless of which window has focus, as long as
        // no widget is currently consuming the key chord. ImGui::IsKeyChordPressed
        // already routes through the global shortcut/route system in 1.92.x.
        if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S))
        {
            DoSaveOrSaveAs(Owner);
        }

        DrawAboutModal();
        DrawDiscardModal(Owner);
    }

    // =============================================================================
    // File
    // =============================================================================
    void MainMenuBar::DrawFileMenu(EditorSubsystem& Owner)
    {
        if (!ImGui::BeginMenu("File")) { return; }

        if (ImGui::MenuItem("New Scene"))
        {
            // Always confirm — no dirty tracking yet (deferred per M2 scope).
            m_bOpenDiscardModal = true;
        }

        if (ImGui::MenuItem("Open Scene..."))
        {
            DoOpen(Owner);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
        {
            DoSaveOrSaveAs(Owner);
        }

        if (ImGui::MenuItem("Save Scene As..."))
        {
            DoSaveAs(Owner);
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
    // File-menu helpers
    // =============================================================================
    void MainMenuBar::DoOpen(EditorSubsystem& Owner)
    {
        SceneManager* lMgr = GetSceneManager(Owner);
        if (!lMgr)
        {
            OPAAX_CORE_WARN("MainMenuBar::DoOpen — SceneManager unavailable.");
            return;
        }

        const OpaaxString lDefaultDir = GetDialogDefaultDir(Owner);

        // tinyfiledialogs takes the filter list as a (char const* const*) array.
        const char* lPath = tinyfd_openFileDialog(
            "Open Scene",
            lDefaultDir.CStr(),
            /*nFilters*/ 1,
            &SCENE_FILTER_PATTERN,
            SCENE_FILTER_DESC,
            /*allowMulti*/ 0);

        if (lPath && lMgr->Open(lPath))
        {
            RememberDialogDir(Owner, lPath);
        }
    }

    void MainMenuBar::DoSaveAs(EditorSubsystem& Owner)
    {
        SceneManager* lMgr = GetSceneManager(Owner);
        if (!lMgr)
        {
            OPAAX_CORE_WARN("MainMenuBar::DoSaveAs — SceneManager unavailable.");
            return;
        }

        // Combine default dir with a placeholder filename so the save dialog opens
        // in the right location with a sensible suggested name pre-filled.
        OpaaxString lDefaultPath = GetDialogDefaultDir(Owner);
        lDefaultPath += "untitled.scene.json";

        const char* lPath = tinyfd_saveFileDialog(
            "Save Scene As",
            lDefaultPath.CStr(),
            /*nFilters*/ 1,
            &SCENE_FILTER_PATTERN,
            SCENE_FILTER_DESC);

        if (lPath && lMgr->SaveAs(lPath))
        {
            RememberDialogDir(Owner, lPath);
            // Auto-refresh so the new file appears in the Asset Browser without a manual Refresh.
            Owner.RefreshAssetBrowser();
        }
    }

    void MainMenuBar::DoSaveOrSaveAs(EditorSubsystem& Owner)
    {
        SceneManager* lMgr = GetSceneManager(Owner);
        if (!lMgr)
        {
            OPAAX_CORE_WARN("MainMenuBar::DoSaveOrSaveAs — SceneManager unavailable.");
            return;
        }

        if (lMgr->HasCurrentScenePath()) { lMgr->Save(); }
        else                             { DoSaveAs(Owner); }
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

    // =============================================================================
    // Discard modal — confirms "New Scene" wipes the current world
    // =============================================================================
    void MainMenuBar::DrawDiscardModal(EditorSubsystem& Owner)
    {
        if (m_bOpenDiscardModal)
        {
            ImGui::OpenPopup(DISCARD_POPUP_ID);
            m_bOpenDiscardModal = false;
        }

        const ImVec2 lCenter = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(lCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal(DISCARD_POPUP_ID, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::Text("Unsaved changes will be lost. Continue?");
            ImGui::Separator();

            if (ImGui::Button("Discard", ImVec2(120, 0)))
            {
                if (SceneManager* lMgr = GetSceneManager(Owner))
                {
                    lMgr->NewScene();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
