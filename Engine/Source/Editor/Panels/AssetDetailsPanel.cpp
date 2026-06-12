#include "AssetDetailsPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#include "Editor/Assets/AssetTypeRegistry.h"
#include "Editor/Assets/IAssetTypeActions.h"
#include "Editor/Events/EditorEvents.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    void AssetDetailsPanel::OnSubscribe(EditorEventBus& InBus)
    {
        m_AssetSelectedToken = InBus.Subscribe<OnAssetSelectedEvent>(
            [this](const OnAssetSelectedEvent& InEvent)
            {
                m_SelectedID   = InEvent.GetAssetID();
                m_SelectedType = InEvent.GetTypeID();
                OPAAX_CORE_INFO("AssetDetailsPanel - selection: {} ({})",
                    m_SelectedID.ToString().CStr(), m_SelectedType.ToString().CStr());
            });
    }

    void AssetDetailsPanel::Draw(IEditorUIBackend& InUIBackend)
    {
        ImGui::Begin("Asset Details");

        if (!m_SelectedID.IsValid())
        {
            ImGui::TextDisabled("No asset selected.");
            ImGui::TextDisabled("Click an asset in the Asset Browser.");
            ImGui::End();
            return;
        }

        IAssetTypeActions* lActions = AssetTypeRegistry::Find(m_SelectedType);
        if (!lActions)
        {
            const OpaaxString lIDStr   = m_SelectedID.ToString();
            const OpaaxString lTypeStr = m_SelectedType.ToString();
            ImGui::Text("ID   : %s", lIDStr.CStr());
            ImGui::Text("Type : %s", lTypeStr.CStr());
            ImGui::TextDisabled("No editor registered for this asset type.");
            ImGui::End();
            return;
        }

        if (lActions->CanEdit())
        {
            lActions->DrawEditor(m_SelectedID, InUIBackend);
        }
        else if (lActions->CanPreview())
        {
            lActions->DrawPreview(m_SelectedID, InUIBackend);
        }
        else
        {
            const OpaaxString lIDStr = m_SelectedID.ToString();
            ImGui::Text("%s", lIDStr.CStr());
            ImGui::TextDisabled("No preview or editor for this asset type.");
        }

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
