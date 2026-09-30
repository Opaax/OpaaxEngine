#include "Editor/Panels/MoveModePanel.h"

#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/MoverOperations.h"
#include "Editor/Properties/PropertyDrawers.h"
#include "Editor/Resources/Types/Mover/EditorMoveModeDocument.h"
#include "Editor/Undo/EditorUndo.h"

#include <imgui.h>

#include <cstring>   // strcmp
#include <tuple>     // std::apply

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        /**
         * Whether InMode uses InField. Tested by name; an unknown mode (e.g. a game's) shows everything.
         */
        bool IsRelevant(const OpaaxStringID InMode, const char* InField)
        {
            // Flight has no ground: gravity, friction, jumping and slopes do not apply.
            if (InMode == OPAAX_ID("FlyMove"))
            {
                return std::strcmp(InField, "MaxSpeed") == 0
                    || std::strcmp(InField, "Acceleration") == 0;
            }

            return true;
        }
    }

    MoveModePanel::MoveModePanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    MoveModePanel::~MoveModePanel() = default;

    void MoveModePanel::DrawContents()
    {
        if (!m_Context.MoveModeDocument.IsOpen())
        {
            ImGui::TextDisabled("No move mode open.");
            ImGui::TextDisabled("Double-click a .opaaxmovemode in the Resource Browser.");
            return;
        }

        DrawHeader();
        ImGui::Separator();

        DrawTuning(m_Context.MoveModeDocument.GetMutableData());
    }

    void MoveModePanel::DrawHeader()
    {
        const OpaaxString lName  = m_Context.MoveModeDocument.FileName();
        const bool        bDirty = m_Context.MoveModeDocument.IsDirty();

        ImGui::Text("%s%s", lName.CStr(), bDirty ? " *" : "");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        ImGui::BeginDisabled(!bDirty);
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_MOVE_MODE, m_Context);
        }
        ImGui::EndDisabled();
    }

    void MoveModePanel::DrawTuning(MoveModeData& InData)
    {
        const OpaaxStringID lMode = InData.Mode;

        // DrawProperties' fold, done by hand so fields the mode ignores can be skipped.
        std::apply([this, lMode, &InData](const auto&... lProperties)
                   {
                       ([&]
                        {
                            if (IsRelevant(lMode, lProperties.Name))
                            {
                                DrawProperty(m_Context.Widgets, lProperties, InData);
                            }
                        }(), ...);
                   },
                   MoveModeData::GetProperties());

        // Like the Inspector: a drawer writes through a reference without reporting it, so an undo step
        // opens when an item becomes active and closes when none is.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasItemActive)
        {
            m_GestureBefore = InData;
            m_bGestureOpen  = true;
        }
        else if (!lItemActive && m_bWasItemActive && m_bGestureOpen)
        {
            MoveModeOps::CommitEdit(m_Context, m_GestureBefore);

            m_GestureBefore = MoveModeData{};
            m_bGestureOpen  = false;
        }

        m_bWasItemActive = lItemActive;
    }
}
