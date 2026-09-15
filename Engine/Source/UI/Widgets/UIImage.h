#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"       // a TYPED reference (**UI19**)
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "UI/UIMargin.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    // NAMED, never completed — a path carries its type, not its header.
    struct TextureResource;
    struct SpriteSheetResource;

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
    // UIImage — a rect of colour, or an image tinted by it: a texture, or one FRAME of a sheet
    //   (**UI25** — the sheet wins when both are named, the sprite's rule). ONE quad, unless a
    //   Border makes it nine (**UI20**), measured against the frame. A fill crops both the rect
    //   and the sampled UVs from the min edge, which is the health bar — and it is a CLIP over
    //   whatever geometry was emitted, so a filled 9-slice keeps its caps.
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
        /** "Source.Property" to pull FillAmount from each frame (a 0..1 number); empty = authored (UI24). */
        OpaaxString FillBinding;

        /** A texture's asset path. TYPED, so a `.png` can be DROPPED on it (**UI19**). */
        TResourcePath<TextureResource> Texture;

        /** A sheet's asset path — an icon cut from an atlas. Wins over Texture when set (**UI25**). */
        TResourcePath<SpriteSheetResource> Sheet;

        /** Which frame of the sheet; -1 is the sheet's own default. */
        Int32 Frame = -1;

        /**
         * The 9-slice border, in TEXTURE PIXELS — how much of each edge must NOT stretch (**UI20**).
         *
         * There is no Sliced mode to pick: a non-zero border on a texture IS sliced, and an
         * all-zero one is the single quad this widget always emitted. Ignored without a texture,
         * since a border is a statement about art.
         */
        UIMargin Border;

        OPAAX_PROPERTIES(UIImage,
                         OPAAX_PROP(Color),
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(Sheet).SetTooltip("An icon cut from a sheet. Wins over Texture when set."),
                         OPAAX_PROP(Frame).SetRange(-1.f, 4096.f).SetDragStep(1.f).SetTooltip("-1 is the sheet's own default frame."),
                         OPAAX_PROP(Border).SetDragStep(1.f),   // pixels; flows to the four edges
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

        void OnPullBindings(UIBindingTable& InBindings) override;

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Texture    = nullptr;
        bool        m_bFillBound = false;   // whether a pull has ever resolved — one log line
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIFill, None, Horizontal, Vertical);
