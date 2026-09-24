#include "Engine/UI/UIInputRouter.h"

#include "Engine/Subsystems/Input/InputCodes.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "UI/UICanvas.h"
#include "UI/UIEvents.h"

namespace Opaax::UIInputRouter
{
    namespace
    {
        /** The UI pointer button for a raw code, or None for anything that is not a mouse button. */
        EUIPointerButton ToPointerButton(const EKeyCode InKey) noexcept
        {
            switch (InKey)
            {
                case EKeyCode::Mouse_Left:   return EUIPointerButton::Primary;
                case EKeyCode::Mouse_Right:  return EUIPointerButton::Secondary;
                case EKeyCode::Mouse_Middle: return EUIPointerButton::Middle;
                default:                     return EUIPointerButton::None;
            }
        }

        bool IsMouseCode(const EKeyCode InKey) noexcept
        {
            return InKey >= EKeyCode::Mouse_Left && InKey <= EKeyCode::Mouse_Button8;
        }

        void Consume(InputKeyMask& OutConsumed, const EKeyCode InKey) noexcept
        {
            const Uint16 lIndex = static_cast<Uint16>(InKey);
            if (lIndex < InputManager::KEY_STATE_COUNT) { OutConsumed[lIndex] = true; }
        }
    }

    void Route(const InputManager& InInput, UICanvas& InCanvas, const EUIInputMode InMode, InputKeyMask& OutConsumed)
    {
        OutConsumed = InputKeyMask{};

        if (InMode == EUIInputMode::GameOnly)
        {
            InCanvas.ClearPointer();
            return;
        }

        const Vector2F lPoint = InCanvas.ScreenToCanvas(InInput.GetMousePosition());

        // Move first, so hover is current before a press hit-tests.
        InCanvas.RoutePointer({ EUIPointerEventType::Move, lPoint, EUIPointerButton::None });

        // The three mouse buttons the UI understands.
        for (const EKeyCode lButton : { EKeyCode::Mouse_Left, EKeyCode::Mouse_Right, EKeyCode::Mouse_Middle })
        {
            const EUIPointerButton lUIButton = ToPointerButton(lButton);

            if (InInput.WasPressedThisFrame(lButton))
            {
                if (InCanvas.RoutePointer({ EUIPointerEventType::Down, lPoint, lUIButton }) == EUIReply::Handled)
                {
                    Consume(OutConsumed, lButton);
                }
            }
            else if (InInput.WasReleasedThisFrame(lButton))
            {
                if (InCanvas.RoutePointer({ EUIPointerEventType::Up, lPoint, lUIButton }) == EUIReply::Handled)
                {
                    Consume(OutConsumed, lButton);
                }
            }
            else if (InInput.IsKeyDown(lButton) && InCanvas.GetPressed() != nullptr)
            {
                // A button held after a captured press: keep swallowing it so a drag never leaks
                // mid-gesture into the world.
                Consume(OutConsumed, lButton);
            }
        }

        // Keyboard edges to the focused widget. The mouse range is handled above; skip it.
        for (Uint16 lIndex = 1; lIndex < InputManager::KEY_STATE_COUNT; ++lIndex)
        {
            const EKeyCode lKey = static_cast<EKeyCode>(lIndex);
            if (IsMouseCode(lKey)) { continue; }

            const bool lPressed  = InInput.WasPressedThisFrame(lKey);
            const bool lReleased = InInput.WasReleasedThisFrame(lKey);
            if (!lPressed && !lReleased) { continue; }

            if (InCanvas.RouteKey({ lKey, lPressed }) == EUIReply::Handled)
            {
                Consume(OutConsumed, lKey);
            }
        }

        if (InMode == EUIInputMode::UIOnly)
        {
            // The game hears nothing this frame — every key is spoken for.
            for (Uint16 lIndex = 0; lIndex < InputManager::KEY_STATE_COUNT; ++lIndex)
            {
                OutConsumed[lIndex] = true;
            }
        }
    }
}
