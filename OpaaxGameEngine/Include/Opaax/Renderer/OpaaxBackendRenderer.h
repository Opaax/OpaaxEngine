#pragma once
#include <SDL3/SDL_video.h>

#include "IOpaaxRendererContext.h"
#include "Opaax/OpaaxStdTypes.h"
#include "Vulkan/OpaaxVulkanRenderer.h"

namespace OPAAX
{
    namespace RENDERER
    {
        enum struct OPAAX_API EOPBackendRenderer : UInt8
        {
            Unknown = 0x00,
            Vulkan  = 0x01,
            Dx12    = 0x10,
            Metal   = 0x11
        };

        OPAAX_API inline SDL_WindowFlags GetSDLWindowFlags(EOPBackendRenderer Renderer)
        {
            SDL_WindowFlags lFlags = SDL_WINDOW_RESIZABLE;
            
            switch (Renderer)
            {
            case EOPBackendRenderer::Unknown:
                OPAAX_LOG("Backend Renderer: Unknown");
                break;
            case EOPBackendRenderer::Vulkan:
                lFlags |= SDL_WINDOW_VULKAN;
                OPAAX_LOG("Backend Renderer: Vulkan");
                break;
            case EOPBackendRenderer::Dx12:
                OPAAX_LOG("Backend Renderer: Dx12");
                break;
            case EOPBackendRenderer::Metal:
                lFlags |= SDL_WINDOW_METAL;
                OPAAX_LOG("Backend Renderer: Metal");
                break;
            default: ;
            }

            return lFlags;
        }
    }
}
