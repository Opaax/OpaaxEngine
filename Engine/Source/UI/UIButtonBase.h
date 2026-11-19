#pragma once

#include "Core/Events/Delegate.h"
#include "Core/OpaaxTypes.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    DECLARE_MULTICAST_DELEGATE(FButtonEventDelegate);

    // =============================================================================
    // UIButtonBase — a clickable widget: takes the pointer's presses, fires the events and the Activate
    //   stage. A subclass draws in Rebuild what GetInteractionState() says (UIButton: a brush per state).
    // =============================================================================
    class UIButtonBase : public UIWidget
    {
        OPAAX_OBJECT(UIButtonBase, UIWidget)
        
        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        void SetEnabled(bool bInEnabled);
        bool IsEnabled() const noexcept { return bIsEnabled;  }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin UIWidget Interface
    public:
        EUIReply OnPointerEvent(const UIPointerEvent& InEvent) override;

        /** False while disabled: its transitions show Disabled. */
        bool IsInteractable() const override { return bIsEnabled; }

        /** The widgets in a button can follow its state (StateSource Button). */
        bool IsStateOwner() const override { return true; }

        /** False once the pointer left during the press: the release no longer clicks. */
        bool HoldsPress() const override { return bIsPressed; }
        //~End UIWidget Interface
        
        // =============================================================================
        // Members
        // =============================================================================
    public:
        /** Fired on a click: Down then Up inside, without leaving in between (enabled), after the Activate stage. */
        FButtonEventDelegate OnClick;
        /** Fired on the pointer start enter (enabled). */
        FButtonEventDelegate OnHoveredStart;
        /** Fired on the pointer exit (enabled). */
        FButtonEventDelegate OnHoveredStop;
        /** Fired on the pointer with press event start (Down inside, enabled). */
        FButtonEventDelegate OnPressedStart;
        /** Fired when a press ends: released, or cancelled by the pointer leaving (then no click follows). */
        FButtonEventDelegate OnPressedStop;
        /** Fired by SetEnabled(false) on an enabled button, after a held press has stopped. */
        FButtonEventDelegate OnDisabled;
        /** Fired by SetEnabled(true) on a disabled button. */
        FButtonEventDelegate OnEnabled;

    private:

        bool bIsEnabled   = true;

        /** A press began inside and has not ended: its release inside clicks. */
        bool bIsPressed   = false;

        // =============================================================================
        // Props
        // =============================================================================
    public:
        OPAAX_PROPERTIES(UIButtonBase,
            OPAAX_PROP(bIsEnabled)
            )
    };
}