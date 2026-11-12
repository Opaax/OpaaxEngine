#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Events/Delegate.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Renderer/Textures/SpriteSheet.h"   // complete: a widget class visits its sheet reference
#include "Renderer/Textures/Texture2D.h"     // and its texture reference
#include "Resources/Asset/AssetRef.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

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

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;
        //~End UIWidget Interface
        
        // =============================================================================
        // Members
        // =============================================================================
    public:
        /** Fired on a click: Down then Up inside, without leaving in between (enabled). */
        FButtonEventDelegate OnClick;
        /** Fired on the pointer start enter (enabled). */
        FButtonEventDelegate OnHoveredStart;
        /** Fired on the pointer exit (enabled). */
        FButtonEventDelegate OnHoveredStop;
        /** Fired on the pointer with press event start (Down inside, enabled). */
        FButtonEventDelegate OnPressedStart;
        /** Fired when a press ends: released, or cancelled by the pointer leaving (then no click follows). */
        FButtonEventDelegate OnPressedStop;
        /** Fired when the button went from enable true to enable false */
        FButtonEventDelegate OnDisabled;
        /** Fired when the button went from enable false to enable true */
        FButtonEventDelegate OnEnabled;
        
    private:
        
        bool bIsEnabled   = true; 
        bool bWasEnabled  = true; //True at first to call event first frame if needed.
        
        bool bIsHovered   = false;
        
        bool bIsPressed   = false;
        bool bWasPressed  = false;
        
        // =============================================================================
        // Props
        // =============================================================================
    public:
        OPAAX_PROPERTIES(UIButtonBase,
            OPAAX_PROP(bIsEnabled)
            )
    };
}