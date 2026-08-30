#include "Editor/Imgui/EditorGui.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"   // OPAAX_ASSERT
#include "Core/Window/Window.h"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/UI/OpenGLEditorUIBackend.h"

using namespace Opaax; // OPAAX_LOG expands to an unqualified ToSpdLevel(...)

namespace
{
    constexpr LogCategory LogEditorGui{"EditorGui"};

    /** The distance of InKey above InFirst, for a range this table walks by arithmetic. */
    constexpr Int32 Offset(const EKeyCode InKey, const EKeyCode InFirst) noexcept
    {
        return static_cast<Int32>(InKey) - static_cast<Int32>(InFirst);
    }

    /**
     * EKeyCode -> ImGuiKey, total over the keyboard.
     *
     * EKeyCode follows GLFW's numbering, so the contiguous runs are the same ones
     * imgui_impl_glfw.cpp walks by arithmetic in its own KeyToImGuiKey; the rest is by name, which
     * is also what keeps this correct where the engine's numbering has drifted from GLFW's (the
     * numpad operators — see the FIXME in InputCodes.h).
     *
     * Deliberately covers the WHOLE keyboard rather than the keys in use: a partial table answering
     * "no such key" for the next shortcut anyone adds would fail silently.
     *
     * @return ImGuiKey_None for a non-keyboard code (mouse, gamepad) — a call-site error, which
     *   Shortcut asserts on.
     */
    ImGuiKey ToImGuiKey(const EKeyCode InKey) noexcept
    {
        if (InKey >= EKeyCode::A && InKey <= EKeyCode::Z)
        {
            return static_cast<ImGuiKey>(ImGuiKey_A + Offset(InKey, EKeyCode::A));
        }
        if (InKey >= EKeyCode::Zero && InKey <= EKeyCode::Nine)
        {
            return static_cast<ImGuiKey>(ImGuiKey_0 + Offset(InKey, EKeyCode::Zero));
        }
        if (InKey >= EKeyCode::F1 && InKey <= EKeyCode::F12)
        {
            return static_cast<ImGuiKey>(ImGuiKey_F1 + Offset(InKey, EKeyCode::F1));
        }
        if (InKey >= EKeyCode::Numpad_Zero && InKey <= EKeyCode::Numpad_Nine)
        {
            return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + Offset(InKey, EKeyCode::Numpad_Zero));
        }

        switch (InKey)
        {
        case EKeyCode::Space:           return ImGuiKey_Space;
        case EKeyCode::Apostrophe:      return ImGuiKey_Apostrophe;
        case EKeyCode::Comma:           return ImGuiKey_Comma;
        case EKeyCode::Minus:           return ImGuiKey_Minus;
        case EKeyCode::Period:          return ImGuiKey_Period;
        case EKeyCode::Slash:           return ImGuiKey_Slash;
        case EKeyCode::Semicolon:       return ImGuiKey_Semicolon;
        case EKeyCode::Equals:          return ImGuiKey_Equal;
        case EKeyCode::LeftBracket:     return ImGuiKey_LeftBracket;
        case EKeyCode::Backslash:       return ImGuiKey_Backslash;
        case EKeyCode::RightBracket:    return ImGuiKey_RightBracket;
        case EKeyCode::GraveAccent:     return ImGuiKey_GraveAccent;

        case EKeyCode::Escape:          return ImGuiKey_Escape;
        case EKeyCode::Enter:           return ImGuiKey_Enter;
        case EKeyCode::Tab:             return ImGuiKey_Tab;
        case EKeyCode::Backspace:       return ImGuiKey_Backspace;
        case EKeyCode::Insert:          return ImGuiKey_Insert;
        case EKeyCode::Delete:          return ImGuiKey_Delete;

        case EKeyCode::Right:           return ImGuiKey_RightArrow;
        case EKeyCode::Left:            return ImGuiKey_LeftArrow;
        case EKeyCode::Down:            return ImGuiKey_DownArrow;
        case EKeyCode::Up:              return ImGuiKey_UpArrow;

        case EKeyCode::PageUp:          return ImGuiKey_PageUp;
        case EKeyCode::PageDown:        return ImGuiKey_PageDown;
        case EKeyCode::Home:            return ImGuiKey_Home;
        case EKeyCode::End:             return ImGuiKey_End;

        case EKeyCode::CapsLock:        return ImGuiKey_CapsLock;
        case EKeyCode::ScrollLock:      return ImGuiKey_ScrollLock;
        case EKeyCode::NumLock:         return ImGuiKey_NumLock;
        case EKeyCode::PrintScreen:     return ImGuiKey_PrintScreen;
        case EKeyCode::Pause:           return ImGuiKey_Pause;

        case EKeyCode::Numpad_Add:      return ImGuiKey_KeypadAdd;
        case EKeyCode::Numpad_Subtract: return ImGuiKey_KeypadSubtract;
        case EKeyCode::Numpad_Multiply: return ImGuiKey_KeypadMultiply;
        case EKeyCode::Numpad_Divide:   return ImGuiKey_KeypadDivide;
        case EKeyCode::Numpad_Enter:    return ImGuiKey_KeypadEnter;
        case EKeyCode::Numpad_Decimal:  return ImGuiKey_KeypadDecimal;

        case EKeyCode::LeftShift:       return ImGuiKey_LeftShift;
        case EKeyCode::LeftControl:     return ImGuiKey_LeftCtrl;
        case EKeyCode::LeftAlt:         return ImGuiKey_LeftAlt;
        case EKeyCode::LeftSuper:       return ImGuiKey_LeftSuper;
        case EKeyCode::RightShift:      return ImGuiKey_RightShift;
        case EKeyCode::RightControl:    return ImGuiKey_RightCtrl;
        case EKeyCode::RightAlt:        return ImGuiKey_RightAlt;
        case EKeyCode::RightSuper:      return ImGuiKey_RightSuper;

        default:                        return ImGuiKey_None;
        }
    }

    /** The chord bit for a modifier KEY. Left and Right fold together — ImGui's mods are side-agnostic. */
    ImGuiKeyChord ToModifier(const EKeyCode InKey) noexcept
    {
        switch (InKey)
        {
        case EKeyCode::None:                                    return 0;
        case EKeyCode::LeftShift:   case EKeyCode::RightShift:   return ImGuiMod_Shift;
        case EKeyCode::LeftControl: case EKeyCode::RightControl: return ImGuiMod_Ctrl;
        case EKeyCode::LeftAlt:     case EKeyCode::RightAlt:     return ImGuiMod_Alt;
        case EKeyCode::LeftSuper:   case EKeyCode::RightSuper:   return ImGuiMod_Super;

        default:
            OPAAX_ASSERT(false);   // not a modifier key — a call-site error, not a runtime state
            return 0;
        }
    }
}

namespace Opaax::Editor
{
    bool EditorGui::Init(Window& InWindow, OpaaxString InLayoutIniPath)
    {
        // Checked BEFORE the context exists, so a failure has nothing to unwind.
        auto* lNativeWindow = static_cast<GLFWwindow*>(InWindow.GetNativeWindow());
        if (lNativeWindow == nullptr)
        {
            OPAAX_LOG(LogEditorGui, Error, "Main window has no native handle — editor UI not created.");
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& lIO = ImGui::GetIO();
        lIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // A panel moves by its TITLE BAR, never by its body. ImGui's default lets a drag on a
        // FLOATING window's background move the window, and ImGui::Image is not an interactive item
        // — so a marquee drawn on an undocked Viewport dragged the panel instead of selecting.
        // Docked panels were unaffected, which is exactly what made it look like a viewport bug.
        //
        // Global rather than a per-panel flag: it is the convention every editor already follows,
        // and dragging inside the Hierarchy's body should not move that panel either. Drag-drop is
        // untouched — a payload source is an ITEM, and items outrank a window move regardless.
        lIO.ConfigWindowsMoveFromTitleBarOnly = true;

        //ImGui::StyleColorsDark();
        ImGui::StyleColorsClassic();
        //ImGui::StyleColorsLight();

        // --- Dock layout persistence. Set BEFORE the first NewFrame: that is where ImGui loads the ini
        //     (it only ever loads once, on the frame it first sees a filename). Empty => no
        //     persistence rather than a stray file next to the exe. ------------------------------
        m_LayoutIniPath = Move(InLayoutIniPath);
        lIO.IniFilename = m_LayoutIniPath.IsEmpty() ? nullptr : m_LayoutIniPath.CStr();

        if (!m_LayoutIniPath.IsEmpty())
        {
            OPAAX_LOG(LogEditorGui, Info, "Dock layout: {}", m_LayoutIniPath.CStr());
        }

        // The renderer impl (OpenGL today, S7) — last, because ImGui_ImplOpenGL3_Init needs the
        // context that now exists.
        m_Backend = MakeUnique<OpenGLEditorUIBackend>(lNativeWindow);
        m_Backend->Init();

        return true;
    }

    void EditorGui::Shutdown()
    {
        if (m_Backend == nullptr) { return; }

        m_Backend->Shutdown();

        // DestroyContext FLUSHES the dock layout through the io.IniFilename pointer that still aims
        // at m_LayoutIniPath — so that member is not cleared, here or anywhere.
        ImGui::DestroyContext();
        m_Backend.reset();
    }

    void EditorGui::BeginFrame()
    {
        m_Backend->NewFrame();
        ImGui::NewFrame();

        // ③ — right after ImGui's own NewFrame, as ImGuizmo's header asks. Needed even though the
        // ViewportPanel calls SetDrawlist: this is what clears the per-frame hotspot flags IsOver()
        // reads, and a stale one would leave the marquee suppressed after the cursor left a handle.
        ImGuizmo::BeginFrame();
    }

    void EditorGui::EndFrame()
    {
        ImGui::Render();
        m_Backend->RenderDrawData();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            m_Backend->RenderPlatformWindows();
        }
    }

    void EditorGui::Draw(const EditorContext& InContext)
    {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        
        //InContext.Extensions.Menus().Draw(InContext);
    }

    double EditorGui::GetTime() const
    {
        return ImGui::GetTime();
    }

    bool EditorGui::IsPointerOverUI() const
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool EditorGui::IsKeyboardOwnedByUI() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    bool EditorGui::Shortcut(const EKeyCode InModifier, const EKeyCode InKey) const
    {
        const ImGuiKey lKey = ToImGuiKey(InKey);

        OPAAX_ASSERT(lKey != ImGuiKey_None);
        if (lKey == ImGuiKey_None) { return false; }

        return ImGui::Shortcut(ToModifier(InModifier) | lKey, ImGuiInputFlags_RouteGlobal);
    }
}
