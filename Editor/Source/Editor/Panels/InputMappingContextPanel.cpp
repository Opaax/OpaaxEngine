#include "Editor/Panels/InputMappingContextPanel.h"

#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/InputOperations.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/Resources/Types/Input/EditorInputMappingContextDocument.h"
#include "Editor/Undo/EditorUndo.h"

#include "Engine/Subsystems/Input/InputKeyNames.h"

#include <imgui.h>

#include <cstdio>
#include <cstring>
#include <tuple>

using namespace Opaax;

namespace Opaax::Editor
{
    InputMappingContextPanel::InputMappingContextPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    InputMappingContextPanel::~InputMappingContextPanel() = default;

    void InputMappingContextPanel::DrawContents()
    {
        if (!m_Context.InputMapDocument.IsOpen())
        {
            ImGui::TextDisabled("No input mapping context open.");
            ImGui::TextDisabled("Double-click a .opaaxinputmap in the Resource Browser.");
            return;
        }

        InputMappingContextData& lData = m_Context.InputMapDocument.GetMutableData();

        DrawHeader(lData);
        ImGui::Separator();

        DrawMappingList(lData);
        ImGui::Separator();

        DrawSelectedMapping(lData);
    }

    void InputMappingContextPanel::DrawHeader(const InputMappingContextData& InData)
    {
        const OpaaxString lName  = m_Context.InputMapDocument.FileName();
        const bool        bDirty = m_Context.InputMapDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_INPUT_MAP, m_Context);
        }
        ImGui::EndDisabled();

        // Its own verb and its own undo step: priority is not a list edit, and a context that
        // outranks another is the whole mechanism behind a menu swallowing Jump (**IM6**).
        int lPriority = static_cast<int>(InData.Priority);
        if (ImGui::InputInt("Priority", &lPriority))
        {
            InputMapOps::SetPriority(m_Context, static_cast<Int32>(lPriority));
        }

        ImGui::SetItemTooltip("Higher contexts are evaluated first and consume keys first.");
    }

    void InputMappingContextPanel::DrawMappingList(const InputMappingContextData& InData)
    {
        if (ImGui::Button("Add")) { InputMapOps::AddMapping(m_Context); }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_Selected < 0);

        if (ImGui::Button("Remove") && m_Selected >= 0)
        {
            if (InputMapOps::RemoveMapping(m_Context, static_cast<Uint32>(m_Selected)))
            {
                m_Selected         = -1;
                m_SelectedModifier = -1;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Up") && m_Selected > 0)
        {
            if (InputMapOps::MoveMapping(m_Context, static_cast<Uint32>(m_Selected), -1)) { --m_Selected; }
        }

        ImGui::SameLine();
        if (ImGui::Button("Down") && m_Selected >= 0)
        {
            if (InputMapOps::MoveMapping(m_Context, static_cast<Uint32>(m_Selected), 1)) { ++m_Selected; }
        }

        ImGui::EndDisabled();

        // FOUR bindings plus Negate/Swizzle is the correct way to build an Axis2D and a miserable
        // thing to type. One click, and no composite concept in the file format (**IM5**).
        ImGui::BeginDisabled(m_Selected < 0);

        if (ImGui::Button("Add 2D Composite (WASD)") && m_Selected >= 0)
        {
            if (InputMapOps::AddComposite2D(m_Context, static_cast<Uint32>(m_Selected),
                                            EKeyCode::W, EKeyCode::S, EKeyCode::A, EKeyCode::D))
            {
                m_SelectedModifier = -1;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Arrows") && m_Selected >= 0)
        {
            if (InputMapOps::AddComposite2D(m_Context, static_cast<Uint32>(m_Selected),
                                            EKeyCode::Up, EKeyCode::Down, EKeyCode::Left, EKeyCode::Right))
            {
                m_SelectedModifier = -1;
            }
        }

        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Replaces the selected mapping with four, one per direction.");

        for (Uint32 lIndex = 0; lIndex < InData.Mappings.size(); ++lIndex)
        {
            const InputMappingEntry& lEntry = InData.Mappings[lIndex];

            ImGui::PushID(static_cast<int>(lIndex));

            // The KEY leads, because that is what an author is looking for when rebinding. The
            // action's file stem follows it; the full path is in the fold below.
            char lLabel[160];
            std::snprintf(lLabel, sizeof(lLabel), "%-14s  %s",
                          ToString(lEntry.Key),
                          lEntry.Action.IsEmpty() ? "(no action)" : lEntry.Action.Path.CStr());

            if (ImGui::Selectable(lLabel, m_Selected == static_cast<Int32>(lIndex)))
            {
                m_Selected         = static_cast<Int32>(lIndex);
                m_SelectedModifier = -1;
            }

            ImGui::PopID();
        }
    }

    bool InputMappingContextPanel::DrawKeyCombo(EKeyCode& InOutKey)
    {
        bool bChanged = false;

        if (!ImGui::BeginCombo("Key", ToString(InOutKey)))
        {
            return false;
        }

        for (const EKeyCode lCandidate : TEnumValues<EKeyCode>::Values)
        {
            // The gamepad range is reserved and unfed (**IN7**), so offering it here would let an
            // author pick a binding that AddContext refuses at load. Filtered rather than
            // explained.
            if (!IsKeyCodeBindable(lCandidate))
            {
                continue;
            }

            const bool bSelected = lCandidate == InOutKey;

            if (ImGui::Selectable(ToString(lCandidate), bSelected))
            {
                InOutKey = lCandidate;
                bChanged = true;
            }

            if (bSelected) { ImGui::SetItemDefaultFocus(); }
        }

        ImGui::EndCombo();
        return bChanged;
    }

    void InputMappingContextPanel::DrawSelectedMapping(InputMappingContextData& InData)
    {
        if (m_Selected < 0 || static_cast<Uint32>(m_Selected) >= InData.EntryCount())
        {
            ImGui::TextDisabled("Select a mapping to edit it.");
            return;
        }

        const Uint32       lIndex = static_cast<Uint32>(m_Selected);
        InputMappingEntry& lEntry = InData.Mappings[lIndex];

        ImGui::PushID(static_cast<int>(lIndex));

        // DrawProperties' own fold, opened up so `Key` can be SKIPPED — it is drawn below by a
        // combo that hides the unfed gamepad range, which the generic enum drawer cannot do.
        std::apply([this, &lEntry](const auto&... lProperties)
                   {
                       ([&]
                        {
                            if (std::strcmp(lProperties.Name, "Key") != 0)
                            {
                                DrawProperty(m_Context.Widgets, lProperties, lEntry);
                            }
                        }(), ...);
                   },
                   InputMappingEntry::GetProperties());

        DrawKeyCombo(lEntry.Key);

        // ---- the entry's own modifier pipeline ----------------------------------------------
        ImGui::Separator();
        ImGui::TextUnformatted("Modifiers (this key only)");

        if (ImGui::Button("Add##mod")) { InputMapOps::AddMappingModifier(m_Context, lIndex); }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedModifier < 0);

        if (ImGui::Button("Remove##mod") && m_SelectedModifier >= 0)
        {
            if (InputMapOps::RemoveMappingModifier(m_Context, lIndex,
                                                   static_cast<Uint32>(m_SelectedModifier)))
            {
                m_SelectedModifier = -1;
            }
        }

        ImGui::EndDisabled();

        for (Uint32 lMod = 0; lMod < lEntry.Modifiers.size(); ++lMod)
        {
            ImGui::PushID(static_cast<int>(1000 + lMod));

            char lLabel[64];
            std::snprintf(lLabel, sizeof(lLabel), "%u. %s", lMod, ToString(lEntry.Modifiers[lMod].Type));

            if (ImGui::Selectable(lLabel, m_SelectedModifier == static_cast<Int32>(lMod)))
            {
                m_SelectedModifier = static_cast<Int32>(lMod);
            }

            ImGui::PopID();
        }

        if (m_SelectedModifier >= 0 && static_cast<Uint32>(m_SelectedModifier) < lEntry.Modifiers.size())
        {
            ImGui::PushID(2000 + m_SelectedModifier);
            DrawProperties(m_Context.Widgets, lEntry.Modifiers[static_cast<Uint32>(m_SelectedModifier)]);
            ImGui::PopID();
        }

        ImGui::PopID();

        // The Inspector's bracket. The CLOSE goes through InputMapOps, which records the whole
        // list before and after — one step for a rebind, exactly as it is one action to a reader.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasItemActive)
        {
            m_GestureBefore = lEntry;
            m_GestureIndex  = lIndex;
            m_bGestureOpen  = true;
        }
        else if (!lItemActive && m_bWasItemActive && m_bGestureOpen)
        {
            InputMapOps::CommitMappingEdit(m_Context, m_GestureIndex, m_GestureBefore);

            m_GestureBefore = InputMappingEntry{};
            m_bGestureOpen  = false;
        }

        m_bWasItemActive = lItemActive;
    }
}
