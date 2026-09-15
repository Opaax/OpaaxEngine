#pragma once

#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIMargin.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    // =============================================================================
    // UISafeArea — a container whose rect is its own MINUS the insets (**UI20**), Unreal's SafeZone.
    //
    //   A CONTAINER, not a flag on every widget: anything that must stay clear of a TV's overscan,
    //   a phone's notch or a monitor's bezel is simply a child of one, and everything else keeps
    //   reaching the true edge. Its bounds ARE the inset rect, so children, the hit-test and the
    //   preview cannot disagree about where the safe rect is.
    //
    //   THE INSETS ARE FRACTIONS of its own rect, which is what makes it adapt: 0.05 per edge is
    //   the 5% title-safe convention, and it stays 5% on 16:9 and on 21:9 alike. An absolute inset
    //   would shrink to nothing as the target widened — the case their "adapting to all screen even
    //   wide" is about.
    //
    //   It draws nothing and is not a hit target, exactly like UIPanel.
    // =============================================================================
    class OPAAX_API UISafeArea final : public UIWidget
    {
        // =============================================================================
        // Authored state
        // =============================================================================
    public:
        /** Per edge, a fraction of my resolved rect. Clamped at resolve, never trusted raw. */
        UIMargin Insets{ 0.05f, 0.05f, 0.05f, 0.05f };

        OPAAX_PROPERTIES(UISafeArea,
                         OPAAX_PROP(Insets))

        void SetInsets(const UIMargin& InInsets);

        // =============================================================================
        // UIWidget
        // =============================================================================
    public:
        /**
         * Stretched over the parent and pass-through by default.
         *
         * A safe area that covers less than its parent is meaningless — the thing it exists to
         * measure IS the parent's edges — so it is the one widget that does not start as a
         * 100x100 box.
         */
        UISafeArea();

        OpaaxStringID GetTypeName() const noexcept override { return OPAAX_ID("UISafeArea"); }

        void SaveFields(nlohmann::json& InOutJson) const override;
        void LoadFields(const nlohmann::json& InJson) override;

    protected:
        /** My anchored rect, inset per edge. The one override the whole widget is made of. */
        Bounds2D ResolveBounds(const Bounds2D& InParentBounds) const override;
    };
}
