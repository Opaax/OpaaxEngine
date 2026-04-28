#include "AssetBrowserPanel.h"

#include "Assets/AssetRegistry.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>
#include "Assets/Loader/TextureLoader.h"
#include "Core/OpaaxPath.h"
#include "Core/Log/OpaaxLog.h"
#include "RHI/OpenGL/OpenGLTexture2D.h"

namespace Opaax::Editor
{
    // =============================================================================
    // Startup — first scan
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
        AssetScanner::ScanConfig lConfig;
        lConfig.RootDir         = "GameAssets";
        lConfig.ManifestAbsPath = m_ManifestAbsPath;
        lConfig.bFlagMissing    = true;

        m_LastScanResult = AssetScanner::Scan(lConfig);

        OPAAX_CORE_INFO("AssetBrowserPanel: scan complete — +{} new, {} existing, {} missing",
            m_LastScanResult.Added,
            m_LastScanResult.Existing,
            m_LastScanResult.Missing);

        AssetScanner::ScanConfig lConfig1;
        lConfig1.RootDir         = "EngineAssets";
        lConfig1.ManifestAbsPath = OpaaxPath::Resolve("EngineAssets/AssetManifest.json");
        lConfig1.bFlagMissing    = true;

        m_LastScanResult = AssetScanner::Scan(lConfig1);

        OPAAX_CORE_INFO("AssetBrowserPanel: scan complete — +{} new, {} existing, {} missing",
            m_LastScanResult.Added,
            m_LastScanResult.Existing,
            m_LastScanResult.Missing);

        
        m_bScanned       = true;
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
        // Refresh button
        if (ImGui::Button("  Refresh  "))
        {
            RunScan();
        }

        // Scan stats
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

        // Filter by text
        {
            char lBuf[128];
            strncpy_s(lBuf, sizeof(lBuf), m_FilterText.CStr(), _TRUNCATE);
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::InputTextWithHint("##Filter", "Filter assets...", lBuf, sizeof(lBuf)))
            {
                m_FilterText = OpaaxString(lBuf);
            }
        }

        ImGui::Spacing();

        // Filter by type buttons
        const OpaaxStringID lTypes[] = {
            OpaaxStringID(""),
            OpaaxStringID("Texture2D"),
            OpaaxStringID("AudioClip"),
            OpaaxStringID("Animation"),
            OpaaxStringID("InputMap"),
            OpaaxStringID("Shader"),
            OpaaxStringID("Data"),
            OpaaxStringID("Scene"),
        };

        const char* lTypeLabels[] = {
            "All", "Texture", "Audio", "Anim", "Input", "Shader", "Data", "Scene"
        };

        for (Int32 i = 0; i < 8; ++i)
        {
            if (i > 0) { ImGui::SameLine(); }

            const bool bSelected = (m_FilterType == lTypes[i]);

            if (bSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                    ImVec4(0.26f, 0.59f, 0.98f, 1.f));
            }

            if (ImGui::SmallButton(lTypeLabels[i]))
            {
                m_FilterType = lTypes[i];
            }

            if (bSelected) { ImGui::PopStyleColor(); }
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

        // Count visible for display
        Int32 lVisible = 0;

        for (const auto& [lKey, lDesc] : lAll)
        {
            // Type filter
            if (m_FilterType.IsValid() && lDesc.Type != m_FilterType)
            {
                continue;
            }

            // Text filter
            if (!m_FilterText.IsEmpty())
            {
                const OpaaxString lIDStr  = lDesc.ID.ToString().ToLower();
                const OpaaxString lFilter = m_FilterText.ToLower();
                if (lIDStr.Find(lFilter.CStr()) == -1) { continue; }
            }

            DrawAssetEntry(lDesc);
            ++lVisible;
        }

        if (lVisible == 0)
        {
            ImGui::TextDisabled("No assets match the current filter.");
        }
    }

    // =============================================================================
    // Single entry
    // =============================================================================
    void AssetBrowserPanel::DrawAssetEntry(const AssetDescriptor& InDesc)
    {
        const bool bLoaded  = AssetRegistry::IsLoaded(InDesc.ID);
        const bool bMissing = InDesc.bMissing;

        // Color coding
        ImVec4 lColor;
        if (bMissing)       { lColor = ImVec4(1.f,  0.3f, 0.3f, 1.f); }  // red
        else if (bLoaded)   { lColor = ImVec4(0.4f, 1.f,  0.4f, 1.f); }  // green
        else                { lColor = ImVec4(1.f,  1.f,  1.f,  1.f); }  // white

        ImGui::PushStyleColor(ImGuiCol_Text, lColor);

        // Type icon prefix — simple text icon
        const char* lIcon = "[ ? ]";
        if      (InDesc.Type == OpaaxStringID("Texture2D")) { lIcon = "[ T ]"; }
        else if (InDesc.Type == OpaaxStringID("AudioClip")) { lIcon = "[ A ]"; }
        else if (InDesc.Type == OpaaxStringID("Animation")) { lIcon = "[ @ ]"; }
        else if (InDesc.Type == OpaaxStringID("InputMap"))  { lIcon = "[ I ]"; }
        else if (InDesc.Type == OpaaxStringID("Shader"))    { lIcon = "[ S ]"; }
        else if (InDesc.Type == OpaaxStringID("Data"))      { lIcon = "[ D ]"; }
        else if (InDesc.Type == OpaaxStringID("Scene"))     { lIcon = "[ # ]"; }

        // Selectable row
        const OpaaxString lIDStr = InDesc.ID.ToString();
        char lLabel[256];
        snprintf(lLabel, sizeof(lLabel), "%s  %s##%u",
            lIcon, lIDStr.CStr(), InDesc.ID.GetId());

        const bool bSelected = (m_HoveredID == InDesc.ID);
        ImGui::Selectable(lLabel, bSelected);

        ImGui::PopStyleColor();

        // --- Drag & Drop source ---
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            const Uint32 lID = InDesc.ID.GetId();
            ImGui::SetDragDropPayload(DragDropPayloadType, &lID, sizeof(Uint32));

            // Preview while dragging
            ImGui::Text("  %s  %s", lIcon, lIDStr.CStr());

            // Show texture preview while dragging if it's a texture
            if (InDesc.Type == OpaaxStringID("Texture2D") && bLoaded)
            {
                DrawTexturePreview(InDesc);
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

            // Inline texture preview in tooltip
            if (InDesc.Type == OpaaxStringID("Texture2D") && bLoaded)
            {
                ImGui::Spacing();
                DrawTexturePreview(InDesc);
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
            if (!bMissing && !bLoaded)
            {
                if (ImGui::MenuItem("Load"))
                {
                    // NOTE: We load as Texture2D if that's the type.
                    //   M8.4 will generalize this with a type-dispatch mechanism.
                    if (InDesc.Type == OpaaxStringID("Texture2D"))
                    {
                        AssetRegistry::Load<OpenGLTexture2D>(InDesc.ID);
                    }
                }
            }

            if (bLoaded)
            {
                if (ImGui::MenuItem("Reload"))
                {
                    if (InDesc.Type == OpaaxStringID("Texture2D"))
                    {
                        AssetRegistry::Reload<OpenGLTexture2D>(InDesc.ID);
                    }
                }

                if (ImGui::MenuItem("Unload"))
                {
                    AssetRegistry::Unload(InDesc.ID);
                }
            }

            ImGui::Separator();
            ImGui::TextDisabled("%s", InDesc.RelPath.CStr());

            ImGui::EndPopup();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
        {
            if (InDesc.Type == OpaaxStringID("Scene"))
            {
                // FIXME: M_Scene — SceneManager::LoadFromFile(InDesc.RelPath)
                //   Pas implémenté — SceneManager ne sait pas encore charger
                //   depuis un AssetDescriptor. À connecter quand Scene devient
                //   un asset first-class.
                OPAAX_CORE_WARN("AssetBrowserPanel: Scene loading not yet implemented. "
                    "Coming in M_Scene. File: '{}'", InDesc.RelPath);
            }
        }
    }

    // =============================================================================
    // Texture preview
    // =============================================================================
    void AssetBrowserPanel::DrawTexturePreview(const AssetDescriptor& InDesc)
    {
        // NOTE: IsLoaded check done by caller — we assume the asset is in cache.
        const auto lHandle =
            AssetRegistry::Load<OpenGLTexture2D>(InDesc.ID);

        if (!lHandle.IsValid()) { return; }

        const OpenGLTexture2D* lTex = lHandle.Get();

        // Scale preview to fit — max 128px, preserve aspect ratio
        constexpr float lMaxSize = 128.f;
        const float lW = static_cast<float>(lTex->GetWidth());
        const float lH = static_cast<float>(lTex->GetHeight());
        const float lAspect = (lH > 0.f) ? (lW / lH) : 1.f;

        float lDrawW = lMaxSize;
        float lDrawH = lMaxSize;

        if (lAspect > 1.f) { lDrawH = lMaxSize / lAspect; }
        else               { lDrawW = lMaxSize * lAspect;  }

        ImGui::Image(
            lTex->GetRendererID(),
            ImVec2(lDrawW, lDrawH),
            ImVec2(0.f, 1.f),
            ImVec2(1.f, 0.f)
        );

        ImGui::TextDisabled("%ux%u", lTex->GetWidth(), lTex->GetHeight());
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR