#pragma once

#include <algorithm>

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/Components/EnvironmentComponent.h"
#include "RHI/Shader.h"

namespace Opaax
{
    class FramebufferTexture;
    class ICommandBuffer;
    class IFramebuffer;
    class IPipeline;
    class IRenderTarget;
    class IRHIDevice;
    class IShader;
    class ITexture2D;
    class IUniformBuffer;
    class OffscreenRenderTarget;
    struct ShadowBlock2D;

    OPAAX_LOG_CATEGORY(ScenePipeline2D);

    /** One view's post-process settings, from the world's EnvironmentComponent. */
    struct PostSettings
    {
        float       ExposureStops = 0.f;
        ETonemapper Tonemapper    = ETonemapper::ACES;

        /** 0: no bloom. */
        float BloomIntensity = 0.f;
        float BloomThreshold = 1.f;
        float BloomSoftness  = 0.5f;

        static PostSettings From(const EnvironmentComponent& InEnvironment) noexcept
        {
            PostSettings lSettings;
            lSettings.ExposureStops  = InEnvironment.Exposure;
            lSettings.Tonemapper     = InEnvironment.Tonemapper;
            lSettings.BloomIntensity = InEnvironment.bBloom ? std::max(InEnvironment.BloomIntensity, 0.f) : 0.f;
            lSettings.BloomThreshold = std::max(InEnvironment.BloomThreshold, 0.f);
            lSettings.BloomSoftness  = std::clamp(InEnvironment.BloomSoftness, 0.f, 1.f);
            return lSettings;
        }
    };

    /** The most levels a bloom chain has. */
    inline constexpr Uint32 MAX_BLOOM_LEVELS_2D = 6;

    /** One level of a bloom chain, pixels. */
    struct BloomLevel2D
    {
        Uint32 Width  = 1;
        Uint32 Height = 1;
    };

    /**
     * The bloom chain of a scene InWidth x InHeight: half its size, then each level half the one
     * before while both sides keep 4 pixels, MAX_BLOOM_LEVELS_2D at most. The first level always exists.
     */
    TDynArray<BloomLevel2D> MakeBloomLevels2D(Uint32 InWidth, Uint32 InHeight);

    /** The shaders of the HDR path. Only the tonemap is required: without another, its feature is off (logged). */
    struct ScenePipelineShaders
    {
        ShaderDesc Tonemap;
        ShaderDesc Shadow;
        ShaderDesc AmbientOcclusion;
        ShaderDesc Bloom;
    };

    // =============================================================================
    // ScenePipeline2D — the HDR path of a world view: the world is drawn into a linear HDR target,
    //   then composited (exposure, tonemap, back to the screen's gamma) into the view's target.
    //   The shadow casters come first, drawn into an occlusion map. From it, a shadow map holds,
    //   for each shadowed light, how far it gets in every direction, and the ambient occlusion map
    //   how much of the casters surrounds each point (the coverage shrunk, then blurred).
    //   Bloom is made in the composite: the bright light halved down a chain, then blurred back up.
    //   Owned by the RenderSystem; its targets follow the size of the view being drawn.
    // =============================================================================
    class ScenePipeline2D
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ScenePipeline2D();
        ~ScenePipeline2D();

        ScenePipeline2D(const ScenePipeline2D&)            = delete;
        ScenePipeline2D& operator=(const ScenePipeline2D&) = delete;

        // =============================================================================
        // Lifecycle
        // =============================================================================
    public:
        /** Creates the shaders and buffers. False (logged) when the tonemap shader is missing. */
        bool Init(IRHIDevice& InDevice, const ScenePipelineShaders& InShaders);
        void Shutdown();

        bool IsReady() const noexcept;

        /** Whether BuildShadowMap can work. */
        bool CanShadow() const noexcept;

        /** Whether BuildAmbientOcclusion can work. */
        bool CanAmbientOcclusion() const noexcept;

        /** Whether the composite can add bloom. */
        bool CanBloom() const noexcept;

        // =============================================================================
        // Frame
        // =============================================================================
    public:
        /** The HDR target the world is drawn into, sized InWidth x InHeight. */
        IRenderTarget& PrepareScene(Uint32 InWidth, Uint32 InHeight);

        /** Writes every pixel of InOutput from the scene: bloom, exposure, tonemap, gamma. */
        void Composite(ICommandBuffer& InCmd, IRenderTarget& InOutput, const PostSettings& InSettings);

        /** The target the shadow casters are drawn into (coverage), sized InWidth x InHeight. */
        IRenderTarget& PrepareOcclusion(Uint32 InWidth, Uint32 InHeight);

        /**
         * Fills the shadow map from the occlusion map: a row per light of InShadows.
         * @return The shadow map to sample, or null (no shadow this frame)
         */
        ITexture2D* BuildShadowMap(ICommandBuffer& InCmd, const ShadowBlock2D& InShadows);

        /**
         * Fills the ambient occlusion map from the occlusion map: its coverage, shrunk then blurred.
         * @param InSigma The blur, ambient occlusion map texels (AmbientOcclusionSigma2D)
         * @return The ambient occlusion map to sample, or null
         */
        ITexture2D* BuildAmbientOcclusion(ICommandBuffer& InCmd, float InSigma);

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** One fullscreen pass of the ambient occlusion shader, from InSource into InTarget. */
        void RunAmbientOcclusionPass(ICommandBuffer& InCmd, Uint32 InMode, float InSigma, const IFramebuffer& InSource,
                                     IRenderTarget& InTarget);

        /**
         * Makes the scene's bloom: down the chain, then back up, added into each level.
         * @return Levels used (the result is in the first), 0 for none
         */
        Uint32 BuildBloom(ICommandBuffer& InCmd, const PostSettings& InSettings);

        /** One fullscreen pass of the bloom shader, from InSource into InTarget (added into it when bInAdd). */
        void RunBloomPass(ICommandBuffer& InCmd, Uint32 InMode, const PostSettings& InSettings,
                          const IFramebuffer& InSource, IRenderTarget& InTarget, bool bInAdd);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IRHIDevice* m_Device = nullptr;

        TUniquePtr<IFramebuffer>          m_Scene;
        TUniquePtr<OffscreenRenderTarget> m_SceneTarget;

        TUniquePtr<IShader>        m_TonemapShader;
        TUniquePtr<IPipeline>      m_TonemapPipeline;
        TUniquePtr<IUniformBuffer> m_PostUBO;

        // Shadows: the casters' coverage (R8), then a distance per light and direction (R16F).
        TUniquePtr<IFramebuffer>          m_Occlusion;
        TUniquePtr<OffscreenRenderTarget> m_OcclusionTarget;
        TUniquePtr<IFramebuffer>          m_ShadowMap;
        TUniquePtr<OffscreenRenderTarget> m_ShadowMapTarget;
        TUniquePtr<FramebufferTexture>    m_ShadowMapTexture;

        TUniquePtr<IShader>        m_ShadowShader;
        TUniquePtr<IPipeline>      m_ShadowPipeline;
        TUniquePtr<IUniformBuffer> m_ShadowUBO;

        // Ambient occlusion: two small R8 maps the passes go back and forth between; [0] is the result.
        TUniquePtr<IFramebuffer>          m_AmbientOcclusion[2];
        TUniquePtr<OffscreenRenderTarget> m_AmbientOcclusionTarget[2];
        TUniquePtr<FramebufferTexture>    m_AmbientOcclusionTexture;

        TUniquePtr<IShader>        m_AmbientOcclusionShader;
        TUniquePtr<IPipeline>      m_AmbientOcclusionPipeline;
        TUniquePtr<IUniformBuffer> m_AmbientOcclusionUBO;

        // Bloom: a chain of HDR levels, each half the one before; [0] holds the result.
        TDynArray<TUniquePtr<IFramebuffer>>          m_BloomLevels;
        TDynArray<TUniquePtr<OffscreenRenderTarget>> m_BloomTargets;

        TUniquePtr<IShader>        m_BloomShader;
        TUniquePtr<IPipeline>      m_BloomPipeline;      // writes
        TUniquePtr<IPipeline>      m_BloomAddPipeline;   // adds (the way back up)
        TUniquePtr<IUniformBuffer> m_BloomUBO;
    };
}
