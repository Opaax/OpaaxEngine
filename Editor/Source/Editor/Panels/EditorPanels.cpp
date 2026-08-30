#include "Editor/Panels/EditorPanels.h"

#include "Editor/Extensions/PanelRegistry.h"

#include <imgui.h>

using namespace Opaax; // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace Opaax::Editor
{
    void EditorPanels::Build(const PanelRegistry& InRegistry, EditorContext& InContext)
    {
        for (const PanelEntry& lEntry : InRegistry.Entries())
        {
            TUniquePtr<IEditorPanel> lPanel = lEntry.Factory ? lEntry.Factory(InContext) : nullptr;

            if (lPanel == nullptr)
            {
                OPAAX_LOG(LogEditorPanels, Warn, "Panel '{}' produced no instance — skipped.", lEntry.Desc.Id);
                continue;
            }

            lPanel->Startup();

            m_Panels.emplace_back(lEntry.Desc, Move(lPanel),
                                  lEntry.Desc.DefaultVisibility == EPanelVisibility::Visible);

            // The SUCCESS branch (L15): a silent loop is indistinguishable from one that ran zero times.
            OPAAX_LOG(LogEditorPanels, Info, "Panel '{}' built — menu '{}', starts {}",
                      lEntry.Desc.Id, lEntry.Desc.Menu, ToString(lEntry.Desc.DefaultVisibility));
        }
    }

    void EditorPanels::OnPreRender()
    {
        for (const LivePanel& lLive : m_Panels)
        {
            lLive.Panel->OnPreRender();
        }
    }

    void EditorPanels::Draw()
    {
        //Should be draw from GUI
        for (LivePanel& lLive : m_Panels)
        {
            if (!lLive.bVisible) { continue; }

            const PanelWindowStyle lStyle = lLive.Panel->GetWindowStyle();

            ImGui::SetNextWindowSize(ImVec2(lStyle.DefaultSize.x, lStyle.DefaultSize.y), ImGuiCond_FirstUseEver);

            if (lStyle.bNoPadding) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f)); }

            // ImGui writes the close button straight into whatever it is handed, so it gets a LOCAL:
            // routing the result back through SetVisible below keeps that one method the only thing
            // that ever moves a panel's visibility, and therefore the only thing that has to log it.
            bool lWantVisible = true;

            const bool lOpen = ImGui::Begin(lLive.Desc.Id.CStr(), &lWantVisible);

            if (lStyle.bNoPadding) { ImGui::PopStyleVar(); }

            // End() runs whether or not Begin() returned true — ImGui's contract, and the pairing
            // every panel used to have to get right on its own.
            if (lOpen) { lLive.Panel->DrawContents(); }

            ImGui::End();

            if (!lWantVisible) { SetVisible(lLive.Desc.Id, false); }
        }
    }

    void EditorPanels::Shutdown()
    {
        while (!m_Panels.empty())
        {
            m_Panels.back().Panel->Shutdown();
            m_Panels.pop_back();
        }
    }

    void EditorPanels::OnActiveWorldChanged(World* InOld, World* InNew)
    {
        for (const LivePanel& lLive : m_Panels)
        {
            lLive.Panel->OnActiveWorldChanged(InOld, InNew);
        }
    }

    bool EditorPanels::IsVisible(const OpaaxStringID InID) const noexcept
    {
        const LivePanel* lLive = Find(InID);
        return lLive != nullptr && lLive->bVisible;
    }

    void EditorPanels::SetVisible(const OpaaxStringID InID, const bool bInVisible)
    {
        LivePanel* lLive = Find(InID);

        if (lLive == nullptr)
        {
            OPAAX_LOG(LogEditorPanels, Warn, "No panel '{}' — visibility unchanged.", InID);
            return;
        }

        if (lLive->bVisible == bInVisible) { return; }

        lLive->bVisible = bInVisible;

        // THE one place a panel changes visibility, so this line cannot be bypassed — the menu, the
        // close button and any later caller all arrive here. A menu click is preceded by the node's
        // own "Menu: ..." line; a bare one of these is the close button.
        OPAAX_LOG(LogEditorPanels, Info, "Panel '{}' -> {}", InID, bInVisible ? "shown" : "hidden");
    }

    EditorPanels::LivePanel* EditorPanels::Find(const OpaaxStringID InID) noexcept
    {
        for (LivePanel& lLive : m_Panels)
        {
            if (lLive.Desc.Id == InID) { return &lLive; }
        }

        return nullptr;
    }

    const EditorPanels::LivePanel* EditorPanels::Find(const OpaaxStringID InID) const noexcept
    {
        for (const LivePanel& lLive : m_Panels)
        {
            if (lLive.Desc.Id == InID) { return &lLive; }
        }

        return nullptr;
    }
}
