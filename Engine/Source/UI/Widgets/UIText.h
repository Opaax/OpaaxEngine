#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Renderer/Text/TextDrawParams.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    /** Where the block of lines sits in the rect's height. */
    enum class EUIVAlign : Uint8
    {
        Top,
        Middle,
        Bottom
    };

    inline const char* ToString(const EUIVAlign InAlign) noexcept
    {
        switch (InAlign)
        {
            case EUIVAlign::Top:    return "Top";
            case EUIVAlign::Middle: return "Middle";
            case EUIVAlign::Bottom: return "Bottom";
        }
        return "Top";
    }

    // =============================================================================
    // UIText — a string laid out in a rect. Text2D's ONE walk (TX5) with the rect as its box:
    //   wraps at the width, aligns inside it; the vertical alignment is a shift of the cached
    //   quads by the extent the walk returns.
    //
    //   The face is named by ASSET PATH and resolved through the host (IUIFontProvider) at
    //   rebuild — this module cannot name a resource. An atlas still uploading re-arms the widget
    //   for the next frame; a face the host cannot resolve draws nothing.
    // =============================================================================
    class OPAAX_API UIText final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        OpaaxString Text;
        /** A face's asset path ("/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf"). */
        OpaaxString Font;
        float       Size            = 32.f;
        LinearColor Color;
        ETextAlign  HAlign          = ETextAlign::Left;
        EUIVAlign   VAlign          = EUIVAlign::Top;
        bool        bWrap           = true;
        float       LineHeightScale = 1.f;
        bool        bKerning        = true;

        OPAAX_PROPERTIES(UIText,
                         OPAAX_PROP(Text).SetFlags(EPropertyFlags::Multiline),
                         OPAAX_PROP(Font),
                         OPAAX_PROP(Size).SetRange(1.f, 512.f),
                         OPAAX_PROP(Color),
                         OPAAX_PROP(HAlign),
                         OPAAX_PROP(VAlign),
                         OPAAX_PROP(bWrap),
                         OPAAX_PROP(LineHeightScale).SetRange(0.5f, 3.f),
                         OPAAX_PROP(bKerning))

        void SetText(const OpaaxString& InText);
        void SetFont(const OpaaxString& InFacePath);
        void SetSize(float InSize);
        void SetColor(const LinearColor& InColor);
        void SetAlign(ETextAlign InHAlign, EUIVAlign InVAlign);

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UIText"); }

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIVAlign, Top, Middle, Bottom);
