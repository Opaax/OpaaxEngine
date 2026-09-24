#pragma once

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/Text/Text2D.h"

namespace Opaax
{
    class ITexture2D;

    /** One frame of a sheet: the sheet's texture, the frame's UVs in it, and the frame's size in pixels. */
    struct UISheetFrameView
    {
        ITexture2D* Texture = nullptr;   // null while the upload is in flight, or when nothing loads
        Vector2F    UVMin   = { 0.f, 0.f };
        Vector2F    UVMax   = { 1.f, 1.f };
        Vector2F    SizePx  = { 0.f, 0.f };
    };

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

        /**
         * A frame of a sprite sheet — an icon cut from an atlas (U12).
         *
         * DEFAULTED to nothing: a host with no sheets (a test stub) need not know the type. A frame
         * index the sheet does not have answers the whole texture, the sprite's rule.
         *
         * @param InFrame the frame's index; -1 = the sheet's own default.
         */
        virtual UISheetFrameView ResolveSheetFrame(const char* InSheetPath, Int32 InFrame)
        {
            (void)InSheetPath; (void)InFrame;
            return {};
        }
    };

    /** What a rebuild may ask of its host. Every pointer is optional — a test passes none. */
    struct UIBuildContext
    {
        IUIAssetProvider* Assets = nullptr;
    };
}
