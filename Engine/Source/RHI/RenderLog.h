#pragma once

#include <cstdio>

// =============================================================================
// RenderLog — the renderer module's injected logging contract.
//   The portable render stack (IRHIDevice / RenderSystem / Renderer2D) NEVER calls
//   the engine's OPAAX_LOG (which resolves through a static locator). It logs through
//   a caller-supplied RenderLogFn. A host adapter passes a shim that forwards to its
//   own logger; standalone tools use DefaultRenderLog (stderr) for zero setup.
// =============================================================================
namespace Opaax
{
    enum class ERenderLogLevel
    {
        Trace,
        Info,
        Warn,
        Error
    };

    // Plain function pointer (POD) — no heap, trivially copyable into a RenderSystemDesc.
    using RenderLogFn = void(*)(ERenderLogLevel InLevel, const char* InMessage);

    // Default sink: stderr. Used when the host injects nothing.
    inline void DefaultRenderLog(ERenderLogLevel /*InLevel*/, const char* InMessage)
    {
        std::fprintf(stderr, "[Render] %s\n", InMessage ? InMessage : "");
    }
}
