#include "RenderStatsOverlaySystem.h"

#include "Renderer/Renderer2D.h"
#include "Renderer/RenderStats.h"
#include "Renderer/Text/Text2D.h"
#include "Renderer/Text/FontAsset.h"
#include "Renderer/RenderTarget.hpp"
#include "World/RenderContext.h"
#include "Assets/AssetHandle.hpp"
#include "Assets/AssetRegistry.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"

#include <cstdio>

namespace Opaax
{
    void RenderStatsOverlaySystem::OnRenderOverlay(World& /*InWorld*/, const RenderContext& InContext)
    {
        if (m_Disabled) { return; }

        // Frame-scoped cache-hit load (first frame bakes; the registry keeps its own ref so the font
        // stays cached). The handle releases at function scope — never outlives AssetRegistry::Shutdown.
        TAssetHandle<FontAsset> lFont = AssetRegistry::Load<FontAsset>(OPAAX_ID("Fonts/Roboto-Regular"));
        if (!lFont.IsValid())
        {
            OPAAX_CORE_WARN("RenderStatsOverlaySystem: engine font 'Fonts/Roboto-Regular' unavailable — overlay disabled.");
            m_Disabled = true;
            return;
        }

        const RenderStats& lStats = Renderer2D::GetStats();

        char lBuf[256];
        std::snprintf(lBuf, sizeof(lBuf),
            "Draw calls: %u\nBatches: %u\nQuads: %u\nPeak slots: %u\nSort: %.1f us\nRing HW: %u\nCmd cap: %u",
            lStats.DrawCalls, lStats.Batches, lStats.Quads, lStats.PeakTextureSlots,
            lStats.SortMicros, lStats.RingHighWater, lStats.CommandCapacity);

        // Screen-space: the OverlayRenderPass binds a ScreenSpaceCamera (bottom-left origin, Y-up,
        // pixel units). Text2D anchors at the top-left of the first glyph and advances downward, so
        // start near the TOP edge (y = height - margin).
        constexpr float k_Margin = 12.f;
        const Vector2F  lPos{ k_Margin, static_cast<float>(InContext.Target.GetHeight()) - k_Margin };

        Text2D::DrawParams lParams;
        lParams.Color = Vector4F(0.25f, 1.f, 0.45f, 1.f); // green, readable over most scenes
        lParams.Scale = 0.55f;                            // 32 px bake -> ~18 px lines

        Text2D::DrawString(lBuf, lPos, *lFont, lParams);
    }
}
