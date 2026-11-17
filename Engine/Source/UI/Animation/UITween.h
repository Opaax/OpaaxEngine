#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/Easing.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/Asset/Asset.h"
#include "UI/UIPose.h"

namespace Opaax
{
    /** What a tween does at its last key. The UI's own: the animation clip's mode brings the renderer's textures. */
    enum class EUILoop : Uint8
    {
        Once,       // stops at the end (holding the last pose, or at rest)
        Loop,       // starts again from the first key
        PingPong    // plays back to the first key, then forth again
    };

    inline const char* ToString(const EUILoop InLoop) noexcept
    {
        switch (InLoop)
        {
        case EUILoop::Once:     return "Once";
        case EUILoop::Loop:     return "Loop";
        case EUILoop::PingPong: return "PingPong";
        }
        return "Once";
    }
}

OPAAX_ENUM_VALUES(Opaax::EUILoop, Once, Loop, PingPong)

namespace Opaax
{
    /** One moment of a tween: the pose at Time, reached from the previous key along Ease (CSS @keyframes). */
    struct UITweenKey
    {
        /** Seconds from the start. */
        float  Time = 0.f;
        /** The curve from the previous key to this one. */
        EEase  Ease = EEase::Linear;
        UIPose Pose;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UITweenKey, Time, Ease, Pose)

        OPAAX_PROPERTIES(UITweenKey,
                         OPAAX_PROP(Time).SetRange(0.f, 600.f).SetDragStep(0.01f)
                                         .SetTooltip("Seconds from the start."),
                         OPAAX_PROP(Ease).SetTooltip("The curve from the previous key to this one."),
                         OPAAX_PROP(Pose).SetTooltip("The pose at this moment."))
    };

    /**
     * The pose of keys InKeys (in time order) at InTime seconds, stateless like SampleClip: any frame
     * rate, any hitch, gives the pose of that moment. Before the first key: its pose. Between two: eased
     * by the second's curve. No key: the rest pose.
     * @param OutFinished True once a Once tween is past its last key (never for Loop and PingPong)
     * @param bInHoldEnd Once, past the end: the last pose (true) or the rest pose (false)
     */
    UIPose SampleTweenKeys(const TDynArray<UITweenKey>& InKeys, EUILoop InLoop, bool bInHoldEnd, float InTime,
                           bool& OutFinished);

    /** Puts keys in time order (equal times keep their order). */
    void SortTweenKeys(TDynArray<UITweenKey>& InOutKeys);

    // =============================================================================
    // UITween — keyed poses over time as an asset (a "UI Tween" preset: a pop-in, a fade, a pulse, a
    //   shake). A UITweenModifier plays one on a widget; many widgets share it.
    // =============================================================================
    class UITween final : public Asset
    {
        OPAAX_OBJECT(UITween, Asset)

    public:
        /** In time order (kept so on edit). */
        TDynArray<UITweenKey> Keys;
        EUILoop               Loop     = EUILoop::Once;
        /** Once: the last pose stays at the end; off, the widget goes back to rest. */
        bool                  bHoldEnd = true;

        OPAAX_PROPERTIES(UITween,
                         OPAAX_PROP(Keys).SetTooltip("The poses and when: sorted by time when edited."),
                         OPAAX_PROP(Loop).SetTooltip("Once stops at the end, Loop starts again,\n"
                                                     "PingPong plays back and forth."),
                         OPAAX_PROP(bHoldEnd).SetTooltip("Once: the last pose stays at the end.\n"
                                                         "Off, the widget goes back to rest."))

        /** Seconds from the start to the last key (0 without keys). */
        float GetLength() const noexcept { return Keys.empty() ? 0.f : Keys.back().Time; }

        //~Begin Object interface
        /** Sorts the keys by time, so the editor may add them in any order. */
        void PostEditChange() override;
        //~End Object interface
    };

    /** SampleTweenKeys on a tween asset's keys and settings. */
    UIPose SampleTween(const UITween& InTween, float InTime, bool& OutFinished);
}
