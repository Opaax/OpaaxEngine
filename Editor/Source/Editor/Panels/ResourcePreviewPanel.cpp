#include "Editor/Panels/ResourcePreviewPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Extensions/ResourceTypeRegistry.h"
#include "Editor/ImguiLibrary/ImguiLayout.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"
#include "Editor/Resources/ResourcePreview.h"
#include "Editor/UI/IEditorUIBackend.h"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"
#include "Engine/Subsystems/Resources/Types/TextureResource.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    ResourcePreviewPanel::ResourcePreviewPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    ResourcePreviewPanel::~ResourcePreviewPanel() = default;

    // =============================================================================
    // Lifecycle
    // =============================================================================
    void ResourcePreviewPanel::Shutdown()
    {
        m_Claims.clear();
    }

    void ResourcePreviewPanel::DrawContents()
    {
        if (m_Context.Preview.IsEmpty())
        {
            ImGui::TextDisabled("Nothing to preview.");
            ImGui::TextDisabled("Double-click a resource in the Resource Browser.");

            ReleaseClosedClaims();
            return;
        }

        if (m_Context.Preview.Entries().size() > 1 && ImGui::SmallButton("Close all"))
        {
            m_Context.Preview.CloseAll();
            ReleaseClosedClaims();
            return;   // the list just changed under the loop below
        }

        // Collected, never applied inside the loop — closing an entry resizes the very array being
        // iterated (the DrawBreadcrumb rule, one panel over).
        Int64 lCloseIndex = -1;

        const TDynArray<ResourcePreviewEntry>& lEntries = m_Context.Preview.Entries();
        for (Uint64 i = 0; i < lEntries.size(); ++i)
        {
            if (DrawEntry(lEntries[i], i, i == m_Context.Preview.Focused()))
            {
                lCloseIndex = static_cast<Int64>(i);
            }
        }

        if (lCloseIndex >= 0)
        {
            m_Context.Preview.Close(static_cast<Uint64>(lCloseIndex));
        }

        ReleaseClosedClaims();
    }

    // =============================================================================
    // Drawing
    // =============================================================================
    bool ResourcePreviewPanel::DrawEntry(const ResourcePreviewEntry& InEntry, const Uint64 InIndex, const bool bInFocused)
    {
        // Scoped by PATH, not by index: an entry keeps its open/closed state when one above it is
        // closed, which an index-keyed id would shuffle (I16's PushID-per-entry rule).
        ImGui::PushID(InEntry.File.AbsPath.CStr());

        // The newest / re-opened one springs open; the others keep whatever the user left them at.
        if (bInFocused) { ImGui::SetNextItemOpen(true, ImGuiCond_Always); }

        const bool bOpen = ImGui::CollapsingHeader(InEntry.File.Name.CStr());

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight());
        const bool bClose = ImGui::SmallButton("x");

        if (bOpen)
        {
            DrawIdentity(InEntry);

            // ONE previewable type today, so one branch. GROWTH POINT, named not built: the second
            // type that wants content here turns this into a `SetPreview` chrome facet beside
            // SetActivate, where per-type presentation already lives — not a chain of ifs.
            if (InEntry.TypeId == ResourceTypeID::Get<TextureResource>())
            {
                DrawTexture(InEntry.File.AbsPath);
            }
            else
            {
                ImGui::TextDisabled("No preview for this resource type.");
            }
        }

        ImGui::PopID();
        return bClose;
    }

    void ResourcePreviewPanel::DrawIdentity(const ResourcePreviewEntry& InEntry) const
    {
        // The chrome's override first, then the FORMAT's own label — the same order the browser's
        // tooltip uses, so the two windows cannot disagree about what a file is.
        const char* lLabel = "Unknown type";
        if (const ResourceTypeDesc* lChrome = m_Context.Extensions.ResourceTypes().Find(InEntry.TypeId);
            lChrome != nullptr && lChrome->Label.IsValid())
        {
            lLabel = lChrome->Label.CStr();
        }
        else if (const ResourceFormatEntry* lFormat =
                     m_Context.Engine.GetRegistries().Resources().FindByTypeId(InEntry.TypeId))
        {
            lLabel = lFormat->Format->Label;
        }

        ImGui::TextDisabled("Type : %s", lLabel);
        ImGui::TextDisabled("Path : %s", InEntry.File.AbsPath.CStr());
    }

    void ResourcePreviewPanel::DrawTexture(const OpaaxString& InAbsPath)
    {
        const TextureResource* lTexture = ClaimTexture(InAbsPath);
        if (lTexture == nullptr)
        {
            ImGui::TextDisabled("Could not load this image.");
            return;
        }

        const ImVec2 lSize = ImguiLayout::AspectFit(lTexture->Width, lTexture->Height, MAX_IMAGE_SIZE);

        ImguiWidgets::Image(lTexture->GetTexture() != nullptr
                                ? m_Context.UIBackend.GetTextureImage(*lTexture->GetTexture())
                                : EditorImage{},
                            lSize);

        ImGui::TextDisabled("%u x %u", lTexture->Width, lTexture->Height);
    }

    // =============================================================================
    // Claims
    // =============================================================================
    const TextureResource* ResourcePreviewPanel::ClaimTexture(const OpaaxString& InAbsPath)
    {
        const OpaaxStringID lKey(InAbsPath);

        auto lIt = m_Claims.find(lKey.GetId());
        if (lIt == m_Claims.end())
        {
            ResourceRef<TextureResource> lRef = m_Context.Resources.Load<TextureResource>(InAbsPath.CStr());

            // Once per open, not per frame: the discrete event, and the only signal that a
            // double-click reached this panel at all.
            if (lRef.IsValid())
            {
                OPAAX_LOG(LogResourcePreviewPanel, Info, "Previewing '{}'", InAbsPath.CStr());
            }
            else
            {
                OPAAX_LOG(LogResourcePreviewPanel, Warn, "Cannot preview '{}' — it did not load", InAbsPath.CStr());
                return nullptr;   // NOT cached: Get() on a failed claim answers the placeholder
            }

            lIt = m_Claims.emplace(lKey.GetId(), Move(lRef)).first;
        }

        return lIt->second.Get();
    }

    void ResourcePreviewPanel::ReleaseClosedClaims()
    {
        for (auto lIt = m_Claims.begin(); lIt != m_Claims.end();)
        {
            bool bStillOpen = false;
            for (const ResourcePreviewEntry& lEntry : m_Context.Preview.Entries())
            {
                if (OpaaxStringID(lEntry.File.AbsPath).GetId() == lIt->first)
                {
                    bStillOpen = true;
                    break;
                }
            }

            lIt = bStillOpen ? std::next(lIt) : m_Claims.erase(lIt);
        }
    }
}
