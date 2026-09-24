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

#include <cstring>   // strcmp — IsRelevant tests a property's NAME
#include <tuple>     // std::apply — the property fold, opened up so a field can be skipped

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        /**
         * Whether InField is a knob InMode actually reads.
         *
         * The one place the editor knows what a mode uses. It is a NAME test rather than a registry
         * lookup because the registry answers with an IMoverMode, which has no opinion about
         * presentation — and a mode a game registers is unknown here, so it falls through to
         * showing EVERYTHING, which is the honest answer for a mode this editor has never met.
         */
        bool IsRelevant(const OpaaxStringID InMode, const char* InField)
        {
            // Flight has no ground under it: gravity, friction, jumping and slopes are all silent.
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

        // DrawProperties' own fold, opened up so each field can be SKIPPED: which knobs matter
        // depends on the mode, and hiding the ones it ignores is the difference between a tuning
        // panel and a list of every float the format can hold.
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

        // The Inspector's bracket: a TPropertyDrawer writes straight through a reference and cannot
        // report that it did, so the edges of "any item is active" open and close one step.
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
