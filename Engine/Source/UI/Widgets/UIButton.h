#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Events/Delegate.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    // Forward-declared: TResourcePath only needs the name.
    struct TextureResource;
    struct SpriteSheetResource;

    DECLARE_MULTICAST_DELEGATE(FOnUIClick)

    // =============================================================================
    // UIButton — a clickable quad. A click is Down then Up still inside (a drag-off does not fire).
    //   Four state colours tint its art (texture or sheet frame). Add a UIText child for a label.
    //   It handles its pointer events, so clicks do not reach the game.
    // =============================================================================
    class UIButton final : public UIWidget
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

        /** The button's art, tinted by the state colour. The sheet wins over the texture. */
        TResourcePath<TextureResource>     Texture;
        TResourcePath<SpriteSheetResource> Sheet;
        Int32                              Frame = -1;

        OPAAX_PROPERTIES(UIButton,
                         OPAAX_PROP(Normal),
                         OPAAX_PROP(Hovered),
                         OPAAX_PROP(Pressed),
                         OPAAX_PROP(Disabled),
                         OPAAX_PROP(bEnabled),
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(Sheet).SetTooltip("An icon cut from a sheet. Wins over Texture when set."),
                         OPAAX_PROP(Frame).SetRange(-1.f, 4096.f).SetDragStep(1.f).SetTooltip("-1 is the sheet's own default frame."))

        /** Fired on a click (Down + Up inside, enabled). */
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

        void SaveFields(nlohmann::json& InOutJson) const override;
        void LoadFields(const nlohmann::json& InJson) override;

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
