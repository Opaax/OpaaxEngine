#pragma once

#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetRef.h"
#include "UI/UIAssetProvider.h"
#include "UI/UIStateBrushes.h"

namespace Opaax
{
    // =============================================================================
    // UIButtonStyle — a button's look as an asset (a "UI Button Style"): the brush of each state and the
    //   tint's fade. Many buttons name one and share it; saving it reaches every one of them, as
    //   Unreal's button style and CommonUI's style asset do. A button's own Brushes show when it
    //   names none, or when it does not load.
    // =============================================================================
    class UIButtonStyle final : public Asset
    {
        OPAAX_OBJECT(UIButtonStyle, Asset)

    public:
        UIStateBrushes Brushes;

        OPAAX_PROPERTIES(UIButtonStyle,
                         OPAAX_PROP(Brushes).SetTooltip("What each state draws, and how the tint moves between them."))

        //~Begin Object interface
        /** Keeps the fade's duration at 0 or more. */
        void PostEditChange() override;
        //~End Object interface
    };

    /**
     * The style InStyle names, read through InAssets each time (a saved style reloads in place), or null:
     * none named, no provider yet, or it does not load as a UI Button Style.
     */
    const UIButtonStyle* ResolveButtonStyle(IUIAssetProvider* InAssets, const TAssetRef<UIButtonStyle>& InStyle);
}
