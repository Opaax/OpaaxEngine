#include "Renderer/Textures/TexturePaint.h"

#include "Core/Maths/Maths.h"
#include "Renderer/CameraView.h"
#include "Renderer/Textures/Texture2D.h"

namespace Opaax
{
    bool IsDrawableStamp(const PaintStamp& InStamp) noexcept
    {
        const float lValues[] = { InStamp.Center.x, InStamp.Center.y, InStamp.Size.x, InStamp.Size.y, InStamp.Rotation,
                                  InStamp.Color.r, InStamp.Color.g, InStamp.Color.b, InStamp.Color.a };
        for (const float lValue : lValues)
        {
            if (!Maths::IsFinite(lValue))
            {
                return false;
            }
        }

        return InStamp.Size.x > 0.f && InStamp.Size.y > 0.f;
    }

    TOptional<TextureRegion> ClipRegion(const TextureRegion& InRegion, const Uint32 InWidth, const Uint32 InHeight) noexcept
    {
        // Widened: X + Width must not wrap around.
        const Uint64 lRight = Maths::Min(static_cast<Uint64>(InRegion.X) + InRegion.Width,  static_cast<Uint64>(InWidth));
        const Uint64 lTop   = Maths::Min(static_cast<Uint64>(InRegion.Y) + InRegion.Height, static_cast<Uint64>(InHeight));

        if (lRight <= InRegion.X || lTop <= InRegion.Y)
        {
            return NullOpt;
        }

        return TextureRegion{ InRegion.X, InRegion.Y, static_cast<Uint32>(lRight - InRegion.X),
                              static_cast<Uint32>(lTop - InRegion.Y) };
    }

    RenderView MakePaintView(const Uint32 InWidth, const Uint32 InHeight, const EBlendMode InBlend)
    {
        CameraView lCamera;
        lCamera.Position  = { 0.5f * static_cast<float>(InWidth), 0.5f * static_cast<float>(InHeight) };
        lCamera.OrthoSize = 0.5f * static_cast<float>(InHeight);

        RenderView lView;
        lView.ViewProjection = MakeViewProjection(lCamera, InWidth, InHeight);
        lView.Viewport       = Viewport{ 0u, 0u, InWidth, InHeight };
        lView.Blend          = InBlend;
        return lView;
    }

    // =============================================================================
    // PaintQueue
    // =============================================================================
    Uint32 PaintQueue::AddStamps(const RuntimeTexture& InTarget, const TDynArray<PaintStamp>& InStamps,
                                 const EPaintMode InMode)
    {
        Uint32 lDropped = 0;
        Uint32 lAdded   = 0;
        PaintWork* lWork = nullptr;

        for (const PaintStamp& lStamp : InStamps)
        {
            if (!IsDrawableStamp(lStamp))
            {
                ++lDropped;
                continue;
            }

            if (lWork == nullptr)
            {
                lWork = &WorkFor(InTarget);

                // Joins the run before when it is stamps of the same mode; a clear or another mode starts one.
                const bool bJoins = !lWork->Ops.empty() && lWork->Ops.back().Kind == EPaintOp::Stamps
                                 && lWork->Ops.back().Mode == InMode;
                if (!bJoins)
                {
                    PaintOp lOp;
                    lOp.Kind       = EPaintOp::Stamps;
                    lOp.Mode       = InMode;
                    lOp.FirstStamp = static_cast<Uint32>(lWork->Stamps.size());
                    lWork->Ops.push_back(lOp);
                }
            }

            lWork->Stamps.push_back(lStamp);
            ++lAdded;
        }

        if (lWork != nullptr)
        {
            lWork->Ops.back().StampCount += lAdded;
        }

        return lDropped;
    }

    void PaintQueue::AddClear(const RuntimeTexture& InTarget, const TextureRegion& InRegion,
                              const Vector4F& InPremultipliedColor)
    {
        PaintOp lOp;
        lOp.Kind   = EPaintOp::Clear;
        lOp.Region = InRegion;
        lOp.Color  = InPremultipliedColor;
        WorkFor(InTarget).Ops.push_back(lOp);
    }

    void PaintQueue::Reset()
    {
        m_Work.clear();
    }

    Uint32 PaintQueue::GetStampCount() const noexcept
    {
        Uint32 lCount = 0;
        for (const PaintWork& lWork : m_Work)
        {
            lCount += static_cast<Uint32>(lWork.Stamps.size());
        }
        return lCount;
    }

    PaintWork& PaintQueue::WorkFor(const RuntimeTexture& InTarget)
    {
        // The same texture whichever handle names it.
        const Texture2D* lTexture = InTarget.GetTexture();
        for (PaintWork& lWork : m_Work)
        {
            if (lWork.Target.GetTexture() == lTexture)
            {
                return lWork;
            }
        }

        PaintWork& lNew = m_Work.emplace_back();
        lNew.Target     = InTarget;
        return lNew;
    }
}
