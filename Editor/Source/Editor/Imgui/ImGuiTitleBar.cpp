#include "Editor/Imgui/ImGuiTitleBar.h"

#include <imgui.h>

#include "Platform/Window/Window.h"
#include "Editor/EditorContext.h"

namespace
{
    using namespace Opaax;
    using namespace Opaax::Editor;

    /** How far inside the window edge counts as the resize border. */
    constexpr Int32 k_BorderThickness = 6;

    /** The minimum size a border drag clamps to. */
    constexpr Int32 k_MinWindowWidth  = 480;
    constexpr Int32 k_MinWindowHeight = 320;

    /** One caption button's width (used by the drag region's reservation and the button). */
    float ButtonWidth() { return ImGui::GetFontSize() * 2.6f; }

    /**
     * The caption glyphs are drawn with lines: ImGui's default font lacks the characters.
     */
    void PaintButtonGlyph(ImDrawList* InDrawList, const EWindowButtonKind InKind, const ImVec2 InCenter,
                          const float InSide, const ImU32 InColor)
    {
        const float lHalf = InSide * 0.5f;

        switch (InKind)
        {
        case EWindowButtonKind::Minimize:
            InDrawList->AddLine(ImVec2(InCenter.x - lHalf, InCenter.y),
                                ImVec2(InCenter.x + lHalf, InCenter.y), InColor, 1.f);
            break;

        case EWindowButtonKind::Maximize:
            InDrawList->AddRect(ImVec2(InCenter.x - lHalf, InCenter.y - lHalf),
                                ImVec2(InCenter.x + lHalf, InCenter.y + lHalf), InColor, 0.f, 0, 1.f);
            break;

        case EWindowButtonKind::Restore:
        {
            // Two offset squares: the usual "restore" icon.
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

        case EWindowButtonKind::Close:
            InDrawList->AddLine(ImVec2(InCenter.x - lHalf, InCenter.y - lHalf),
                                ImVec2(InCenter.x + lHalf, InCenter.y + lHalf), InColor, 1.2f);
            InDrawList->AddLine(ImVec2(InCenter.x - lHalf, InCenter.y + lHalf),
                                ImVec2(InCenter.x + lHalf, InCenter.y - lHalf), InColor, 1.2f);
            break;
        }
    }

    const char* ButtonId(const EWindowButtonKind InKind) noexcept
    {
        switch (InKind)
        {
        case EWindowButtonKind::Minimize: return "##TitleBarMinimize";
        case EWindowButtonKind::Maximize:
        case EWindowButtonKind::Restore:  return "##TitleBarMaximize";   // one button, two glyphs
        case EWindowButtonKind::Close:    return "##TitleBarClose";
        }

        return "##TitleBarButton";
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
    TitleBarDrag ImGuiTitleBar::DragRegion(const Uint32 InTrailingButtons)
    {
        TitleBarDrag lResult;

        const float lBarHeight = ImGui::GetFrameHeight();
        const float lReserved  = ButtonWidth() * static_cast<float>(InTrailingButtons);
        const float lSlack     = ImGui::GetContentRegionAvail().x - lReserved;

        if (lSlack <= 0.f) { return lResult; }

        ImGui::InvisibleButton("##TitleBarDrag", ImVec2(lSlack, lBarHeight));

        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            const ImVec2 lDelta = ImGui::GetIO().MouseDelta;
            lResult.Delta = Vector2F(lDelta.x, lDelta.y);
        }

        lResult.bDoubleClicked = ImGui::IsItemHovered()
                              && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

        // Right-align what follows.
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - lReserved);

        return lResult;
    }

    bool ImGuiTitleBar::Button(const EWindowButtonKind InKind)
    {
        const float  lBarHeight = ImGui::GetFrameHeight();
        const float  lWidth     = ButtonWidth();
        ImDrawList*  lDrawList  = ImGui::GetWindowDrawList();

        // No spacing between the buttons (like every OS), otherwise they overflow the reserved width.
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));

        const ImVec2 lMin = ImGui::GetCursorScreenPos();

        const bool lClicked = ImGui::InvisibleButton(ButtonId(InKind), ImVec2(lWidth, lBarHeight));

        const ImVec2 lMax = ImVec2(lMin.x + lWidth, lMin.y + lBarHeight);

        if (ImGui::IsItemActive() || ImGui::IsItemHovered())
        {
            // Close gets the usual red; the other two the normal highlight.
            const ImU32 lHighlight = InKind == EWindowButtonKind::Close
                                         ? ImGui::GetColorU32(ImVec4(0.78f, 0.16f, 0.16f, 1.f))
                                         : ImGui::GetColorU32(ImGuiCol_HeaderHovered);

            lDrawList->AddRectFilled(lMin, lMax, lHighlight);
        }

        PaintButtonGlyph(lDrawList, InKind,
                         ImVec2((lMin.x + lMax.x) * 0.5f, (lMin.y + lMax.y) * 0.5f),
                         ImGui::GetFontSize() * 0.5f,
                         ImGui::GetColorU32(ImGuiCol_Text));

        ImGui::PopStyleVar();

        return lClicked;
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

        // Hit test in ImGui's space (main viewport rect vs GetMousePos()), consistent with or without
        // multi-viewport.
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
                // Applied to the real window rect in screen pixels (only the edge crosses between the two spaces).
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
            // Skipped while an item is active, so a panel's splitter in the same pixels wins.
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
