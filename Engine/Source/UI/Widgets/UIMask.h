#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    class ITexture2D;

    // Forward-declared: TResourcePath only needs the name.
    struct TextureResource;

    // =============================================================================
    // UIMask — masks everything under it (like Unity's Mask). White shows, black hides
    //   (alpha *= mask.r * mask.a). The texture is stretched over this widget's rect.
    //   An empty texture path clips to the rect. Draws nothing itself unless bShowMaskGraphic,
    //   and is not hit-testable.
    // =============================================================================
    class UIMask final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        /** A texture path. Empty = clip to the rect. */
        TResourcePath<TextureResource> Texture;

        /** Also draw the mask's own shape (to see where it cuts). */
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
         * The resolved mask texture, or null for a rect clip. Resolved at rebuild (a texture still
         * uploading re-arms the widget).
         */
        ITexture2D* GetResolvedTexture() const noexcept { return m_Resolved; }

    protected:
        void Rebuild(const UIBuildContext& InContext, TDynArray<UIQuad>& OutQuads) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ITexture2D* m_Resolved = nullptr;   // borrowed
    };
}
