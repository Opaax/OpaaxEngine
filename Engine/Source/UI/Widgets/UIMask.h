#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"       // a TYPED reference (**UI19**)
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    // NAMED, never completed — a path carries its type, not its header.
    struct TextureResource;

    // =============================================================================
    // UIMask — a container that MASKS EVERYTHING UNDER IT (**UI16**), Unity's Mask.
    //
    //   WHITE SHOWS, BLACK HIDES. The texture is stretched across this widget's own rect and the
    //   fragment multiplies alpha by `mask.r * mask.a` — a black-and-white png with no alpha reads
    //   as its luminance, a white shape on transparency reads as its silhouette. One expression,
    //   both intuitions, no mode to get wrong.
    //
    //   It masks its CHILDREN, not itself: text, images and buttons under it are all cut the same
    //   way, because the mask rides on the QUAD rather than on the widget type.
    //
    //   AN EMPTY TEXTURE PATH IS A PURE RECT CLIP, free from the same mechanism: outside the rect
    //   is discarded and inside samples white, so "clip to this box" needs no art.
    //
    //   It draws nothing of its own unless asked (Unity's "Show Mask Graphic"), and it is not a hit
    //   target, so a mask never swallows a click meant for what it masks.
    // =============================================================================
    class OPAAX_API UIMask final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        /** A texture's asset path. EMPTY = clip to the rect alone; droppable (**UI19**). */
        TResourcePath<TextureResource> Texture;

        /** Draw the mask's own shape too (white where it shows) — how an author sees where the cut is. */
        bool bShowMaskGraphic = false;

        OPAAX_PROPERTIES(UIMask,
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(bShowMaskGraphic))

        void SetTexture(const OpaaxString& InAssetPath);
        void SetShowMaskGraphic(bool bInShow);

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        UIMask() { bHitTestable = false; }

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UIMask"); }

        void SaveFields(nlohmann::json& InOutJson) const override;
        void LoadFields(const nlohmann::json& InJson) override;

        /**
         * The resolved mask texture, or null for a pure rect clip.
         *
         * Resolved at REBUILD (through the provider, like every other asset a widget names) rather
         * than at submit, because submit is const and has no host to ask — and a texture still
         * uploading re-arms the widget instead of being polled (**UI3**).
         */
        ITexture2D* GetResolvedTexture() const noexcept { return m_Resolved; }

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Resolved = nullptr;   // borrowed; the resource owns it
    };
}
