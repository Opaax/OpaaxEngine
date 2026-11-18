#pragma once

#include "Core/Events/Delegate.h"
#include "Core/OpaaxTypes.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    DECLARE_MULTICAST_DELEGATE(FButtonEventDelegate);

    /** What a button shows: disabled wins, then pressed, then hovered. */
    enum class EUIButtonState : Uint8
    {
        Normal,
        Hovered,
        Pressed,
        Disabled
    };

    inline const char* ToString(const EUIButtonState InState) noexcept
    {
        switch (InState)
        {
            case EUIButtonState::Normal:   return "Normal";
            case EUIButtonState::Hovered:  return "Hovered";
            case EUIButtonState::Pressed:  return "Pressed";
            case EUIButtonState::Disabled: return "Disabled";
        }
        return "Normal";
    }

    // =============================================================================
    // UIButtonBase — a clickable widget: keeps its state from the pointer, fires the events, and
    //   rebuilds with the function of its state. A subclass draws each state.
    // =============================================================================
    class UIButtonBase : public UIWidget
    {
        OPAAX_OBJECT(UIButtonBase, UIWidget)
        
        // =============================================================================
        // Functions
        // =============================================================================
        
    protected:
        virtual void RebuildHoveredState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)        {}
        virtual void RebuildDisabledState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)     {}
        virtual void RebuildPressedState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)      {}
        virtual void RebuildNormalState(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads)       {}
        
        // =============================================================================
        // Get - Set
    public:
        void SetEnabled(bool bInEnabled);
        bool IsEnabled() const noexcept { return bIsEnabled;  }

        /** The state shown now: Disabled, else Pressed, else Hovered, else Normal. */
        EUIButtonState GetState() const noexcept;

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

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;
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

        bool bIsHovered   = false;

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