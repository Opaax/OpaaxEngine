#include "Editor/Panels/InputPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Input/InputRoute.h"

#include "Application/Services/IEngine.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Subsystems/Input/InputManager.h"

#include <imgui.h>

using namespace Opaax;

namespace
{
    // Short display names for the panel (InputKeyNames.h has the full names used in files).
    // Printable keys show as themselves; unknown ones show their code.
    OpaaxString KeyName(EKeyCode InKey)
    {
        switch (InKey)
        {
        case EKeyCode::Space:        return "Space";
        case EKeyCode::Enter:        return "Enter";
        case EKeyCode::Escape:       return "Esc";
        case EKeyCode::Tab:          return "Tab";
        case EKeyCode::Backspace:    return "Backspace";
        case EKeyCode::LeftShift:    return "LShift";
        case EKeyCode::RightShift:   return "RShift";
        case EKeyCode::LeftControl:  return "LCtrl";
        case EKeyCode::RightControl: return "RCtrl";
        case EKeyCode::LeftAlt:      return "LAlt";
        case EKeyCode::RightAlt:     return "RAlt";
        case EKeyCode::Left:         return "Left";
        case EKeyCode::Right:        return "Right";
        case EKeyCode::Up:           return "Up";
        case EKeyCode::Down:         return "Down";
        case EKeyCode::Mouse_Left:   return "LMB";
        case EKeyCode::Mouse_Right:  return "RMB";
        case EKeyCode::Mouse_Middle: return "MMB";
        default:                     break;
        }

        const Uint16 lCode = static_cast<Uint16>(InKey);

        // Function keys are contiguous from F1 = 290.
        if (lCode >= static_cast<Uint16>(EKeyCode::F1) && lCode <= static_cast<Uint16>(EKeyCode::F12))
        {
            return OpaaxString("F") + OpaaxString::FromInt(lCode - static_cast<Uint16>(EKeyCode::F1) + 1);
        }

        // Printable ASCII: the code is the character (GLFW numbering).
        if (lCode >= 33 && lCode <= 126)
        {
            const char lText[2] = { static_cast<char>(lCode), '\0' };
            return OpaaxString(lText);
        }

        return OpaaxString("#") + OpaaxString::FromUInt(lCode);
    }
}

namespace Opaax::Editor
{
    InputPanel::InputPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    InputPanel::~InputPanel() = default;

    void InputPanel::DrawContents()
    {
        const InputManager& lInput = m_Context.Engine.GetInput();

        // ---- Who gets the input. First: when the editor has it, the engine gets nothing and the rest
        //      shows its last values. ------------------------------------------------------------------
        const bool lGameHasIt = m_Context.Route.IsOpen();

        ImGui::Text("Focus:");
        ImGui::SameLine();

        if (lGameHasIt)
        {
            ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "GAME");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.f, 0.6f, 0.2f, 1.f), "EDITOR");
            ImGui::SameLine();
            ImGui::TextDisabled("— %s", ToString(m_Context.Route.GetState()));
        }

        ImGui::Separator();

        // ---- Held keys ----------------------------------------------------------------------
        const TDynArray<EKeyCode> lDown = lInput.GetKeysDown();

        if (lDown.empty())
        {
            ImGui::TextDisabled("Down:   (none)");
        }
        else
        {
            OpaaxString lLine;
            for (const EKeyCode lKey : lDown)
            {
                if (!lLine.IsEmpty()) { lLine += "  "; }
                lLine += KeyName(lKey);
            }

            ImGui::Text("Down:   %s", lLine.CStr());
        }

        // ---- Mouse, in window pixels --------------------------------------------------------------
        const Vector2F lPos   = lInput.GetMousePosition();
        const Vector2F lDelta = lInput.GetMouseDelta();

        ImGui::Text("Mouse:  %.0f, %.0f", lPos.x, lPos.y);
        ImGui::Text("Delta:  %+.0f, %+.0f", lDelta.x, lDelta.y);

        // ---- Scroll, held on screen briefly (a notch lasts one frame). Display only. ----------------
        const Vector2F lScroll = lInput.GetScrollDelta();

        if (lScroll.x != 0.f || lScroll.y != 0.f)
        {
            m_HeldScroll     = lScroll;
            m_ScrollHoldLeft = SCROLL_HOLD_SECONDS;
        }
        else if (m_ScrollHoldLeft > 0.f)
        {
            m_ScrollHoldLeft -= ImGui::GetIO().DeltaTime;
        }

        if (m_ScrollHoldLeft > 0.f)
        {
            ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Scroll: %+.1f, %+.1f", m_HeldScroll.x, m_HeldScroll.y);
        }
        else
        {
            ImGui::TextDisabled("Scroll: 0, 0");
        }

        DrawActions();
    }

    void InputPanel::DrawActions()
    {
        ImGui::Separator();

        // Input mapping lives on the GameInstance: nothing to show while editing (no game).
        const GameInstanceManager& lGames = m_Context.Engine.GetGameInstances();
        const GameInstance*        lGame  = lGames.GetGameInstance();

        if (lGame == nullptr)
        {
            ImGui::TextDisabled("Actions: no game running.");
            ImGui::SetItemTooltip("Input mapping lives on the GameInstance. Press Play.");
            return;
        }

        const InputMappingSubsystem* lActions = lGame->GetSubsystems().GetSubsystem<InputMappingSubsystem>();

        if (lActions == nullptr)
        {
            ImGui::TextDisabled("Actions: the session has no input mapping subsystem.");
            return;
        }

        ImGui::Text("Actions (%llu over %llu context(s), %llu binding(s))",
                    static_cast<unsigned long long>(lActions->GetActionCount()),
                    static_cast<unsigned long long>(lActions->GetContextCount()),
                    static_cast<unsigned long long>(lActions->GetBindingCount()));

        if (lActions->GetActionCount() == 0)
        {
            ImGui::TextDisabled("  (no context added)");
            return;
        }

        // Value and phase together (a value alone cannot tell "held" from "pressed this frame").
        lActions->ForEachAction([](const InputAction& InAction, const InputActionState& InState)
        {
            OpaaxString lPhase;
            if (InState.bStarted)   { lPhase += "Started ";   }
            if (InState.bTriggered) { lPhase += "Triggered "; }
            if (InState.bCompleted) { lPhase += "Completed "; }
            if (InState.bHold)      { lPhase += "Hold ";      }

            const ImVec4 lColour = InState.bTriggered ? ImVec4(0.4f, 1.f, 0.4f, 1.f)
                                                      : ImVec4(0.6f, 0.6f, 0.6f, 1.f);

            ImGui::TextColored(lColour, "  %-12s %-7s (%+.2f, %+.2f) %s",
                               InAction.Name.CStr(), ToString(InAction.ValueType),
                               InState.Value.Value.x, InState.Value.Value.y, lPhase.CStr());
        });
    }
}
