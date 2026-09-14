#pragma once

#include "Core/EngineAPI.h"
#include "Renderer/Text/Text2D.h"

namespace Opaax
{
    // =============================================================================
    // IUIFontProvider — how a widget names a face without knowing what a resource is. The host
    //   (the renderer, which already caches faces by path) answers with a FontFaceView; the atlas
    //   is null while the upload is in flight (TX4), and a widget that needs it re-arms itself.
    // =============================================================================
    class OPAAX_API IUIFontProvider
    {
    public:
        virtual ~IUIFontProvider() = default;

        /** @param InAssetPath a face's asset path ("/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf"). */
        virtual FontFaceView ResolveFace(const char* InAssetPath) = 0;
    };

    /** What a rebuild may ask of its host. Every pointer is optional — a test passes none. */
    struct UIBuildContext
    {
        IUIFontProvider* Fonts = nullptr;
    };
}
