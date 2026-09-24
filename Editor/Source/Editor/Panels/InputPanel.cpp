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
    // Display names, panel-local on purpose. The canonical table now EXISTS
    // (Engine/Subsystems/Input/InputKeyNames.h) and this is deliberately still not it: that one is
    // what a .opaaxinputmap writes, so it spells every code out. These abbreviate, which is right
    // in a panel and wrong in a file format (IM10). Printables render as themselves, the groups a reader actually looks
    // for get names, and anything else shows its code rather than a lie.
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

        // Printable ASCII: the code IS the character (the enum follows GLFW numbering).
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

        // ---- Who is getting the input. First, because it explains every line below it: when the
        //      editor owns it, the engine is not being told anything and the rest is frozen at its
        //      last value — stale by design, not broken. -----------------------------------------
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

        // ---- Mouse. Window pixels — world space needs the viewport rect and the camera. -------
        const Vector2F lPos   = lInput.GetMousePosition();
        const Vector2F lDelta = lInput.GetMouseDelta();

        ImGui::Text("Mouse:  %.0f, %.0f", lPos.x, lPos.y);
        ImGui::Text("Delta:  %+.0f, %+.0f", lDelta.x, lDelta.y);

        // ---- Scroll, HELD. A wheel notch is one frame of non-zero and then gone — about 16 ms,
        //      which is unreadable. The last non-zero value stays up briefly so the eye can catch
        //      it; the engine's own value is untouched, this is display only. --------------------
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

        // The SESSION, not the world: mapping lives on the GameInstance, so there is nothing to
        // show while the editor is authoring. That absence is the honest answer, and it is the
        // same one the log gives as "0 game session(s) ran" (GI1).
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

        // The VALUE and the PHASE together. A value alone cannot tell "held" from "pressed this
        // frame", which is exactly the distinction a binding is written against, so a panel
        // showing only the number would leave the trigger half unobservable.
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
