#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Renderer/Textures/SpriteSheet.h"   // complete: a widget class visits its sheet reference
#include "Renderer/Textures/Texture2D.h"     // and its texture reference
#include "Resources/Asset/AssetRef.h"
#include "UI/UIBrush.h"
#include "UI/UIMargin.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

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
    // UIImage — draws its brush: a colour rect, or an image tinted by it (a texture, or a sheet frame;
    //   the sheet wins). One quad, or nine with a border (9-slice): the brush's Border, else the sheet
    //   frame's. FillAmount crops from the min edge (health bar) and works with 9-slice too.
    // =============================================================================
    class UIImage final : public UIWidget
    {
        OPAAX_OBJECT(UIImage, UIWidget)

        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        /** What it draws: the tint, the texture or sheet frame, the 9-slice border. */
        UIBrush     Brush;
        EUIFill     Fill       = EUIFill::None;
        float       FillAmount = 1.f;
        /** "Source.Property" to read FillAmount from (0..1); empty = the authored value. */
        OpaaxString FillBinding;

        OPAAX_PROPERTIES(UIImage,
                         OPAAX_PROP(Brush),
                         OPAAX_PROP(Fill),
                         OPAAX_PROP(FillAmount).SetRange(0.f, 1.f),
                         OPAAX_PROP(FillBinding).SetTooltip("Source.Property, pulled each frame into FillAmount."))

        /** Brush.Texture. */
        void SetTexturePath(const OpaaxString& InAssetPath);
        /** Brush.Sheet and Brush.Frame. */
        void SetSheetFrame(const OpaaxString& InSheetPath, Int32 InFrame);

        /** Brush.Tint. */
        void SetColor(const LinearColor& InColor);
        void SetFill(EUIFill InFill, float InAmount);
        void SetFillAmount(float InAmount);
        void SetBorder(const UIMargin& InBorder);
        void SetBorderScale(float InBorderScale);

        /**
         * A runtime texture, borrowed. Wins over the authored path when set.
         */
        void        SetTexture(ITexture2D* InTexture);
        ITexture2D* GetTexture() const noexcept { return m_Texture; }

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        void OnPullBindings(UIBindingTable& InBindings) override;

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Texture    = nullptr;
        bool        m_bFillBound = false;   // whether a read ever succeeded (log once)
        bool        m_bWarnedTiles = false; // too many tiles, said once
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIFill, None, Horizontal, Vertical);
