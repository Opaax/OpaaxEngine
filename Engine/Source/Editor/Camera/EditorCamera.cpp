#include "EditorCamera.h"

#if OPAAX_WITH_EDITOR

#include <algorithm>
#include <cmath>

namespace Opaax::Editor
{
    namespace
    {
        constexpr float k_ZoomStep = 1.1f;
        constexpr float k_ZoomMin  = 0.1f;
        constexpr float k_ZoomMax  = 10.f;
    }

    void EditorCamera::Pan(const Vector2F& InScreenDelta)
    {
        if (InScreenDelta.x == 0.f && InScreenDelta.y == 0.f)
        {
            return;
        }

        const float lZoom = GetZoom();
        // World delta is screen delta scaled by inverse zoom, with X negated (drag right
        // grabs world right, camera moves left) and Y unchanged-then-negated (screen-Y-down
        // grab translates to camera-Y-up in world-Y-up convention; the net is +y here).
        const Vector2F lWorldDelta{
            -InScreenDelta.x / lZoom,
            +InScreenDelta.y / lZoom
        };
        SetPosition(GetPosition() + lWorldDelta);
    }

    void EditorCamera::Zoom(float InScrollDelta, const Vector2F& InCursorLocalPx, const Vector2F& InViewportPx)
    {
        if (InScrollDelta == 0.f || InViewportPx.x <= 0.f || InViewportPx.y <= 0.f)
        {
            return;
        }

        // Snapshot the world point under the cursor BEFORE zooming.
        const Vector2F lWorldBefore = ScreenToWorld(InCursorLocalPx, InViewportPx);

        const float lOldZoom = GetZoom();
        const float lNewZoom = std::clamp(lOldZoom * std::pow(k_ZoomStep, InScrollDelta), k_ZoomMin, k_ZoomMax);
        if (lNewZoom == lOldZoom)
        {
            return;
        }
        SetZoom(lNewZoom);

        // After the zoom change, the same cursor pixel now maps to a different world point.
        // Translate the camera by the delta so the original world point lands back under
        // the cursor — the cursor-anchored zoom semantic.
        const Vector2F lWorldAfter = ScreenToWorld(InCursorLocalPx, InViewportPx);
        SetPosition(GetPosition() + (lWorldBefore - lWorldAfter));
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
