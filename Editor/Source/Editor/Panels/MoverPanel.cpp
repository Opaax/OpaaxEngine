#include "Editor/Panels/MoverPanel.h"

#include <cstdio>   // snprintf — the entry list's row labels

#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/MoverOperations.h"
#include "Editor/Properties/PropertyDrawers.h"   // the specializations DrawProperties folds over
#include "Editor/Resources/Types/Mover/EditorMoverDocument.h"
#include "Editor/Undo/EditorUndo.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    MoverPanel::MoverPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    MoverPanel::~MoverPanel() = default;

    void MoverPanel::DrawContents()
    {
        if (!m_Context.MoverDocument.IsOpen())
        {
            ImGui::TextDisabled("No mover open.");
            ImGui::TextDisabled("Double-click a .opaaxmover in the Resource Browser.");

            m_Selected = -1;
            return;
        }

        MoverData& lData = m_Context.MoverDocument.GetMutableData();

        DrawHeader(lData);
        ImGui::Separator();

        DrawSelectedEntry(lData);
        ImGui::Separator();

        DrawEntryList(lData);
    }

    void MoverPanel::DrawHeader(const MoverData& InData)
    {
        const OpaaxString lName  = m_Context.MoverDocument.FileName();
        const bool        bDirty = m_Context.MoverDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_MOVER, m_Context);
        }
        ImGui::EndDisabled();

        // The default is what a mover naming no mode starts in, so it is picked from the names that
        // exist rather than typed — a typo here would silently fall through to the first entry.
        const char* lCurrent = InData.DefaultMode.IsValid() ? InData.DefaultMode.CStr() : "(first entry)";

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8.f);

        if (ImGui::BeginCombo("Default", lCurrent))
        {
            if (ImGui::Selectable("(first entry)", !InData.DefaultMode.IsValid()))
            {
                MoverOps::SetDefaultMode(m_Context, OpaaxStringID());
            }

            for (const MoverEntry& lEntry : InData.Entries)
            {
                if (!lEntry.Name.IsValid()) { continue; }

                if (ImGui::Selectable(lEntry.Name.CStr(), lEntry.Name == InData.DefaultMode))
                {
                    MoverOps::SetDefaultMode(m_Context, lEntry.Name);
                }
            }

            ImGui::EndCombo();
        }

        ImGui::TextDisabled("Modes : %u", InData.EntryCount());
    }

    void MoverPanel::DrawEntryList(const MoverData& InData)
    {
        if (!ImGui::TreeNodeEx("Modes", ImGuiTreeNodeFlags_DefaultOpen)) { return; }

        if (ImGui::Button("Add Mode")) { MoverOps::AddEntry(m_Context); }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_Selected < 0);

        if (ImGui::Button("Remove") && m_Selected >= 0)
        {
            if (MoverOps::RemoveEntry(m_Context, static_cast<Uint32>(m_Selected)))
            {
                m_Selected = -1;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Up") && m_Selected > 0)
        {
            if (MoverOps::MoveEntry(m_Context, static_cast<Uint32>(m_Selected), -1)) { --m_Selected; }
        }

        ImGui::SameLine();
        if (ImGui::Button("Down") && m_Selected >= 0)
        {
            if (MoverOps::MoveEntry(m_Context, static_cast<Uint32>(m_Selected), 1)) { ++m_Selected; }
        }

        ImGui::EndDisabled();

        for (Uint32 lIndex = 0; lIndex < InData.EntryCount(); ++lIndex)
        {
            const MoverEntry& lEntry = InData.Entries[lIndex];

            // The FIRST row is what a mover falls back to when the default names nothing, so it is
            // worth seeing which one that is without reading the combo.
            const bool bIsFallback = !InData.DefaultMode.IsValid() && lIndex == 0;
            const bool bIsDefault  = InData.DefaultMode.IsValid() && lEntry.Name == InData.DefaultMode;

            char lLabel[224];
            std::snprintf(lLabel, sizeof(lLabel), "%s%s  ->  %s##entry%u",
                          lEntry.Name.IsValid() ? lEntry.Name.CStr() : "(unnamed)",
                          (bIsDefault || bIsFallback) ? "  [default]" : "",
                          lEntry.ModeAsset.IsEmpty() ? "(no tuning)" : lEntry.ModeAsset.Path.CStr(),
                          lIndex);

            if (ImGui::Selectable(lLabel, m_Selected == static_cast<Int32>(lIndex)))
            {
                m_Selected = static_cast<Int32>(lIndex);
            }
        }

        ImGui::TreePop();
    }

    void MoverPanel::DrawSelectedEntry(MoverData& InData)
    {
        if (m_Selected < 0 || static_cast<Uint32>(m_Selected) >= InData.EntryCount())
        {
            ImGui::TextDisabled("Select a mode to edit it.");
            return;
        }

        const Uint32 lIndex = static_cast<Uint32>(m_Selected);

        ImGui::PushID(static_cast<int>(lIndex));
        DrawProperties(m_Context.Widgets, InData.Entries[lIndex]);
        ImGui::PopID();

        // The Inspector's bracket: a TPropertyDrawer writes straight through a reference and cannot
        // report that it did, so the edges of "any item is active" open and close one step. The
        // CLOSE goes through MoverOps, which is what judges the name against the rest of the list.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasItemActive)
        {
            m_GestureBefore = InData.Entries[lIndex];
            m_GestureIndex  = lIndex;
            m_bGestureOpen  = true;
        }
        else if (!lItemActive && m_bWasItemActive && m_bGestureOpen)
        {
            MoverOps::CommitEntryEdit(m_Context, m_GestureIndex, m_GestureBefore);

            m_GestureBefore = MoverEntry{};
            m_bGestureOpen  = false;
        }

        m_bWasItemActive = lItemActive;
    }
}
