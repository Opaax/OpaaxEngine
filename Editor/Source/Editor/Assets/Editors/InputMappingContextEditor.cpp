#include "Editor/Assets/Editors/InputMappingContextEditor.h"

#include <imgui.h>

#include "Core/String/CString.h"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/Assets/Editors/InputMappingComposite.h"
#include "Editor/EditorContext.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/UI/IEditorWidgets.h"
#include "Input/Assets/InputMappingContextAsset.h"
#include "Input/InputKeyNames.h"

namespace Opaax::Editor
{
    void InputMappingContextEditor::DrawContents(AssetDocument& InDocument)
    {
        InputMappingContextAsset* lAsset = Cast<InputMappingContextAsset>(InDocument.GetAsset());
        if (lAsset == nullptr)
        {
            return;
        }

        // An undo can shorten the list under the selection.
        if (m_Selected >= static_cast<Int32>(lAsset->EntryCount()))
        {
            m_Selected = -1;
        }

        DrawHeader(*lAsset);
        ImGui::Separator();

        DrawMappingList(*lAsset);
        ImGui::Separator();

        DrawSelectedMapping(*lAsset);
    }

    void InputMappingContextEditor::DrawHeader(InputMappingContextAsset& InAsset)
    {
        TupleApply([this, &InAsset](const auto&... lProperties)
                   {
                       ([&]
                        {
                            if (!CString::Equals(lProperties.Name, "Mappings"))
                            {
                                DrawProperty(m_Context.Widgets, lProperties, InAsset);
                            }
                        }(), ...);
                   },
                   InputMappingContextAsset::GetProperties());
    }

    void InputMappingContextEditor::DrawMappingList(InputMappingContextAsset& InAsset)
    {
        TDynArray<InputMappingEntry>& lMappings = InAsset.Mappings;

        if (ImGui::Button("Add"))
        {
            lMappings.emplace_back();
            m_Selected = static_cast<Int32>(lMappings.size()) - 1;
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_Selected < 0);

        if (ImGui::Button("Remove") && m_Selected >= 0)
        {
            lMappings.erase(lMappings.begin() + m_Selected);
            m_Selected = -1;
        }

        ImGui::SameLine();
        if (ImGui::Button("Up") && m_Selected > 0)
        {
            const Uint32 lIndex = static_cast<Uint32>(m_Selected);
            Swap(lMappings[lIndex], lMappings[lIndex - 1]);
            --m_Selected;
        }

        ImGui::SameLine();
        if (ImGui::Button("Down") && m_Selected >= 0 && m_Selected + 1 < static_cast<Int32>(lMappings.size()))
        {
            const Uint32 lIndex = static_cast<Uint32>(m_Selected);
            Swap(lMappings[lIndex], lMappings[lIndex + 1]);
            ++m_Selected;
        }

        ImGui::EndDisabled();

        // Four mappings with Negate/Swizzle make an Axis2D: one click instead of four rows by hand.
        const bool bCanComposite = m_Selected >= 0 && !lMappings[static_cast<Uint32>(m_Selected)].Action.IsEmpty();
        ImGui::BeginDisabled(!bCanComposite);

        if (ImGui::Button("2D Composite (WASD)") && bCanComposite)
        {
            ReplaceWithComposite2D(lMappings, static_cast<Uint32>(m_Selected),
                                   EKeyCode::W, EKeyCode::S, EKeyCode::A, EKeyCode::D);
        }
        ImGui::SetItemTooltip("Replaces the selected mapping with four, one per direction, on its action.");

        ImGui::SameLine();
        if (ImGui::Button("Arrows") && bCanComposite)
        {
            ReplaceWithComposite2D(lMappings, static_cast<Uint32>(m_Selected),
                                   EKeyCode::Up, EKeyCode::Down, EKeyCode::Left, EKeyCode::Right);
        }
        ImGui::SetItemTooltip("The same with the arrow keys.");

        ImGui::EndDisabled();

        if (lMappings.empty())
        {
            ImGui::TextDisabled("No mappings yet: add one, then pick its action and key.");
            return;
        }

        // The key first (what a rebind looks for), then the action.
        constexpr ImGuiTableFlags lFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

        if (!ImGui::BeginTable("##mappings", 2, lFlags))
        {
            return;
        }

        ImGui::TableSetupColumn("Key",    ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("Numpad_Subtract").x);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (Uint32 lIndex = 0; lIndex < lMappings.size(); ++lIndex)
        {
            const InputMappingEntry& lEntry = lMappings[lIndex];

            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(lIndex));

            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(ToString(lEntry.Key), m_Selected == static_cast<Int32>(lIndex),
                                  ImGuiSelectableFlags_SpanAllColumns))
            {
                m_Selected = static_cast<Int32>(lIndex);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(lEntry.Action.IsEmpty() ? "(no action)" : lEntry.Action.Path.CStr());

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    void InputMappingContextEditor::DrawSelectedMapping(InputMappingContextAsset& InAsset)
    {
        if (m_Selected < 0)
        {
            ImGui::TextDisabled("Select a mapping to edit it.");
            return;
        }

        InputMappingEntry& lEntry = InAsset.Mappings[static_cast<Uint32>(m_Selected)];

        ImGui::PushID(m_Selected);

        // The generic drawers, but the key's: its combo leaves out what AddContext would refuse.
        TupleApply([this, &lEntry](const auto&... lProperties)
                   {
                       ([&]
                        {
                            if (CString::Equals(lProperties.Name, "Key"))
                            {
                                DrawKeyCombo(lEntry.Key);
                                DrawPropertyNote(m_Context.Widgets, lProperties.Meta);
                            }
                            else
                            {
                                DrawProperty(m_Context.Widgets, lProperties, lEntry);
                            }
                        }(), ...);
                   },
                   InputMappingEntry::GetProperties());

        ImGui::PopID();
    }

    void InputMappingContextEditor::DrawKeyCombo(EKeyCode& InOutKey)
    {
        IEditorWidgets& lWidgets = m_Context.Widgets;

        if (!lWidgets.BeginCombo("Key", ToString(InOutKey)))
        {
            return;
        }

        for (const EKeyCode lCandidate : TEnumValues<EKeyCode>::Values)
        {
            // Gamepad codes have no input yet: AddContext would skip them.
            if (!IsKeyCodeBindable(lCandidate))
            {
                continue;
            }

            const bool bSelected = lCandidate == InOutKey;
            if (lWidgets.Selectable(ToString(lCandidate), bSelected))
            {
                InOutKey = lCandidate;
            }

            if (bSelected)
            {
                lWidgets.SetDefaultFocus();
            }
        }

        lWidgets.EndCombo();
    }
}
