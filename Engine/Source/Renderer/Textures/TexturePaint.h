#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Renderer/RenderView.h"
#include "Renderer/Textures/PaintStamp.h"
#include "Renderer/Textures/RuntimeTexture.h"
#include "RHI/Pipeline.h"   // EBlendMode
#include "RHI/TextureRegion.h"

namespace Opaax
{
    // =============================================================================
    // Painting into a texture — what a game asks (stamps, clears) and the renderer's queue of it.
    //   A paintable RuntimeTexture is drawn into on the GPU before the frame's views; the CPU side
    //   here is plain data and pure functions.
    // =============================================================================

    /** False for a stamp that draws nothing or cannot be placed: a size not above 0, a value not a number. */
    bool IsDrawableStamp(const PaintStamp& InStamp) noexcept;

    /** InRegion cut to a InWidth x InHeight texture; nothing when none of it is inside. */
    TOptional<TextureRegion> ClipRegion(const TextureRegion& InRegion, Uint32 InWidth, Uint32 InHeight) noexcept;

    /**
     * Stamps drawn in one pass at most. Each gets its own order in layer, so the renderer's sort keeps
     * the order they were given in (a tie would group them by texture); a longer run takes more passes.
     */
    inline constexpr Uint32 MAX_STAMPS_PER_PASS = 65536u;

    /** The order in layer of the InIndexInPass-th stamp of a pass (below MAX_STAMPS_PER_PASS): rising with it. */
    constexpr Int16 PaintOrderInLayer(const Uint32 InIndexInPass) noexcept
    {
        return static_cast<Int16>(static_cast<Int32>(InIndexInPass % MAX_STAMPS_PER_PASS) - 32768);
    }

    /**
     * The view of a pass painting a InWidth x InHeight texture: one unit per texel, (0, 0) its
     * bottom-left corner, unlit (the colours are written as given), blended with InBlend.
     */
    RenderView MakePaintView(Uint32 InWidth, Uint32 InHeight, EBlendMode InBlend);

    // =============================================================================
    // PaintQueue — the paint asked for since the last frame, per texture.
    // =============================================================================

    /** What one step of a texture's paint does. */
    enum class EPaintOp : Uint8
    {
        Stamps,
        Clear
    };

    /** One step: a run of stamps of one mode, or a clear. */
    struct PaintOp
    {
        EPaintOp      Kind       = EPaintOp::Stamps;
        EPaintMode    Mode       = EPaintMode::Over;   // stamps
        Uint32        FirstStamp = 0;                  // stamps, into PaintWork::Stamps
        Uint32        StampCount = 0;                  // stamps
        TextureRegion Region;                          // clear, inside the texture
        Vector4F      Color      = { 0.f, 0.f, 0.f, 0.f };   // clear, premultiplied
    };

    /** A texture's paint for one frame, its steps in the order they were asked for. */
    struct PaintWork
    {
        /** Holds the texture until it is painted. */
        RuntimeTexture        Target;
        TDynArray<PaintStamp> Stamps;
        TDynArray<PaintOp>    Ops;
    };

    /**
     * Paint asked for on the main thread, drawn when the frame starts. One entry per texture, in the
     * order each was first asked for (its mipmaps are remade once); in an entry, the steps in call
     * order, a call's stamps added to the run before when it is stamps of the same mode.
     */
    class PaintQueue
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Adds InStamps for InTarget, the ones not drawable dropped.
         * @return How many were dropped
         */
        Uint32 AddStamps(const RuntimeTexture& InTarget, const TDynArray<PaintStamp>& InStamps, EPaintMode InMode);

        /** Adds a clear of InRegion (inside the texture) to InPremultipliedColor. */
        void   AddClear(const RuntimeTexture& InTarget, const TextureRegion& InRegion, const Vector4F& InPremultipliedColor);

        /** Forgets everything (and lets go of the textures it held). */
        void   Reset();

        // =============================================================================
        // Get
        // =============================================================================
    public:
        const TDynArray<PaintWork>& GetWork() const noexcept { return m_Work; }
        bool   IsEmpty() const noexcept { return m_Work.empty(); }

        /** The stamps queued, every texture's. */
        Uint32 GetStampCount() const noexcept;

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** InTarget's entry, made at the end when it has none. */
        PaintWork& WorkFor(const RuntimeTexture& InTarget);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<PaintWork> m_Work;
    };
}
