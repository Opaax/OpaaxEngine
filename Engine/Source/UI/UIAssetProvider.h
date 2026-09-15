#pragma once

#include "Core/EngineAPI.h"
#include "Renderer/Text/Text2D.h"

namespace Opaax
{
    class ITexture2D;

    // =============================================================================
    // IUIAssetProvider — how a widget names an asset without knowing what a resource is.
    //
    //   The host (the renderer, which already caches both faces and textures by path) answers; the
    //   UI module never learns what a `ResourceManager` is (**UI1**). It resolves TWO kinds now —
    //   a face for text, a texture for an image or a mask (**UI17**) — which is why it is no
    //   longer called a font provider.
    //
    //   Both answers may be EMPTY while a load is in flight, and a widget that needs one re-arms
    //   itself rather than polling (**UI3**).
    // =============================================================================
    class OPAAX_API IUIAssetProvider
    {
    public:
        virtual ~IUIAssetProvider() = default;

        /** @param InAssetPath a face's asset path ("/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf"). */
        virtual FontFaceView ResolveFace(const char* InAssetPath) = 0;

        /**
         * @param InAssetPath a texture's asset path ("UI/MaskCircle.png").
         * @return null while the upload is in flight, or when nothing loads from there.
         */
        virtual ITexture2D* ResolveTexture(const char* InAssetPath) = 0;
    };

    /** What a rebuild may ask of its host. Every pointer is optional — a test passes none. */
    struct UIBuildContext
    {
        IUIAssetProvider* Assets = nullptr;
    };
}
