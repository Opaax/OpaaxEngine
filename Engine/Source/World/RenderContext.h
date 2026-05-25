#pragma once

namespace Opaax
{
    class ICamera;

    // Render-side context passed to every IWorldSystem::OnRender call.
    // Kept minimal on purpose — add fields only when a concrete system needs them.
    struct RenderContext
    {
        ICamera& Camera;
        double   Alpha;
    };
}
