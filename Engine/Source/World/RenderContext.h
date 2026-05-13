#pragma once

namespace Opaax
{
    class Camera2D;

    // Render-side context passed to every IWorldSystem::OnRender call.
    // Kept minimal on purpose — add fields only when a concrete system needs them.
    struct RenderContext
    {
        Camera2D& Camera;
        double    Alpha;
    };
}
