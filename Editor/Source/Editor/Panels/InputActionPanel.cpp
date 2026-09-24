#include "Editor/Panels/InputActionPanel.h"

#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/InputOperations.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/Resources/Types/Input/EditorInputActionDocument.h"
#include "Editor/Undo/EditorUndo.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    InputActionPanel::InputActionPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    InputActionPanel::~InputActionPanel() = default;

    void InputActionPanel::DrawContents()
    {
        if (!m_Context.InputActionDocument.IsOpen())
        {
            ImGui::TextDisabled("No input action open.");
            ImGui::TextDisabled("Double-click a .opaaxaction in the Resource Browser.");
            return;
        }

        DrawHeader();
        ImGui::Separator();

        InputActionData& lData = m_Context.InputActionDocument.GetMutableData();

        DrawFields(lData);

        ImGui::Separator();
        DrawModifiers(lData);
    }

    void InputActionPanel::DrawHeader()
    {
        const OpaaxString lName  = m_Context.InputActionDocument.FileName();
        const bool        bDirty = m_Context.InputActionDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_INPUT_ACTION, m_Context);
        }
        ImGui::EndDisabled();
    }

    void InputActionPanel::DrawFields(InputActionData& InData)
    {
        // Modifiers is a TDynArray with no OPAAX_PROP, so the fold skips it on its own and the
        // list below owns it — the same split MoverData and SpriteSheetData make.
        DrawProperties(m_Context.Widgets, InData);
    }

    void InputActionPanel::DrawModifiers(InputActionData& InData)
    {
        ImGui::TextUnformatted("Modifiers (applied to the SUM of every binding)");

        if (ImGui::Button("Add")) { InputActionOps::AddModifier(m_Context); }

        ImGui::SameLine();
        ImGui::BeginDisabled(m_SelectedModifier < 0);

        if (ImGui::Button("Remove") && m_SelectedModifier >= 0)
        {
            if (InputActionOps::RemoveModifier(m_Context, static_cast<Uint32>(m_SelectedModifier)))
            {
                m_SelectedModifier = -1;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Up") && m_SelectedModifier > 0)
        {
            if (InputActionOps::MoveModifier(m_Context, static_cast<Uint32>(m_SelectedModifier), -1))
            {
                --m_SelectedModifier;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Down") && m_SelectedModifier >= 0)
        {
            if (InputActionOps::MoveModifier(m_Context, static_cast<Uint32>(m_SelectedModifier), 1))
            {
                ++m_SelectedModifier;
            }
        }

        ImGui::EndDisabled();

        // ORDER IS SHOWN because order is authored: the index prefix is what makes
        // "DeadZone then Scalar" legible as different from the reverse (**IM5**).
        for (Uint32 lIndex = 0; lIndex < InData.Modifiers.size(); ++lIndex)
        {
            ImGui::PushID(static_cast<int>(lIndex));

            char lLabel[64];
            std::snprintf(lLabel, sizeof(lLabel), "%u. %s", lIndex, ToString(InData.Modifiers[lIndex].Type));

            if (ImGui::Selectable(lLabel, m_SelectedModifier == static_cast<Int32>(lIndex)))
            {
                m_SelectedModifier = static_cast<Int32>(lIndex);
            }

            ImGui::PopID();
        }

        if (m_SelectedModifier >= 0 && static_cast<Uint32>(m_SelectedModifier) < InData.Modifiers.size())
        {
            ImGui::Separator();
            ImGui::PushID(m_SelectedModifier);
            DrawProperties(m_Context.Widgets, InData.Modifiers[static_cast<Uint32>(m_SelectedModifier)]);
            ImGui::PopID();
        }

        // The Inspector's bracket, covering the WHOLE panel: a TPropertyDrawer writes straight
        // through a reference and cannot report that it did, so the edges of "any item is active"
        // open and close one step. One bracket for fields and modifier knobs alike, because both
        // are edits to the same document and an author does not think of them as different.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasItemActive)
        {
            m_GestureBefore = InData;
            m_bGestureOpen  = true;
        }
        else if (!lItemActive && m_bWasItemActive && m_bGestureOpen)
        {
            InputActionOps::CommitEdit(m_Context, m_GestureBefore);

            m_GestureBefore = InputActionData{};
            m_bGestureOpen  = false;
        }

        m_bWasItemActive = lItemActive;
    }
}
