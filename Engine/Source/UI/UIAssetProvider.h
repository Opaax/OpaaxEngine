#pragma once

#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Renderer/Text/Text2D.h"

namespace Opaax
{
    class ITexture2D;

    /** One sheet frame: the sheet's texture, the frame's UVs, and its size in pixels. */
    struct UISheetFrameView
    {
        ITexture2D* Texture = nullptr;   // null while uploading, or if nothing loads
        Vector2F    UVMin   = { 0.f, 0.f };
        Vector2F    UVMax   = { 1.f, 1.f };
        Vector2F    SizePx  = { 0.f, 0.f };
    };

    // =============================================================================
    // IUIAssetProvider — resolves assets for widgets by path (implemented by the renderer),
    //   so the UI module never touches the ResourceManager. Answers may be empty while loading;
    //   the widget then re-arms itself.
    // =============================================================================
    class IUIAssetProvider
    {
    public:
        virtual ~IUIAssetProvider() = default;

        /** @param InAssetPath A font face path ("/Engine/Fonts/Roboto/roboto-latin-700-normal.ttf") */
        virtual FontFaceView ResolveFace(const char* InAssetPath) = 0;

        /**
         * @param InAssetPath A texture path ("UI/MaskCircle.png")
         * @return Null while uploading, or if nothing loads
         */
        virtual ITexture2D* ResolveTexture(const char* InAssetPath) = 0;

        /**
         * A sprite sheet frame. Defaults to nothing (test stubs need not implement it).
         * A missing frame index gives the whole texture.
         * @param InFrame Frame index; -1 = the sheet's default
         */
        virtual UISheetFrameView ResolveSheetFrame(const char* InSheetPath, Int32 InFrame)
        {
            (void)InSheetPath; (void)InFrame;
            return {};
        }
    };

    /** What a rebuild may use from its host. Every pointer is optional. */
    struct UIBuildContext
    {
        IUIAssetProvider* Assets = nullptr;
    };
}
