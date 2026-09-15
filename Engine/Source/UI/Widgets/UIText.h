#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"       // a TYPED reference — what the drop target keys on (**UI19**)
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "Renderer/Text/TextDrawParams.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    // NAMED, never completed — a path carries its type, not its header.
    struct FontFaceResource;

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
    //   The face is named by ASSET PATH and resolved through the host (IUIAssetProvider) at
    //   rebuild — this module cannot name a resource. An atlas still uploading re-arms the widget
    //   for the next frame; a face the host cannot resolve draws nothing.
    //
    //   BOUND, the authored Text is the FORMAT (UI24): "Jumps: {}" with Binding "Hud.Jumps" shows
    //   the value in the braces; a Text with no braces is replaced whole. Unbound (or unresolved),
    //   it shows as written — which is what the editor's preview shows, having no sources.
    // =============================================================================
    class OPAAX_API UIText final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        OpaaxString Text;
        /** "Source.Property" to pull the text from each frame; empty = Text is what shows. */
        OpaaxString Binding;
        /** A face's asset path. TYPED, so the editor gives it a `.ttf` drop target (**UI19**). */
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

        /** What is drawn: the bound, formatted text when a binding resolved, else Text. */
        const OpaaxString& GetDisplayText() const noexcept { return m_bBound ? m_BoundText : Text; }

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString m_BoundText;        // the last formatted value
        bool        m_bBound = false;   // whether a pull has ever resolved
    };
}

OPAAX_ENUM_VALUES(Opaax::EUIVAlign, Top, Middle, Bottom);
