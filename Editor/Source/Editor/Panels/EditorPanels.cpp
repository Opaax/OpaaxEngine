#include "Editor/Panels/EditorPanels.h"

#include "Editor/Extensions/PanelRegistry.h"
#include "Editor/UI/IEditorGui.h"

using namespace Opaax;

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
        }
    }

    void EditorPanels::OnPreRender()
    {
        for (const LivePanel& lLive : m_Panels)
        {
            lLive.Panel->OnPreRender();
        }
    }

    void EditorPanels::Draw(IEditorGui& InGui)
    {
        // Recomputed every frame (a closed panel must not keep the focus).
        m_Focused = OpaaxStringID();

        for (LivePanel& lLive : m_Panels)
        {
            if (!lLive.bVisible) { continue; }

            // The close button writes into a local; SetVisible below is the only thing that changes visibility.
            bool lWantVisible = true;

            const bool lOpen = InGui.BeginPanelWindow(lLive.Desc.Id.CStr(),
                                                      lLive.Panel->GetWindowStyle(), lWantVisible);

            // Asked inside the window bracket. A collapsed panel can still hold focus.
            if (InGui.IsPanelWindowFocused()) { m_Focused = lLive.Desc.Id; }

            // EndPanelWindow runs whether or not the body opened.
            if (lOpen) { lLive.Panel->DrawContents(); }

            InGui.EndPanelWindow();

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

        // The only place a panel's visibility changes (menu, close button, ...). A menu click logs its own
        // line first; a bare one of these is the close button.
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
