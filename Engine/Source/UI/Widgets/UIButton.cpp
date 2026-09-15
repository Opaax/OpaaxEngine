#include "UI/Widgets/UIButton.h"

#include "UI/UIImageSource.h"

namespace Opaax
{
    void UIButton::SetEnabled(const bool bInEnabled)
    {
        bEnabled = bInEnabled;
        InvalidateContent();
    }

    void UIButton::SetTexture(ITexture2D* InTexture)
    {
        m_Texture = InTexture;
        InvalidateContent();
    }

    EUIReply UIButton::OnPointerEvent(const UIPointerEvent& InEvent)
    {
        // A disabled button is inert: it does not take the click, so it falls through like an image.
        if (!bEnabled)
        {
            return EUIReply::Unhandled;
        }

        switch (InEvent.Type)
        {
            case EUIPointerEventType::Enter:
                m_bHovered = true;
                InvalidateContent();
                return EUIReply::Unhandled;   // hover never consumes

            case EUIPointerEventType::Leave:
                m_bHovered = false;
                InvalidateContent();
                return EUIReply::Unhandled;

            case EUIPointerEventType::Down:
                if (InEvent.Button != EUIPointerButton::Primary) { return EUIReply::Unhandled; }
                m_bPressed = true;
                InvalidateContent();
                return EUIReply::Handled;

            case EUIPointerEventType::Up:
            {
                if (InEvent.Button != EUIPointerButton::Primary) { return EUIReply::Unhandled; }

                const bool lWasPressed = m_bPressed;
                m_bPressed = false;
                InvalidateContent();

                // A click needs the release to land inside; a drag-off cancels but still consumes,
                // because this widget captured the press.
                if (lWasPressed && GetBounds().Contains(InEvent.Position))
                {
                    OnClick.Broadcast();
                }
                return EUIReply::Handled;
            }

            default:
                return EUIReply::Unhandled;
        }
    }

    void UIButton::SaveFields(nlohmann::json& InOutJson) const
    {
        UIWidget::SaveFields(InOutJson);

        // OnClick is NOT written: a handler is code. Gameplay binds it by NAME after loading (UI13).
        InOutJson["Normal"]   = Normal;
        InOutJson["Hovered"]  = Hovered;
        InOutJson["Pressed"]  = Pressed;
        InOutJson["Disabled"] = Disabled;
        InOutJson["bEnabled"] = bEnabled;
        InOutJson["Texture"]  = Texture;
        InOutJson["Sheet"]    = Sheet;
        InOutJson["Frame"]    = Frame;
    }

    void UIButton::LoadFields(const nlohmann::json& InJson)
    {
        UIWidget::LoadFields(InJson);

        Normal   = InJson.value("Normal", Normal);
        Hovered  = InJson.value("Hovered", Hovered);
        Pressed  = InJson.value("Pressed", Pressed);
        Disabled = InJson.value("Disabled", Disabled);
        bEnabled = InJson.value("bEnabled", bEnabled);
        Texture  = InJson.value("Texture", Texture);
        Sheet    = InJson.value("Sheet", Sheet);
        Frame    = InJson.value("Frame", Frame);
    }

    void UIButton::Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)
    {
        // The image's sources, the image's rule (UI25): named but not ready draws nothing and
        // asks again; nothing named is the state colour on a plain quad.
        UIResolvedImage lImage;
        bool            bNamed = false;
        if (!ResolveImageSource(InContext, m_Texture, Texture, Sheet, Frame, lImage, bNamed) && bNamed)
        {
            InvalidateContent();
            return;
        }

        UIQuad& lQuad = OutQuads.emplace_back();
        lQuad.Bounds  = GetBounds();
        lQuad.Texture = lImage.Texture;
        lQuad.UVMin   = lImage.UVMin;
        lQuad.UVMax   = lImage.UVMax;
        lQuad.Color   = !bEnabled ? Disabled
                      : m_bPressed ? Pressed
                      : m_bHovered ? Hovered
                                   : Normal;
    }
}
