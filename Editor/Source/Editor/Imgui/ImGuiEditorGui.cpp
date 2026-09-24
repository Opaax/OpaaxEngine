#include "Editor/Imgui/ImGuiEditorGui.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <cstdio>   // snprintf — the host window's label

#include "ImguiHelper.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/ILogger.h"
#include "Configs/Config_EditorImgui.h"
#include "Core/EngineAPI.h"   // OPAAX_ASSERT
#include "Window/Window.h"
#include "Editor/EditorContext.h"
#include "Editor/Application/EditorApplication.h"
#include "Editor/Panels/EditorPanels.h"
#include "Editor/Panels/IEditorPanel.h"   // PanelWindowStyle — the window chrome's one parameter
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

    /** Vertical frame padding for the caption row — what makes the menu bar title-bar height. */
    constexpr float k_TitleBarPaddingY = 8.f;

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
    void ImGuiEditorGui::CheckStyle()
    {
        IConfigSystem& lConfigSys = OpaaxApplication::GetAppService<IConfigSystem>();
        
        if (lConfigSys.IsNull())
        {
            ImGui::StyleColorsClassic();
            return;
        }
        
        const EditorImguiConfigData& lImguiCG = lConfigSys.Get<Config_EditorImgui>().GetData();
        
        UpdateStyle(lImguiCG);
    }

    void ImGuiEditorGui::UpdateStyle(const EditorImguiConfigData& InCFG)
    {
        ImGuiStyle* lStyle = &ImGui::GetStyle();
        ImVec4* lColors = lStyle->Colors;
        
        // --- 1. Framing & Spacing ---
        lStyle->WindowPadding = ImguiHelper::Vector2FToImVec2(InCFG.WindowPadding);
        lStyle->FramePadding = ImVec2(6.0f, 4.0f);
        lStyle->ItemSpacing = ImVec2(8.0f, 6.0f);
        lStyle->ScrollbarSize = 14.0f;
        lStyle->GrabMinSize = 12.0f;

        // --- 2. Borders & Rounding ---
        lStyle->WindowRounding = 6.0f;
        lStyle->FrameRounding = 4.0f;
        lStyle->PopupRounding = 4.0f;
        lStyle->ScrollbarRounding = 12.0f;
        lStyle->GrabRounding = 4.0f;
        lStyle->TabRounding = 4.0f;

        lStyle->WindowBorderSize = 1.0f;
        lStyle->FrameBorderSize = 1.0f;
        
        lColors[ImGuiCol_Text]                      = ImguiHelper::LinearColorToImColor(InCFG.TextColor);
        lColors[ImGuiCol_TextDisabled]              = ImguiHelper::LinearColorToImColor(InCFG.TextDisabledColor);
        lColors[ImGuiCol_WindowBg]                  = ImguiHelper::LinearColorToImColor(InCFG.WindowBackground);
    }

    bool ImGuiEditorGui::Init(Window& InWindow, OpaaxString InLayoutIniPath)
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
        lIO.ConfigFlags |= ImGuiConfigFlags_DockingEnable; //Docking
        lIO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; //Panels can be drag outside the main viewport
        
        lIO.ConfigWindowsMoveFromTitleBarOnly = true;
        
        CheckStyle();

        //ImGui::StyleColorsDark();
        //ImGui::StyleColorsClassic();
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

    void ImGuiEditorGui::SetUIFont(const EditorUIFont& InFont)
    {
        if (InFont.Path.IsEmpty())
        {
            return;   // keep ProggyClean — an unset config is a choice, not an error
        }

        ImGuiIO& lIO = ImGui::GetIO();

        // NoLoadError turns ImGui's assert-on-missing-file into a null return, which is what makes a
        // mistyped config path survivable: the editor keeps its default font instead of dying on an
        // IM_ASSERT before it has drawn a single frame.
        ImFontConfig lPrimaryCfg;
        lPrimaryCfg.Flags |= ImFontFlags_NoLoadError;

        ImFont* lPrimary = lIO.Fonts->AddFontFromFileTTF(InFont.Path.CStr(), InFont.SizePx, &lPrimaryCfg);
        if (lPrimary == nullptr)
        {
            OPAAX_LOG(LogEditorGui, Warn, "UI font '{}' could not be read — keeping the default",
                      InFont.Path.CStr());
            return;
        }

        // MERGED into the primary, not stacked beside it: the fallbacks are SUBSET files, so the
        // result has to be one typeface that happens to cover Greek and Cyrillic — not three fonts a
        // caller would have to choose between per string. ImGui 1.92 loads glyphs on demand, so no
        // range table is needed and merging costs nothing until a character is actually drawn.
        Uint32 lMerged = 0u;
        for (const OpaaxString& lFallback : InFont.Fallbacks)
        {
            ImFontConfig lMergeCfg;
            lMergeCfg.MergeMode = true;
            lMergeCfg.Flags    |= ImFontFlags_NoLoadError;

            if (lIO.Fonts->AddFontFromFileTTF(lFallback.CStr(), InFont.SizePx, &lMergeCfg) != nullptr)
            {
                ++lMerged;
            }
            else
            {
                OPAAX_LOG(LogEditorGui, Warn, "UI font fallback '{}' could not be read — skipped",
                          lFallback.CStr());
            }
        }

        lIO.FontDefault = lPrimary;

        OPAAX_LOG(LogEditorGui, Info, "UI font: '{}' at {}px, {} of {} fallback(s) merged",
                  InFont.Path.CStr(), InFont.SizePx, lMerged, InFont.Fallbacks.size());
    }

    void ImGuiEditorGui::Shutdown()
    {
        if (m_Backend == nullptr) { return; }

        m_Backend->Shutdown();

        // DestroyContext FLUSHES the dock layout through the io.IniFilename pointer that still aims
        // at m_LayoutIniPath — so that member is not cleared, here or anywhere.
        ImGui::DestroyContext();
        m_Backend.reset();
    }

    void ImGuiEditorGui::BeginFrame()
    {
        m_Backend->NewFrame();
        CheckStyle();
        ImGui::NewFrame();

        // ③ — right after ImGui's own NewFrame, as ImGuizmo's header asks. Needed even though the
        // ViewportPanel calls SetDrawlist: this is what clears the per-frame hotspot flags IsOver()
        // reads, and a stale one would leave the marquee suppressed after the cursor left a handle.
        ImGuizmo::BeginFrame();
    }

    void ImGuiEditorGui::EndFrame()
    {
        ImGui::Render();
        m_Backend->RenderDrawData();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            m_Backend->RenderPlatformWindows();
        }
    }

    void ImGuiEditorGui::Draw(EditorContext& InContext)
    {
        const ImGuiViewport* lViewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(lViewport->Pos);
        ImGui::SetNextWindowSize(lViewport->Size);
        ImGui::SetNextWindowViewport(lViewport->ID);

        constexpr ImGuiWindowFlags k_HostFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        // snprintf, not ImGui's ImFormatString — that one lives in imgui_internal.h. The FORMAT is
        // copied verbatim from DockSpaceOverViewport; it is what the saved layout is keyed on.
        char lLabel[32];
        std::snprintf(lLabel, sizeof(lLabel), "WindowOverViewport_%08X", lViewport->ID);

        // Pos/Size rather than WorkPos/WorkSize: the bar is INSIDE this window now, so there is no
        // reserved strip above it to avoid.
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));

        // Taller than a menu strip — this is a caption. Pushed before Begin because the menu bar's
        // height is decided there, and kept through the bar so its items match it.
        const ImVec2 lFramePadding = ImGui::GetStyle().FramePadding;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(lFramePadding.x, k_TitleBarPaddingY));

        ImGui::Begin(lLabel, nullptr, k_HostFlags);
        ImGui::PopStyleVar(3);   // the three window vars; FramePadding stays for the bar

        if (ImGui::BeginMenuBar())
        {
            m_TitleBar.Draw(InContext, *this);
            ImGui::EndMenuBar();
        }

        ImGui::PopStyleVar();   // FramePadding

        ImGui::DockSpace(ImGui::GetID("DockSpace"));
        ImGui::End();

        m_Panels.Draw(*this);

        // LAST: it reads IsAnyItemActive, which only means anything once the panels have submitted.
        m_ImGuiTitleBar.UpdateResizeBorder(InContext);
    }

    bool ImGuiEditorGui::BeginMenu(const char* InLabel, const bool bInEnabled)
    {
        return ImGui::BeginMenu(InLabel, bInEnabled);
    }

    void ImGuiEditorGui::EndMenu()
    {
        ImGui::EndMenu();
    }

    bool ImGuiEditorGui::MenuItem(const char* InLabel, const bool bInChecked, const bool bInEnabled)
    {
        return ImGui::MenuItem(InLabel, nullptr, bInChecked, bInEnabled);
    }

    void ImGuiEditorGui::MenuSeparator()
    {
        ImGui::Separator();
    }

    TitleBarDrag ImGuiEditorGui::TitleBarDragRegion(const Uint32 InTrailingButtons)
    {
        return m_ImGuiTitleBar.DragRegion(InTrailingButtons);
    }

    bool ImGuiEditorGui::TitleBarButton(const EWindowButtonKind InKind)
    {
        return m_ImGuiTitleBar.Button(InKind);
    }

    bool ImGuiEditorGui::BeginPanelWindow(const char* InLabel, const PanelWindowStyle& InStyle,
                                          bool& bOutWantOpen)
    {
        ImGui::SetNextWindowSize(ImVec2(InStyle.DefaultSize.x, InStyle.DefaultSize.y), ImGuiCond_FirstUseEver);

        if (InStyle.bNoPadding) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f)); }

        const bool lOpen = ImGui::Begin(InLabel, &bOutWantOpen);

        // Popped right after Begin, not at End: the var applies to the window's own padding, which
        // Begin has already consumed. Swallowing the pair here is why EndPanelWindow takes nothing.
        if (InStyle.bNoPadding) { ImGui::PopStyleVar(); }

        return lOpen;
    }

    bool ImGuiEditorGui::IsPanelWindowFocused() const
    {
        // RootAndChildWindows, so typing in the frame list's scroll child still counts as being in
        // the panel — a focus test that a child window can turn off is one nobody can rely on.
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    }

    void ImGuiEditorGui::EndPanelWindow()
    {
        ImGui::End();
    }

    double ImGuiEditorGui::GetTime() const
    {
        return ImGui::GetTime();
    }

    bool ImGuiEditorGui::IsPointerOverUI() const
    {
        return ImGui::GetIO().WantCaptureMouse;
    }

    bool ImGuiEditorGui::IsKeyboardOwnedByUI() const
    {
        return ImGui::GetIO().WantCaptureKeyboard;
    }

    bool ImGuiEditorGui::Shortcut(const EKeyCode InModifier, const EKeyCode InKey) const
    {
        const ImGuiKey lKey = ToImGuiKey(InKey);

        OPAAX_ASSERT(lKey != ImGuiKey_None);
        if (lKey == ImGuiKey_None) { return false; }

        return ImGui::Shortcut(ToModifier(InModifier) | lKey, ImGuiInputFlags_RouteGlobal);
    }
}
