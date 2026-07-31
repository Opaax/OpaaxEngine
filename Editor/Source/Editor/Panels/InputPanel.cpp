#include "Editor/Panels/InputPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/InputRoute.h"

#include "Application/Services/IEngine.h"
#include "Engine/Subsystems/Input/InputManager.h"

#include <imgui.h>

using namespace Opaax;

namespace
{
    // Display names, panel-local on purpose: a real key-name table belongs with the rebinding UI
    // that does not exist yet. Printables render as themselves, the groups a reader actually looks
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
            return OpaaxString(("F" + std::to_string(lCode - static_cast<Uint16>(EKeyCode::F1) + 1)).c_str());
        }

        // Printable ASCII: the code IS the character (the enum follows GLFW numbering).
        if (lCode >= 33 && lCode <= 126)
        {
            const char lText[2] = { static_cast<char>(lCode), '\0' };
            return OpaaxString(lText);
        }

        return OpaaxString(("#" + std::to_string(lCode)).c_str());
    }
}

namespace Opaax::Editor
{
    InputPanel::InputPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    InputPanel::~InputPanel() = default;

    void InputPanel::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(360.f, 220.f), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Title.CStr());

        const InputManager& lInput = m_Context.Engine.GetInput();

        // ---- The route, first: it explains everything below it. A closed route means the lists
        //      are stale by design, not broken, and that distinction is the panel's main job. ----
        const bool lOpen = m_Context.Route.IsOpen();

        ImGui::TextColored(lOpen ? ImVec4(0.4f, 1.f, 0.4f, 1.f) : ImVec4(1.f, 0.6f, 0.2f, 1.f),
                           "Route: %s", ToString(m_Context.Route.GetState()));

        if (!lOpen)
        {
            ImGui::TextDisabled("The engine is not being fed — keys below will not change.");
        }

        ImGui::Separator();

        // ---- Held keys ----------------------------------------------------------------------
        const TDynArray<EKeyCode> lDown = lInput.GetKeysDown();

        if (lDown.empty())
        {
            ImGui::TextDisabled("Down: (none)");
        }
        else
        {
            OpaaxString lLine;
            for (const EKeyCode lKey : lDown)
            {
                if (!lLine.IsEmpty()) { lLine += "  "; }
                lLine += KeyName(lKey);
            }

            ImGui::Text("Down: %s", lLine.CStr());
        }

        // ---- Edges. Latched into the panel because each is true for a single frame, which is
        //      about 16 ms — far too short to read. ---------------------------------------------
        for (const EKeyCode lKey : lDown)
        {
            if (lInput.WasPressedThisFrame(lKey)) { m_LastPressed = KeyName(lKey); }
        }

        // A release cannot be found by walking the held set — the key is no longer in it — so the
        // whole feedable range is swept. Debug-panel work, once a frame, on 512 entries.
        for (Uint16 lCode = 1; lCode < InputManager::KEY_STATE_COUNT; ++lCode)
        {
            const EKeyCode lKey = static_cast<EKeyCode>(lCode);
            if (lInput.WasReleasedThisFrame(lKey)) { m_LastReleased = KeyName(lKey); }
        }

        ImGui::Text("Last: v %s    ^ %s", m_LastPressed.CStr(), m_LastReleased.CStr());

        ImGui::Separator();

        // ---- Mouse. Window pixels — world space needs the viewport rect and the camera. -------
        const Vector2F lPos    = lInput.GetMousePosition();
        const Vector2F lDelta  = lInput.GetMouseDelta();
        const Vector2F lScroll = lInput.GetScrollDelta();

        ImGui::Text("Mouse: %.0f, %.0f", lPos.x, lPos.y);
        ImGui::SameLine();
        ImGui::TextDisabled("(window px)");

        ImGui::Text("Delta: %+.0f, %+.0f     Scroll: %+.1f, %+.1f", lDelta.x, lDelta.y, lScroll.x, lScroll.y);

        ImGui::Separator();
        ImGui::Text("Modifiers: %s %s %s",
                    lInput.IsShiftDown() ? "SHIFT" : "-",
                    lInput.IsCtrlDown()  ? "CTRL"  : "-",
                    lInput.IsAltDown()   ? "ALT"   : "-");

        ImGui::End();
    }
}
