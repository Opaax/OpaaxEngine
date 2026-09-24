#include "UI/UIImageSource.h"

#include "RHI/Texture.h"

namespace Opaax
{
    bool ResolveImageSource(const UIBuildContext& InContext, ITexture2D* const InRuntime,
                            const TResourcePath<TextureResource>& InTexture,
                            const TResourcePath<SpriteSheetResource>& InSheet, const Int32 InFrame,
                            UIResolvedImage& Out, bool& bOutNamed)
    {
        Out       = {};
        bOutNamed = InRuntime != nullptr || !InSheet.IsEmpty() || !InTexture.IsEmpty();

        if (InRuntime != nullptr)
        {
            Out.Texture = InRuntime;
            Out.SizePx  = { static_cast<float>(InRuntime->GetWidth()), static_cast<float>(InRuntime->GetHeight()) };
            return true;
        }

        if (InContext.Assets == nullptr)
        {
            return false;   // nobody to ask — a test with no provider, or a plain colour
        }

        if (!InSheet.IsEmpty())
        {
            const UISheetFrameView lFrame = InContext.Assets->ResolveSheetFrame(InSheet.Path.CStr(), InFrame);
            if (lFrame.Texture == nullptr)
            {
                return false;   // still uploading, or the sheet did not load
            }

            Out.Texture = lFrame.Texture;
            Out.UVMin   = lFrame.UVMin;
            Out.UVMax   = lFrame.UVMax;
            Out.SizePx  = lFrame.SizePx;
            return true;
        }

        if (!InTexture.IsEmpty())
        {
            Out.Texture = InContext.Assets->ResolveTexture(InTexture.Path.CStr());
            if (Out.Texture == nullptr)
            {
                return false;
            }

            Out.SizePx = { static_cast<float>(Out.Texture->GetWidth()), static_cast<float>(Out.Texture->GetHeight()) };
            return true;
        }

        return false;
    }
}
