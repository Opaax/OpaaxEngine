#include "AssetBrowserPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include <filesystem>

#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    void AssetBrowserPanel::Draw()
    {
        ImGui::Begin("Asset Browser");

        // Current base path
        const OpaaxString& lBase = OpaaxPath::GetBasePath();
        ImGui::TextDisabled("Base: %s", lBase.CStr());
        ImGui::Separator();

        // List loaded assets from registry
        ImGui::SeparatorText("Loaded Assets");

        const auto& lAssets = AssetRegistry::GetAssets();

        if (lAssets.empty())
        {
            ImGui::TextDisabled("No assets loaded.");
        }
        else
        {
            for (const auto& [lKey, lEntry] : lAssets)
            {
                // Show filename only — full path is too long for a panel
                const std::string lFullPath = OpaaxStringID(lKey).ToString().CStr();
                const std::string lFilename = std::filesystem::path(lFullPath).filename().string();

                ImGui::Text("%s", lFilename.c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s\nRefs: %u",
                        lFullPath.c_str(),
                        lEntry.Block ? lEntry.Block->Get() : 0u);
                }
            }
        }

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR