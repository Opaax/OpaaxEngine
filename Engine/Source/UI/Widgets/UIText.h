#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "Renderer/Text/TextDrawParams.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    // Forward-declared: TResourcePath only needs the name.
    struct FontFaceResource;

    /** Vertical position of the lines in the rect. */
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
    // UIText — text laid out in a rect (wraps at the width, aligns inside).
    //   The font is an asset path resolved through the host; while uploading it retries next frame.
    //   With a Binding, Text is the format: "Jumps: {}" with "Hud.Jumps" shows the value in the
    //   braces; without braces the value replaces the text. Unbound, the text shows as written.
    // =============================================================================
    class UIText final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        OpaaxString Text;
        /** "Source.Property" to read the text from; empty = Text is shown. */
        OpaaxString Binding;
        /** A font face path (accepts a dropped .ttf). */
        TResourcePath<FontFaceResource> Font;
        float       Size            = 32.f;
        LinearColor Color;
        ETextAlign  HAlign          = ETextAlign::Left;
        EUIVAlign   VAlign          = EUIVAlign::Top;
        bool        bWrap           = true;
        float       LineHeightScale = 1.f;
        bool        bKerning        = true;

        OPAAX_PROPERTIES(UIText,
                         OPAAX_PROP(Text).SetFlags(EPropertyFlags::Multiline),
                         OPAAX_PROP(Binding).SetTooltip("Source.Property, pulled each frame; Text is then the format and {} is the value."),
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

        void SaveFields(nlohmann::json& InOutJson) const override;
        void LoadFields(const nlohmann::json& InJson) override;

        void OnPullBindings(UIBindingTable& InBindings) override;

        /** What is drawn: the formatted bound value when available, else Text. */
        const OpaaxString& GetDisplayText() const noexcept { return m_bBound ? m_BoundText : Text; }

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString m_BoundText;        // last formatted value
        bool        m_bBound = false;   // whether a read ever succeeded
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIVAlign, Top, Middle, Bottom);
