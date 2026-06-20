#pragma once

#include "Core/EngineAPI.h"
#include "World/IOverlayRenderSystem.h"

namespace Opaax
{
    // =============================================================================
    // RenderStatsOverlaySystem
    //
    // First IOverlayRenderSystem implementor. Draws the previous frame's Renderer2D::GetStats()
    // in screen-space (top-left). Registered by RenderSubsystem ONLY when EngineConfig::RenderStats()
    // is true, so it costs nothing when disabled.
    //
    // The font is fetched via a per-frame AssetRegistry::Load cache hit (the handle is a frame-scoped
    // temporary), NOT a stored member: this system lives in TPolymorphicList static storage with no
    // shutdown hook, so a persistent handle would outlive AssetRegistry::Shutdown. A frame-local handle
    // never does. m_Disabled latches a one-time resolve failure so it neither retries nor log-spams.
    // =============================================================================
    class OPAAX_API RenderStatsOverlaySystem final : public IOverlayRenderSystem
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void OnRenderOverlay(World& InWorld, const RenderContext& InContext) override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        bool m_Disabled = false;   // set once if the engine font can't be resolved (stop trying)
    };
}
