#include "Editor/Panels/AnimationLibraryPanel.h"

#include <cstdio>   // snprintf

#include "Editor/Resources/Types/Animation/EditorAnimationLibraryDocument.h"
#include "Editor/EditorContext.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/LibraryOperations.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/Undo/EditorUndo.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    AnimationLibraryPanel::AnimationLibraryPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    AnimationLibraryPanel::~AnimationLibraryPanel() = default;

    void AnimationLibraryPanel::DrawContents()
    {
        if (!m_Context.LibraryDocument.IsOpen())
        {
            ImGui::TextDisabled("No animation library open.");
            ImGui::TextDisabled("Double-click a .opaaxanim in the Resource Browser.");

            m_Selected = -1;
            return;
        }

        AnimationLibraryData& lData = m_Context.LibraryDocument.GetMutableData();

        DrawHeader(lData);
        ImGui::Separator();

        DrawSelectedEntry(lData);
        ImGui::Separator();

        DrawEntryList(lData);
    }

    void AnimationLibraryPanel::DrawHeader(const AnimationLibraryData& InData)
    {
        const OpaaxString lName  = m_Context.LibraryDocument.FileName();
        const bool        bDirty = m_Context.LibraryDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_LIBRARY, m_Context);
        }
        ImGui::EndDisabled();

        // Picked from existing names rather than typed (a typo would fall through to the first entry).
        const char* lCurrent = InData.DefaultClip.IsValid() ? InData.DefaultClip.CStr() : "(first entry)";

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8.f);

        if (ImGui::BeginCombo("Default", lCurrent))
        {
            if (ImGui::Selectable("(first entry)", !InData.DefaultClip.IsValid()))
            {
                LibraryOps::SetDefaultClip(m_Context, OpaaxStringID());
            }

            for (const AnimationLibraryEntry& lEntry : InData.Entries)
            {
                if (!lEntry.Name.IsValid()) { continue; }

                if (ImGui::Selectable(lEntry.Name.CStr(), lEntry.Name == InData.DefaultClip))
                {
                    LibraryOps::SetDefaultClip(m_Context, lEntry.Name);
                }
            }

            ImGui::EndCombo();
        }

        ImGui::TextDisabled("Clips : %u", InData.EntryCount());
    }

    void AnimationLibraryPanel::DrawEntryList(const AnimationLibraryData& InData)
    {
        if (!ImGui::TreeNodeEx("Clips", ImGuiTreeNodeFlags_DefaultOpen)) { return; }

        if (ImGui::Button("Add Clip")) { LibraryOps::AddEntry(m_Context); }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_Selected < 0);

        if (ImGui::Button("Remove") && m_Selected >= 0)
        {
            if (LibraryOps::RemoveEntry(m_Context, static_cast<Uint32>(m_Selected)))
            {
                m_Selected = -1;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Up") && m_Selected > 0)
        {
            if (LibraryOps::MoveEntry(m_Context, static_cast<Uint32>(m_Selected), -1)) { --m_Selected; }
        }

        ImGui::SameLine();
        if (ImGui::Button("Down") && m_Selected >= 0)
        {
            if (LibraryOps::MoveEntry(m_Context, static_cast<Uint32>(m_Selected), 1)) { ++m_Selected; }
        }

        ImGui::EndDisabled();

        for (Uint32 lIndex = 0; lIndex < InData.EntryCount(); ++lIndex)
        {
            const AnimationLibraryEntry& lEntry = InData.Entries[lIndex];

            // The first row is the fallback when the default names nothing: show which one it is.
            const bool bIsFallback = !InData.DefaultClip.IsValid() && lIndex == 0;
            const bool bIsDefault  = InData.DefaultClip.IsValid() && lEntry.Name == InData.DefaultClip;

            char lLabel[224];
            std::snprintf(lLabel, sizeof(lLabel), "%s%s  ->  %s##entry%u",
                          lEntry.Name.IsValid() ? lEntry.Name.CStr() : "(unnamed)",
                          (bIsDefault || bIsFallback) ? "  [default]" : "",
                          lEntry.Clip.IsEmpty() ? "(no clip)" : lEntry.Clip.Path.CStr(),
                          lIndex);

            if (ImGui::Selectable(lLabel, m_Selected == static_cast<Int32>(lIndex)))
            {
                m_Selected = static_cast<Int32>(lIndex);
            }
        }

        ImGui::TreePop();
    }

    void AnimationLibraryPanel::DrawSelectedEntry(AnimationLibraryData& InData)
    {
        if (m_Selected < 0 || static_cast<Uint32>(m_Selected) >= InData.EntryCount())
        {
            ImGui::TextDisabled("Select a clip to edit it.");
            return;
        }

        const Uint32 lIndex = static_cast<Uint32>(m_Selected);

        ImGui::PushID(static_cast<int>(lIndex));
        DrawProperties(m_Context.Widgets, InData.Entries[lIndex]);
        ImGui::PopID();

        // Like the Inspector: a drawer writes through a reference without reporting it, so an undo step
        // opens when an item becomes active and closes when none is.
        // The close goes through LibraryOps, which checks the name against the list.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasItemActive)
        {
            m_GestureBefore = InData.Entries[lIndex];
            m_GestureIndex  = lIndex;
            m_bGestureOpen  = true;
        }
        else if (!lItemActive && m_bWasItemActive && m_bGestureOpen)
        {
            LibraryOps::CommitEntryEdit(m_Context, m_GestureIndex, m_GestureBefore);

            m_GestureBefore = AnimationLibraryEntry{};
            m_bGestureOpen  = false;
        }

        m_bWasItemActive = lItemActive;
    }
}
