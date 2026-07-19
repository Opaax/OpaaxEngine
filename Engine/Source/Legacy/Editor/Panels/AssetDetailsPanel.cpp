#include "AssetDetailsPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#include "Assets/AssetRegistry.h"
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

        const bool bCanEdit    = lActions->CanEdit();
        const bool bCanPreview = lActions->CanPreview();

        if (!bCanEdit && !bCanPreview)
        {
            const OpaaxString lIDStr = m_SelectedID.ToString();
            ImGui::Text("%s", lIDStr.CStr());
            ImGui::TextDisabled("No preview or editor for this asset type.");
            ImGui::End();
            return;
        }

        // GLOBAL load gate (fixes the Unload-fights-preview bug for EVERY type). Both DrawEditor and
        // DrawPreview need the asset loaded, and several type-actions call AssetRegistry::Load inside
        // them — so drawing the selection every frame would force-reload it and silently defeat an
        // explicit Unload from the browser. Gate the dispatch on load state here (one place, all
        // types) and never auto-load; offer an explicit Load instead (this is a real, interactive window).
        if (!AssetRegistry::IsLoaded(m_SelectedID))
        {
            const OpaaxString lIDStr = m_SelectedID.ToString();
            ImGui::Text("%s", lIDStr.CStr());
            ImGui::TextDisabled("Not loaded.");
            if (ImGui::Button("Load")) { lActions->Load(m_SelectedID); }
            ImGui::End();
            return;
        }

        if (bCanEdit) { lActions->DrawEditor(m_SelectedID, InUIBackend); }
        else          { lActions->DrawPreview(m_SelectedID, InUIBackend); }

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
