#pragma once

#include "IRenderAPI.h"

namespace Opaax
{
    class ICommandBuffer;
    class IGraphicsContext;

    /**
     * @class RenderCommand
     *
    * Static facade over the active IRenderAPI instance. Owns the device/frame lifecycle;
    * the per-frame draw work is recorded through the ICommandBuffer returned by
    * GetCommandBuffer(). Swapping backends means changing Init(), nothing else.
    *
    * s_API is set once during engine startup before any rendering occurs.
    * It is never null after Init(). No null checks on the hot path.
    */
    class OPAAX_API RenderCommand
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    private:
        RenderCommand()     = default;
        ~RenderCommand()    = default;

    public:
        RenderCommand(const RenderCommand& Other)                   = delete;
        RenderCommand(RenderCommand&& Other) noexcept               = delete;
        RenderCommand& operator=(const RenderCommand& Other)        = delete;
        RenderCommand& operator=(RenderCommand&& Other) noexcept    = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Take ownership of the API (UniquePtr) and bring it up against the graphics context.
         * @param InAPI     The API (ownership transferred)
         * @param InContext The already-initialized graphics context (Vulkan borrows its device)
         */
        static void Init(IRenderAPI* InAPI, IGraphicsContext& InContext);
        static void Shutdown();
        static void BeginFrame();
        static void EndFrame();

        // The frame's recorder (valid between BeginFrame and EndFrame). All clear/draw/bind
        // work goes through this — see ICommandBuffer.
        static ICommandBuffer& GetCommandBuffer();

        // Global viewport set (window-resize path).
        static void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height);

        // Block until the GPU finishes all submitted work (teardown before destroying resources).
        static void WaitIdle();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static UniquePtr<IRenderAPI> s_API;
    };
} // namespace Op
