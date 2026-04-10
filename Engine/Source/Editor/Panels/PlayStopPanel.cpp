#include "PlayStopPanel.h"

#include "Scene/SceneManager.h"
#include "Scene/SceneSerializer.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include <filesystem>

#include "Scene/SceneManager.h"
#include "Scene/SceneSerializer.h"
#include "Core/OpaaxPath.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    static constexpr const char* TEMP_SCENE_PATH = "EditorTemp/PlaySession.json";

    void PlayStopPanel::Draw(SceneManager* InSceneManager)
    {
        // Centered toolbar
        ImGui::Begin("##Toolbar",
            nullptr,
            ImGuiWindowFlags_NoDecoration      |
            ImGuiWindowFlags_NoScrollbar        |
            ImGuiWindowFlags_NoScrollWithMouse);

        const float lWidth = ImGui::GetContentRegionAvail().x;
        const float lBtnW  = 80.f;

        ImGui::SetCursorPosX((lWidth - lBtnW * 2.f - ImGui::GetStyle().ItemSpacing.x) * 0.5f);

        if (!m_bPlaying)
        {
            if (ImGui::Button("▶  Play", ImVec2(lBtnW, 0.f)))
            {
                OnPlay(InSceneManager);
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.f));
            if (ImGui::Button("■  Stop", ImVec2(lBtnW, 0.f)))
            {
                OnStop(InSceneManager);
            }
            ImGui::PopStyleColor();
        }

        ImGui::End();
    }

    void PlayStopPanel::OnPlay(SceneManager* InSceneManager)
    {
        if (!InSceneManager || !InSceneManager->GetActiveScene()) { return; }

        OPAAX_CORE_INFO("PlayStopPanel: Play");

        // Serialize current state to temp file — restored on Stop
        const OpaaxString lTempPath = OpaaxPath::Resolve(TEMP_SCENE_PATH);
        std::filesystem::create_directories(
            std::filesystem::path(lTempPath.CStr()).parent_path());

        SceneSerializer::Serialize(*InSceneManager->GetActiveScene(), lTempPath.CStr());

        m_bPlaying = true;
    }

    void PlayStopPanel::OnStop(SceneManager* InSceneManager)
    {
        if (!InSceneManager || !InSceneManager->GetActiveScene()) { return; }

        OPAAX_CORE_INFO("PlayStopPanel: Stop — restoring scene.");

        const OpaaxString lTempPath = OpaaxPath::Resolve(TEMP_SCENE_PATH);

        if (!std::filesystem::exists(lTempPath.CStr()))
        {
            OPAAX_CORE_WARN("PlayStopPanel: no temp file found, cannot restore.");
            m_bPlaying = false;
            return;
        }

        // Get scene name before replacing
        const OpaaxString lSceneName = InSceneManager->GetActiveScene()->GetName();

        // Replace active scene with a fresh one deserialized from temp
        // NOTE: We can't clear the World in-place cleanly yet (FIXME in World.cpp).
        //   Replace() is the safe path — unloads current, loads fresh.
        auto lFreshScene = MakeUnique<Scene>(lSceneName.CStr());
        SceneSerializer::Deserialize(*lFreshScene, lTempPath.CStr());
        InSceneManager->Replace(std::move(lFreshScene));

        m_bPlaying = false;
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR