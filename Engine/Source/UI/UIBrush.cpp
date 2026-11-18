#include "UI/UIBrush.h"

#include "Object/Object.h"

namespace Opaax
{
    UIBrushClip SampleBrushClip(IUIAssetProvider* const InAssets, const UIBrush& InBrush, const float InSeconds)
    {
        UIBrushClip lOut;
        if (InBrush.Animation.IsEmpty() || InAssets == nullptr)
        {
            return lOut;
        }

        lOut.Clip = Cast<AnimationClip>(InAssets->ResolveAsset(InBrush.Animation.Path.CStr()));
        if (lOut.Clip == nullptr)
        {
            lOut.Issue = EUIBrushIssue::ClipMissing;
            return lOut;
        }

        const AnimationSample lSample = SampleClip(*lOut.Clip, InSeconds);
        lOut.Step      = lSample.Step;
        lOut.bFinished = lSample.bFinished;
        return lOut;
    }

    bool ResolveBrush(const UIBuildContext& InContext, const UIBrush& InBrush, ITexture2D* const InRuntime,
                      const float InSeconds, UIResolvedImage& Out, bool& bOutNamed, UIBrushClip* const OutClip)
    {
        UIBrushClip lClip;
        if (InRuntime == nullptr)
        {
            lClip = SampleBrushClip(InContext.Assets, InBrush, InSeconds);
        }

        bool bResolved = false;
        const AnimationStep* lStep = lClip.Clip != nullptr ? lClip.Clip->StepAt(lClip.Step) : nullptr;
        if (lStep == nullptr)
        {
            bResolved = ResolveImageSource(InContext, InRuntime, InBrush.Texture, InBrush.Sheet, InBrush.Frame,
                                           Out, bOutNamed);
        }
        else if (!lClip.Clip->Sheet.IsEmpty())
        {
            // A step names its frame: the clip's sheet says which index it is.
            const SpriteSheet* lSheet = Cast<SpriteSheet>(InContext.Assets->ResolveAsset(lClip.Clip->Sheet.Path.CStr()));
            Int32 lFrame = -1;
            if (lSheet == nullptr)
            {
                lClip.Issue = EUIBrushIssue::SheetMissing;
            }
            else
            {
                lFrame = lSheet->FindFrameIndex(lStep->Frame);
                if (lFrame < 0)
                {
                    lClip.Issue = EUIBrushIssue::FrameUnknown;
                }
            }
            bResolved = ResolveImageSource(InContext, nullptr, {}, lClip.Clip->Sheet, lFrame, Out, bOutNamed);
        }
        else
        {
            bResolved = ResolveImageSource(InContext, nullptr, lStep->Texture, {}, -1, Out, bOutNamed);
        }

        if (OutClip != nullptr)
        {
            *OutClip = lClip;
        }
        return bResolved;
    }

    EUIBrushDraw DrawBrush(const UIBuildContext& InContext, const Bounds2D& InRect, const UIBrush& InBrush,
                           ITexture2D* const InRuntime, const float InSeconds, TDynArray<UIQuad>& OutQuads,
                           UIBrushClip* const OutClip)
    {
        UIResolvedImage lImage;
        bool            bNamed = false;
        if (!ResolveBrush(InContext, InBrush, InRuntime, InSeconds, lImage, bNamed, OutClip) && bNamed)
        {
            return EUIBrushDraw::NotReady;
        }

        if (!BuildImageQuads(InRect, lImage, InBrush.Border, InBrush.BorderScale, InBrush.Tint, OutQuads))
        {
            return EUIBrushDraw::Stretched;
        }
        return EUIBrushDraw::Drawn;
    }

    OpaaxString DescribeBrush(const UIBrush& InBrush, const bool bInRuntime, const UIBrushClip& InClip)
    {
        if (bInRuntime)
        {
            return OpaaxString("runtime");
        }
        if (InClip.Clip != nullptr)
        {
            return OpaaxString::Format("{}#{}", InBrush.Animation.Path.CStr(), InClip.Step);
        }
        if (!InBrush.Sheet.IsEmpty())
        {
            return OpaaxString::Format("{}#{}", InBrush.Sheet.Path.CStr(), InBrush.Frame);
        }
        if (!InBrush.Texture.IsEmpty())
        {
            return InBrush.Texture.Path;
        }
        return OpaaxString("none");
    }

    OpaaxString DescribeBrushIssue(const UIBrush& InBrush, const UIBrushClip& InClip)
    {
        switch (InClip.Issue)
        {
            case EUIBrushIssue::ClipMissing:
                return OpaaxString::Format("clip '{}' did not load as an animation clip: its Sheet or Texture shows instead",
                                           InBrush.Animation.Path.CStr());
            case EUIBrushIssue::SheetMissing:
                return OpaaxString::Format("the sheet of clip '{}' did not load: its default frame shows",
                                           InBrush.Animation.Path.CStr());
            case EUIBrushIssue::FrameUnknown:
            {
                const AnimationStep* lStep = InClip.Clip != nullptr ? InClip.Clip->StepAt(InClip.Step) : nullptr;
                return OpaaxString::Format("step {} of clip '{}' names frame '{}', which its sheet does not have: "
                                           "the sheet's default frame shows",
                                           InClip.Step, InBrush.Animation.Path.CStr(),
                                           lStep != nullptr ? lStep->Frame.CStr() : "");
            }
            case EUIBrushIssue::None:
                break;
        }
        return OpaaxString();
    }
}
