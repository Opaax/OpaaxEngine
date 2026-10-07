#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/Components/EnvironmentComponent.h"

namespace Opaax
{
    class ICommandBuffer;
    class IFramebuffer;
    class IPipeline;
    class IRenderTarget;
    class IRHIDevice;
    class IShader;
    class IUniformBuffer;
    class OffscreenRenderTarget;
    struct ShaderDesc;

    OPAAX_LOG_CATEGORY(ScenePipeline2D);

    /** One view's post-process settings, from the world's EnvironmentComponent. */
    struct PostSettings
    {
        float       ExposureStops = 0.f;
        ETonemapper Tonemapper    = ETonemapper::ACES;

        static PostSettings From(const EnvironmentComponent& InEnvironment) noexcept
        {
            return PostSettings{ InEnvironment.Exposure, InEnvironment.Tonemapper };
        }
    };

    // =============================================================================
    // ScenePipeline2D — the HDR path of a world view: the world is drawn into a linear HDR target,
    //   then composited (exposure, tonemap, back to the screen's gamma) into the view's target.
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
        bool Init(IRHIDevice& InDevice, const ShaderDesc& InTonemapShader);
        void Shutdown();

        bool IsReady() const noexcept;

        // =============================================================================
        // Frame
        // =============================================================================
    public:
        /** The HDR target the world is drawn into, sized InWidth x InHeight. */
        IRenderTarget& PrepareScene(Uint32 InWidth, Uint32 InHeight);

        /** Writes every pixel of InOutput from the scene: exposure, tonemap, gamma. */
        void Composite(ICommandBuffer& InCmd, IRenderTarget& InOutput, const PostSettings& InSettings);

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
    };
}
