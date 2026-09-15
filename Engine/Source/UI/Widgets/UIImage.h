#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    /** How FillAmount crops the image. None ignores it. */
    enum class EUIFill : Uint8
    {
        None,
        Horizontal,   // from the left edge
        Vertical      // from the bottom edge
    };

    inline const char* ToString(const EUIFill InFill) noexcept
    {
        switch (InFill)
        {
            case EUIFill::None:       return "None";
            case EUIFill::Horizontal: return "Horizontal";
            case EUIFill::Vertical:   return "Vertical";
        }
        return "None";
    }

    // =============================================================================
    // UIImage — a rect of colour, or a texture tinted by it. ONE quad. A fill crops both the
    //   rect and the sampled UVs from the min edge, which is the health bar.
    //   Sliced (9-slice) and sheet sub-rects are U5.
    // =============================================================================
    class OPAAX_API UIImage final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        LinearColor Color;
        EUIFill     Fill       = EUIFill::None;
        float       FillAmount = 1.f;

        /** A texture's asset path. Empty draws the colour alone (**UI17**). */
        OpaaxString Texture;

        OPAAX_PROPERTIES(UIImage,
                         OPAAX_PROP(Color),
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(Fill),
                         OPAAX_PROP(FillAmount).SetRange(0.f, 1.f))

        void SetTexturePath(const OpaaxString& InAssetPath);

        void SetColor(const LinearColor& InColor);
        void SetFill(EUIFill InFill, float InAmount);
        void SetFillAmount(float InAmount);

        /**
         * A RUNTIME texture, borrowed. It WINS over the authored path when set — `TextComponent`'s
         * Font-over-Face precedence (**TX1**), so code that hands over an atlas is never overruled
         * by what the file happens to name.
         */
        void        SetTexture(ITexture2D* InTexture);
        ITexture2D* GetTexture() const noexcept { return m_Texture; }

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UIImage"); }

        void SaveFields(nlohmann::json& InOutJson) const override;
        void LoadFields(const nlohmann::json& InJson) override;

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Texture = nullptr;
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIFill, None, Horizontal, Vertical);
