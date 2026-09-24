#include "Editor/Panels/ResourcePreviewPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Extensions/ResourceTypeRegistry.h"
#include "Editor/Resources/ResourcePreview.h"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"

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

            // NO TYPE ID IS COMPARED HERE any more. The chrome's SetPreview facet built the live
            // object, and it is the only thing that named a resource type — which is what this
            // panel's own growth-point note asked for when a second previewable type arrived.
            if (IResourcePreviewClaim* lClaim = ClaimFor(InEntry))
            {
                lClaim->Draw(m_Context);
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

    // =============================================================================
    // Claims
    // =============================================================================
    IResourcePreviewClaim* ResourcePreviewPanel::ClaimFor(const ResourcePreviewEntry& InEntry)
    {
        const OpaaxStringID lKey(InEntry.File.AbsPath);

        auto lIt = m_Claims.find(lKey.GetId());
        if (lIt != m_Claims.end())
        {
            return lIt->second.get();
        }

        const ResourceTypeDesc* lChrome = m_Context.Extensions.ResourceTypes().Find(InEntry.TypeId);
        if (lChrome == nullptr || !lChrome->OnPreviewOpen)
        {
            return nullptr;   // NOT cached: a type with no preview has nothing to keep
        }

        TUniquePtr<IResourcePreviewClaim> lClaim = lChrome->OnPreviewOpen(m_Context.Resources, InEntry.File);

        // Once per open, not per frame: the discrete event, and the only signal that a double-click
        // reached this panel at all.
        if (lClaim == nullptr)
        {
            OPAAX_LOG(LogResourcePreviewPanel, Warn, "Cannot preview '{}' — it did not load",
                      InEntry.File.AbsPath.CStr());
            return nullptr;
        }

        OPAAX_LOG(LogResourcePreviewPanel, Info, "Previewing '{}'", InEntry.File.AbsPath.CStr());

        return m_Claims.emplace(lKey.GetId(), Move(lClaim)).first->second.get();
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
