#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"
#include "UI/UIMargin.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    // Forward-declared: TResourcePath only needs the name.
    struct TextureResource;
    struct SpriteSheetResource;

    /** How FillAmount crops the image. None ignores it. */
    enum class EUIFill : Uint8
    {
        None,
        Horizontal,   // from the left
        Vertical      // from the bottom
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
    // UIImage — a colour rect, or an image tinted by it (a texture, or a sheet frame; the sheet wins).
    //   One quad, or nine with a Border (9-slice). FillAmount crops from the min edge (health bar)
    //   and works with 9-slice too.
    // =============================================================================
    class UIImage final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        LinearColor Color;
        EUIFill     Fill       = EUIFill::None;
        float       FillAmount = 1.f;
        /** "Source.Property" to read FillAmount from (0..1); empty = the authored value. */
        OpaaxString FillBinding;

        /** A texture path (accepts a dropped .png). */
        TResourcePath<TextureResource> Texture;

        /** A sheet path. Wins over Texture. */
        TResourcePath<SpriteSheetResource> Sheet;

        /** Sheet frame; -1 = the sheet's default. */
        Int32 Frame = -1;

        /**
         * 9-slice border in texture pixels (the edges that do not stretch). All zero = one quad.
         * Ignored without a texture.
         */
        UIMargin Border;

        OPAAX_PROPERTIES(UIImage,
                         OPAAX_PROP(Color),
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(Sheet).SetTooltip("An icon cut from a sheet. Wins over Texture when set."),
                         OPAAX_PROP(Frame).SetRange(-1.f, 4096.f).SetDragStep(1.f).SetTooltip("-1 is the sheet's own default frame."),
                         OPAAX_PROP(Border).SetDragStep(1.f),   // pixels
                         OPAAX_PROP(Fill),
                         OPAAX_PROP(FillAmount).SetRange(0.f, 1.f),
                         OPAAX_PROP(FillBinding).SetTooltip("Source.Property, pulled each frame into FillAmount."))

        void SetTexturePath(const OpaaxString& InAssetPath);
        void SetSheetFrame(const OpaaxString& InSheetPath, Int32 InFrame);

        void SetColor(const LinearColor& InColor);
        void SetFill(EUIFill InFill, float InAmount);
        void SetFillAmount(float InAmount);
        void SetBorder(const UIMargin& InBorder);

        /**
         * A runtime texture, borrowed. Wins over the authored path when set.
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

        void OnPullBindings(UIBindingTable& InBindings) override;

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Texture    = nullptr;
        bool        m_bFillBound = false;   // whether a read ever succeeded (log once)
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIFill, None, Horizontal, Vertical);
