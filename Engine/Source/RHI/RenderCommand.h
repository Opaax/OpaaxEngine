#pragma once

#include "IRenderAPI.h"

namespace Opaax
{
    /**
     * @class RenderCommand
     *
    * Static facade over the active IRenderAPI instance.
    * Renderer2D calls RenderCommand::DrawIndexed — never raw GL/Vulkan/etc...
    * Swapping backends means changing Init(), nothing else.
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
         * Take the ownership of the API to encapsulate it in UniquePtr
         * @param InAPI The API
         */
        static void Init(IRenderAPI* InAPI);
        static void Shutdown();
        static void SetViewport(Uint32 X, Uint32 Y, Uint32 Width, Uint32 Height);
        static void SetClearColor(float Red, float Green, float Blue, float Alpha);
        static void Clear();
        static void DrawIndexed(Uint32 IndexCount);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static UniquePtr<IRenderAPI> s_API;
    };
} // namespace Op
