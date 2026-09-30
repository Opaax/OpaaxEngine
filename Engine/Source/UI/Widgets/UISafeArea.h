#pragma once

#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIMargin.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    // =============================================================================
    // UISafeArea — a container whose rect is its own minus the insets (like Unreal's SafeZone).
    //   Children stay clear of overscan, notches and bezels. Insets are fractions of the rect
    //   (0.05 = 5% title-safe on any aspect). Draws nothing, not hit-testable.
    // =============================================================================
    class OPAAX_API UISafeArea final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        /** Per edge, a fraction of my rect. Clamped at resolve. */
        UIMargin Insets{ 0.05f, 0.05f, 0.05f, 0.05f };

        OPAAX_PROPERTIES(UISafeArea,
                         OPAAX_PROP(Insets).SetRange(0, 1).SetDragStep(0.01f))

        void SetInsets(const UIMargin& InInsets);

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        /**
         * Stretched over the parent and not hit-testable by default.
         */
        UISafeArea();

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UISafeArea"); }

        void SaveFields(nlohmann::json& InOutJson) const override;
        void LoadFields(const nlohmann::json& InJson) override;

    protected:
        /** My rect, inset per edge. */
        Bounds2D ResolveBounds(const Bounds2D& InParentBounds) const override;
    };
}
