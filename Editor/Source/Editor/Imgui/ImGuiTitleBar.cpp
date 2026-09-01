#include "Editor/Imgui/ImGuiTitleBar.h"

#include <imgui.h>

#include "Core/Window/Window.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Menus/MenuRegistry.h"

namespace
{
    using namespace Opaax;
    using namespace Opaax::Editor;

    /** How far inside the window edge counts as the resize border. */
    constexpr Int32 k_BorderThickness = 6;

    /** The floor a border drag clamps to — small enough to be useful, big enough to stay usable. */
    constexpr Int32 k_MinWindowWidth  = 480;
    constexpr Int32 k_MinWindowHeight = 320;

    enum class EWindowButton : Uint8
    {
        Minimize,
        Maximize,
        Restore,
        Close
    };

    /**
     * The three captions are PAINTED, not typed.
     *
     * ImGui's default font covers Basic Latin + Latin-1 only, so the glyphs these want — U+2014,
     * U+25A1, U+2715 — would all render as boxes. Strokes on the draw list need no font at all.
     */
    void PaintButtonGlyph(ImDrawList* InDrawList, const EWindowButton InButton, const ImVec2 InCenter,
                          const float InSide, const ImU32 InColor)
    {
        const float lHalf = InSide * 0.5f;

        switch (InButton)
        {
        case EWindowButton::Minimize:
            InDrawList->AddLine(ImVec2(InCenter.x - lHalf, InCenter.y),
                                ImVec2(InCenter.x + lHalf, InCenter.y), InColor, 1.f);
            break;

        case EWindowButton::Maximize:
            InDrawList->AddRect(ImVec2(InCenter.x - lHalf, InCenter.y - lHalf),
                                ImVec2(InCenter.x + lHalf, InCenter.y + lHalf), InColor, 0.f, 0, 1.f);
            break;

        case EWindowButton::Restore:
        {
            // Two offset squares — the "already maximized" caption every OS uses.
            const float lShift = InSide * 0.22f;

            InDrawList->AddRect(ImVec2(InCenter.x - lHalf + lShift, InCenter.y - lHalf - lShift),
                                ImVec2(InCenter.x + lHalf + lShift, InCenter.y + lHalf - lShift),
                                InColor, 0.f, 0, 1.f);

            InDrawList->AddRectFilled(ImVec2(InCenter.x - lHalf - lShift, InCenter.y - lHalf + lShift),
                                      ImVec2(InCenter.x + lHalf - lShift, InCenter.y + lHalf + lShift),
                                      ImGui::GetColorU32(ImGuiCol_MenuBarBg));

            InDrawList->AddRect(ImVec2(InCenter.x - lHalf - lShift, InCenter.y - lHalf + lShift),
                                ImVec2(InCenter.x + lHalf - lShift, InCenter.y + lHalf + lShift),
                                InColor, 0.f, 0, 1.f);
            break;
        }

        case EWindowButton::Close:
            InDrawList->AddLine(ImVec2(InCenter.x - lHalf, InCenter.y - lHalf),
                                ImVec2(InCenter.x + lHalf, InCenter.y + lHalf), InColor, 1.2f);
            InDrawList->AddLine(ImVec2(InCenter.x - lHalf, InCenter.y + lHalf),
                                ImVec2(InCenter.x + lHalf, InCenter.y - lHalf), InColor, 1.2f);
            break;
        }
    }

    /**
     * One caption button: an InvisibleButton that owns the interaction, painted underneath it.
     *
     * @param InHoverColor Close gets the red every OS gives it; the other two get the ordinary
     *   header highlight.
     */
    bool WindowButton(const char* InId, const EWindowButton InButton, const ImVec2 InSize,
                      const ImU32 InHoverColor)
    {
        ImDrawList* lDrawList = ImGui::GetWindowDrawList();
        const ImVec2 lMin     = ImGui::GetCursorScreenPos();

        const bool lClicked = ImGui::InvisibleButton(InId, InSize);

        const ImVec2 lMax = ImVec2(lMin.x + InSize.x, lMin.y + InSize.y);

        if (ImGui::IsItemActive() || ImGui::IsItemHovered())
        {
            lDrawList->AddRectFilled(lMin, lMax, InHoverColor);
        }

        PaintButtonGlyph(lDrawList, InButton,
                         ImVec2((lMin.x + lMax.x) * 0.5f, (lMin.y + lMax.y) * 0.5f),
                         ImGui::GetFontSize() * 0.5f,
                         ImGui::GetColorU32(ImGuiCol_Text));

        return lClicked;
    }

    ImGuiMouseCursor CursorForEdge(const EWindowFrameEdge InEdge) noexcept
    {
        switch (InEdge)
        {
        case EWindowFrameEdge::Left:
        case EWindowFrameEdge::Right:       return ImGuiMouseCursor_ResizeEW;

        case EWindowFrameEdge::Top:
        case EWindowFrameEdge::Bottom:      return ImGuiMouseCursor_ResizeNS;

        case EWindowFrameEdge::TopLeft:
        case EWindowFrameEdge::BottomRight: return ImGuiMouseCursor_ResizeNWSE;

        case EWindowFrameEdge::TopRight:
        case EWindowFrameEdge::BottomLeft:  return ImGuiMouseCursor_ResizeNESW;

        default:                            return ImGuiMouseCursor_Arrow;
        }
    }
}

namespace Opaax::Editor
{
    void ImGuiTitleBar::DrawBar(EditorContext& InContext, const MenuRegistry& InMenus)
    {
        InMenus.Draw(InContext);

        Window&                lWindow   = InContext.MainWindow;
        const EditorCommandRegistry& lCommands = InContext.Extensions.Commands();

        // GetFrameHeight, not the window's MenuBarHeight: that member needs imgui_internal.h, and
        // the bar's height IS a frame height — it is what the pushed FramePadding produced.
        const float lBarHeight    = ImGui::GetFrameHeight();
        const float lButtonWidth  = ImGui::GetFontSize() * 2.6f;
        const float lButtonsSpan  = lButtonWidth * 3.f;

        // The drag region takes whatever the menus and the buttons leave. Claiming it LAST of the
        // two would let it cover the buttons; claiming it first would let it cover the menus.
        const float lSlack = ImGui::GetContentRegionAvail().x - lButtonsSpan;

        if (lSlack > 0.f)
        {
            ImGui::InvisibleButton("##TitleBarDrag", ImVec2(lSlack, lBarHeight));

            // A maximized window is not draggable — Windows would restore-and-follow, which is
            // polish this does not have yet (named, and the Win32 frame gets it for free).
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)
                && !lWindow.IsMaximized())
            {
                const ImVec2 lDelta = ImGui::GetIO().MouseDelta;

                Int32 lPosX = 0;
                Int32 lPosY = 0;
                lWindow.GetPosition(lPosX, lPosY);

                lWindow.SetPosition(lPosX + static_cast<Int32>(lDelta.x),
                                    lPosY + static_cast<Int32>(lDelta.y));
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                lCommands.Execute(Tags::EDITOR_COMMAND_TOGGLE_MAXIMIZE_WINDOW, InContext);
            }
        }

        // FLUSH, like every OS caption — and not merely cosmetic: a menu bar lays items out
        // horizontally with ItemSpacing between them, so three spaced buttons would overrun the
        // span reserved for them and push Close off the edge.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - lButtonsSpan);

        const ImU32 lHover = ImGui::GetColorU32(ImGuiCol_HeaderHovered);
        const ImU32 lClose = ImGui::GetColorU32(ImVec4(0.78f, 0.16f, 0.16f, 1.f));

        const bool lMaximized = lWindow.IsMaximized();

        if (WindowButton("##Minimize", EWindowButton::Minimize, ImVec2(lButtonWidth, lBarHeight), lHover))
        {
            lCommands.Execute(Tags::EDITOR_COMMAND_MINIMIZE_WINDOW, InContext);
        }

        if (WindowButton("##Maximize", lMaximized ? EWindowButton::Restore : EWindowButton::Maximize,
                         ImVec2(lButtonWidth, lBarHeight), lHover))
        {
            lCommands.Execute(Tags::EDITOR_COMMAND_TOGGLE_MAXIMIZE_WINDOW, InContext);
        }

        // The SAME verb the File menu's Exit runs — one close path, as Window::RequestClose is.
        if (WindowButton("##Close", EWindowButton::Close, ImVec2(lButtonWidth, lBarHeight), lClose))
        {
            lCommands.Execute(Tags::EDITOR_COMMAND_QUIT, InContext);
        }

        ImGui::PopStyleVar();   // ItemSpacing
    }

    void ImGuiTitleBar::UpdateResizeBorder(EditorContext& InContext)
    {
        Window& lWindow = InContext.MainWindow;

        // Nothing to resize, and nothing should look resizable.
        if (lWindow.IsMaximized() || !ImGui::IsMousePosValid())
        {
            m_ResizeEdge = EWindowFrameEdge::None;
            return;
        }

        // The HIT TEST runs in ImGui's own space — the main viewport's rect against GetMousePos().
        // That is self-consistent whether or not multi-viewport is on, where a rect built from
        // Window::GetPosition would only agree with the mouse in one of the two cases.
        const ImGuiViewport* lViewport = ImGui::GetMainViewport();

        const WindowFrameRect lHitRect{
            static_cast<Int32>(lViewport->Pos.x), static_cast<Int32>(lViewport->Pos.y),
            static_cast<Int32>(lViewport->Size.x), static_cast<Int32>(lViewport->Size.y)
        };

        const ImVec2 lMouse = ImGui::GetMousePos();

        const EWindowFrameEdge lHovered = HitTestFrame(lHitRect, static_cast<Int32>(lMouse.x),
                                                       static_cast<Int32>(lMouse.y), k_BorderThickness);

        if (m_ResizeEdge != EWindowFrameEdge::None)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                m_ResizeEdge = EWindowFrameEdge::None;
            }
            else
            {
                // APPLIED against the real window rect, in screen pixels — the hit test's space is
                // ImGui's, this one is the OS's, and only the edge travels between them.
                Int32 lPosX = 0;
                Int32 lPosY = 0;
                lWindow.GetPosition(lPosX, lPosY);

                const WindowFrameRect lRect{
                    lPosX, lPosY,
                    static_cast<Int32>(lWindow.GetWidth()), static_cast<Int32>(lWindow.GetHeight())
                };

                const ImVec2 lDelta = ImGui::GetIO().MouseDelta;

                const WindowFrameRect lNew = ResizeFrame(lRect, m_ResizeEdge,
                                                         static_cast<Int32>(lDelta.x),
                                                         static_cast<Int32>(lDelta.y),
                                                         k_MinWindowWidth, k_MinWindowHeight);

                if (lNew.X != lRect.X || lNew.Y != lRect.Y)
                {
                    lWindow.SetPosition(lNew.X, lNew.Y);
                }

                if (lNew.Width != lRect.Width || lNew.Height != lRect.Height)
                {
                    lWindow.SetSize(static_cast<Uint32>(lNew.Width), static_cast<Uint32>(lNew.Height));
                }
            }
        }
        else if (lHovered != EWindowFrameEdge::None && !ImGui::IsAnyItemActive()
                 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            // Gated on IsAnyItemActive so a panel's own splitter, which lives in the same pixels,
            // keeps winning.
            m_ResizeEdge = lHovered;
        }

        const EWindowFrameEdge lShown = m_ResizeEdge != EWindowFrameEdge::None
                                            ? m_ResizeEdge
                                            : (ImGui::IsAnyItemActive() ? EWindowFrameEdge::None : lHovered);

        if (lShown != EWindowFrameEdge::None)
        {
            ImGui::SetMouseCursor(CursorForEdge(lShown));
        }
    }
}
