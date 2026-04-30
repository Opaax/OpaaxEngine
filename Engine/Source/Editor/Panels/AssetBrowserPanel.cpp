#include "AssetBrowserPanel.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Assets/AssetRegistry.h"
#include "Assets/AssetManifest.h"
#include "Assets/AssetScanner.h"
#include "Core/OpaaxPath.h"
#include "Core/Log/OpaaxLog.h"
#include "Editor/Assets/AssetTypeRegistry.h"
#include "Editor/Assets/IAssetTypeActions.h"

namespace Opaax::Editor
{
    // =============================================================================
    // Startup
    // =============================================================================
    void AssetBrowserPanel::Startup()
    {
        m_ManifestAbsPath = OpaaxPath::Resolve("GameAssets/AssetManifest.json");
        RunScan();
    }

    // =============================================================================
    // Scan
    // =============================================================================
    void AssetBrowserPanel::RunScan()
    {
        struct ScanTarget { const char* RootDir; const char* ManifestRelPath; };
        static constexpr ScanTarget k_ScanTargets[] =
        {
            { "GameAssets",   "GameAssets/AssetManifest.json"   },
            { "EngineAssets", "EngineAssets/AssetManifest.json" },
        };

        for (const auto& lTarget : k_ScanTargets)
        {
            AssetScanner::ScanConfig lConfig;
            lConfig.RootDir         = lTarget.RootDir;
            lConfig.ManifestAbsPath = OpaaxPath::Resolve(lTarget.ManifestRelPath);
            lConfig.bFlagMissing    = true;

            m_LastScanResult = AssetScanner::Scan(lConfig);

            OPAAX_CORE_INFO("AssetBrowserPanel: scan '{}' — +{} new, {} existing, {} missing",
                lTarget.RootDir,
                m_LastScanResult.Added,
                m_LastScanResult.Existing,
                m_LastScanResult.Missing);
        }

        m_bScanned = true;
    }

    // =============================================================================
    // Draw
    // =============================================================================
    void AssetBrowserPanel::Draw()
    {
        ImGui::Begin("Asset Browser");
        DrawToolbar();
        ImGui::Separator();
        DrawAssetList();
        ImGui::End();
    }

    // =============================================================================
    // Toolbar
    // =============================================================================
    void AssetBrowserPanel::DrawToolbar()
    {
        if (ImGui::Button("  Refresh  "))
        {
            RunScan();
        }

        if (m_bScanned)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("+%u  |  %u  |  ",
                m_LastScanResult.Added,
                m_LastScanResult.Existing);

            if (m_LastScanResult.Missing > 0)
            {
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.3f, 0.3f, 1.f));
                ImGui::Text("%u missing", m_LastScanResult.Missing);
                ImGui::PopStyleColor();
            }
        }

        ImGui::Spacing();

        // Text filter
        {
            char lBuf[128];
            strncpy_s(lBuf, sizeof(lBuf), m_Filter.Text.CStr(), _TRUNCATE);
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::InputTextWithHint("##Filter", "Filter assets...", lBuf, sizeof(lBuf)))
            {
                m_Filter.Text = OpaaxString(lBuf);
            }
        }

        ImGui::Spacing();

        // Type filter — "All" button, then one per registered asset type
        {
            const bool bAllSelected = !m_Filter.TypeID.IsValid();
            
            if (bAllSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f));
            }
            
            if (ImGui::SmallButton("All"))
            {
                m_Filter.TypeID = OpaaxStringID{};
            }
            
            if (bAllSelected)
            {
                ImGui::PopStyleColor();
            }
        }

        for (const auto& lActions : AssetTypeRegistry::GetAll())
        {
            ImGui::SameLine();
            
            const bool bSelected = (m_Filter.TypeID == lActions->GetTypeID());
            
            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.f));
            }
            
            if (ImGui::SmallButton(lActions->GetLabel()))
            {
                m_Filter.TypeID = lActions->GetTypeID();
            }
            
            if (bSelected)
            {
                ImGui::PopStyleColor();
            }
        }
    }

    // =============================================================================
    // Asset list
    // =============================================================================
    void AssetBrowserPanel::DrawAssetList()
    {
        const auto& lAll = AssetManifest::GetAll();

        if (lAll.empty())
        {
            ImGui::TextDisabled("No assets in manifest. Click Refresh to scan.");
            return;
        }

        Int32 lVisible = 0;

        for (const auto& [lKey, lDesc] : lAll)
        {
            if (!m_Filter.Matches(lDesc)) { continue; }
            DrawAssetEntry(lDesc);
            ++lVisible;
        }

        if (lVisible == 0)
        {
            ImGui::TextDisabled("No assets match the current filter.");
        }
    }

    // =============================================================================
    // Single entry — icon, colour, D&D source, tooltip, context menu
    // =============================================================================
    void AssetBrowserPanel::DrawAssetEntry(const AssetDescriptor& InDesc)
    {
        const bool bLoaded  = AssetRegistry::IsLoaded(InDesc.ID);
        const bool bMissing = InDesc.bMissing;

        // Status colour
        ImVec4 lColor;
        if (bMissing)       { lColor = ImVec4(1.f,  0.3f, 0.3f, 1.f); }
        else if (bLoaded)   { lColor = ImVec4(0.4f, 1.f,  0.4f, 1.f); }
        else                { lColor = ImVec4(1.f,  1.f,  1.f,  1.f); }

        ImGui::PushStyleColor(ImGuiCol_Text, lColor);

        const char*       lIcon  = AssetTypeRegistry::GetIcon(InDesc.Type);
        const OpaaxString lIDStr = InDesc.ID.ToString();

        char lLabel[256];
        snprintf(lLabel, sizeof(lLabel), "%s  %s##%u", lIcon, lIDStr.CStr(), InDesc.ID.GetId());

        ImGui::Selectable(lLabel, m_HoveredID == InDesc.ID);
        ImGui::PopStyleColor();

        // --- Drag & Drop source ---
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            const Uint32 lID = InDesc.ID.GetId();
            ImGui::SetDragDropPayload(IAssetTypeActions::DragDropPayloadType, &lID, sizeof(Uint32));
            ImGui::Text("  %s  %s", lIcon, lIDStr.CStr());

            IAssetTypeActions* lActions = AssetTypeRegistry::Find(InDesc.Type);
            if (lActions && lActions->CanPreview() && bLoaded)
            {
                lActions->DrawPreview(InDesc.ID);
            }

            ImGui::EndDragDropSource();
        }

        // --- Hover tooltip ---
        if (ImGui::IsItemHovered())
        {
            m_HoveredID = InDesc.ID;

            ImGui::BeginTooltip();
            ImGui::TextDisabled("ID   : %s", lIDStr.CStr());
            ImGui::TextDisabled("Path : %s", InDesc.RelPath.CStr());
            ImGui::TextDisabled("Type : %s", InDesc.Type.ToString().CStr());
            ImGui::TextDisabled("Status : %s",
                bMissing ? "MISSING" : bLoaded ? "Loaded" : "Not loaded");

            IAssetTypeActions* lActions = AssetTypeRegistry::Find(InDesc.Type);
            if (lActions && lActions->CanPreview() && bLoaded)
            {
                ImGui::Spacing();
                lActions->DrawPreview(InDesc.ID);
            }

            ImGui::EndTooltip();
        }
        else if (m_HoveredID == InDesc.ID)
        {
            m_HoveredID = OpaaxStringID{};
        }

        // --- Right-click context menu ---
        if (ImGui::BeginPopupContextItem())
        {
            IAssetTypeActions* lActions = AssetTypeRegistry::Find(InDesc.Type);

            if (!bMissing && !bLoaded && lActions)
            {
                if (ImGui::MenuItem("Load")) { lActions->Load(InDesc.ID); }
            }

            if (bLoaded && lActions)
            {
                if (ImGui::MenuItem("Reload")) { lActions->Reload(InDesc.ID); }
                if (ImGui::MenuItem("Unload")) { AssetRegistry::Unload(InDesc.ID); }
            }

            ImGui::Separator();
            ImGui::TextDisabled("%s", InDesc.RelPath.CStr());
            ImGui::EndPopup();
        }

        // --- Double-click ---
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            if (InDesc.Type == OpaaxStringID("Scene"))
            {
                // FIXME: M_Scene — connect SceneManager::LoadFromFile when Scene becomes a first-class asset
                OPAAX_CORE_WARN("AssetBrowserPanel: Scene loading not yet implemented. File: '{}'",
                    InDesc.RelPath);
            }
        }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
