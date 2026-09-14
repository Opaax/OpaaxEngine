#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Events/Delegate.h"
#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    DECLARE_MULTICAST_DELEGATE(FOnUIClick)

    // =============================================================================
    // UIButton — one clickable quad. A click is Down then Up while STILL INSIDE (the pointer is
    //   captured between them, so a drag-off-and-release does not fire). Unity's ColorBlock for the
    //   four states; a label is a UIText child the author adds.
    //
    //   It HANDLES its pointer events (returns Handled), which is what stops the click bubbling on
    //   to the game — a bare UIImage does not, so a HUD image lets the click through (UI9).
    // =============================================================================
    class OPAAX_API UIButton final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        LinearColor Normal   = { 0.25f, 0.25f, 0.28f, 1.f };
        LinearColor Hovered  = { 0.35f, 0.35f, 0.40f, 1.f };
        LinearColor Pressed  = { 0.15f, 0.15f, 0.18f, 1.f };
        LinearColor Disabled = { 0.20f, 0.20f, 0.20f, 0.5f };
        bool        bEnabled = true;

        OPAAX_PROPERTIES(UIButton,
                         OPAAX_PROP(Normal),
                         OPAAX_PROP(Hovered),
                         OPAAX_PROP(Pressed),
                         OPAAX_PROP(Disabled),
                         OPAAX_PROP(bEnabled))

        /** Fired on a completed click (Down + Up inside, enabled). */
        FOnUIClick OnClick;

        void        SetEnabled(bool bInEnabled);
        void        SetTexture(ITexture2D* InTexture);
        ITexture2D* GetTexture() const noexcept { return m_Texture; }

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UIButton"); }

        EUIReply OnPointerEvent(const UIPointerEvent& InEvent) override;

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Texture   = nullptr;   // borrowed
        bool        m_bHovered  = false;
        bool        m_bPressed  = false;
    };
}
