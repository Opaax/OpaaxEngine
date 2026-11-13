#pragma once

#include "Core/OpaaxTypes.h"
#include "RHI/Framebuffer.h"

namespace Opaax
{
    /** What a cached render target is for and how it is made: two equal keys can share one target. */
    struct RenderTargetKey
    {
        /** What it holds in the frame (the scene, a bloom level): two uses alive at once differ here. */
        Uint32 Purpose = 0;

        Uint32         Width         = 1;
        Uint32         Height        = 1;
        ETextureFormat Format        = ETextureFormat::RGBA8;
        bool           bLinearFilter = true;

        bool operator==(const RenderTargetKey& InOther) const noexcept
        {
            return Purpose == InOther.Purpose && Width == InOther.Width && Height == InOther.Height
                && Format == InOther.Format && bLinearFilter == InOther.bLinearFilter;
        }

        /** Same use and same making, the size apart: one can be resized into the other. */
        bool IsSameKind(const RenderTargetKey& InOther) const noexcept
        {
            return Purpose == InOther.Purpose && Format == InOther.Format && bLinearFilter == InOther.bLinearFilter;
        }
    };

    // =============================================================================
    // TRenderTargetCache — the render targets views are drawn through, kept from frame to frame.
    //   Several views of different sizes are drawn in a frame (a game view and a camera preview):
    //   each size keeps its own target, so none is rebuilt while the sizes hold.
    //   - A key seen before gets its target back.
    //   - A new size takes over a target of the same kind that the last two frames did not use
    //     (a view being resized: its old sizes are reused, not piled up), else a new one is made.
    //   - A target unused for MAX_IDLE_FRAMES is dropped.
    //   TTarget is what the owner keeps per target; the cache only makes, resizes and drops it
    //   through the functions given to Acquire, so it is tested without a device.
    // =============================================================================
    template<typename TTarget>
    class TRenderTargetCache
    {
    public:
        /** Frames a target stays without being used before it is dropped. */
        static constexpr Uint32 MAX_IDLE_FRAMES = 120;

        using CreateFunction = TFunction<TUniquePtr<TTarget>(const RenderTargetKey&)>;
        using ResizeFunction = TFunction<void(TTarget&, const RenderTargetKey&)>;

        /**
         * The target for InKey, marked as used this frame.
         * @param InCreate Makes a target for a key
         * @param InResize Gives an existing target of the same kind the key's size
         */
        TTarget& Acquire(const RenderTargetKey& InKey, const CreateFunction& InCreate, const ResizeFunction& InResize)
        {
            for (Entry& lEntry : m_Entries)
            {
                if (lEntry.Key == InKey)
                {
                    lEntry.IdleFrames = 0;
                    return *lEntry.Target;
                }
            }

            // Used this frame (0) or the one before (1): another view may still own it.
            for (Entry& lEntry : m_Entries)
            {
                if (lEntry.IdleFrames >= 2 && lEntry.Key.IsSameKind(InKey))
                {
                    InResize(*lEntry.Target, InKey);
                    lEntry.Key        = InKey;
                    lEntry.IdleFrames = 0;
                    ++m_Resized;
                    return *lEntry.Target;
                }
            }

            m_Entries.push_back(Entry{ InKey, InCreate(InKey), 0 });
            ++m_Created;
            return *m_Entries.back().Target;
        }

        /** Ends the frame: ages every target, drops the ones unused for MAX_IDLE_FRAMES. */
        void EndFrame()
        {
            for (size_t i = m_Entries.size(); i > 0; --i)
            {
                Entry& lEntry = m_Entries[i - 1];

                if (++lEntry.IdleFrames > MAX_IDLE_FRAMES)
                {
                    // The last one takes its place: it was aged already (the loop goes backwards).
                    if (i != m_Entries.size())
                    {
                        lEntry = Move(m_Entries.back());
                    }
                    m_Entries.pop_back();
                    ++m_Dropped;
                }
            }
        }

        /** Drops every target. */
        void Clear() { m_Entries.clear(); }

        Uint32 GetCount()   const noexcept { return static_cast<Uint32>(m_Entries.size()); }

        /** Since the cache was made: targets created, resized into a new size, dropped for being unused. */
        Uint64 GetCreated() const noexcept { return m_Created; }
        Uint64 GetResized() const noexcept { return m_Resized; }
        Uint64 GetDropped() const noexcept { return m_Dropped; }

    private:
        struct Entry
        {
            RenderTargetKey     Key;
            TUniquePtr<TTarget> Target;
            Uint32              IdleFrames = 0;   // 0: used this frame
        };

        TDynArray<Entry> m_Entries;
        Uint64           m_Created = 0;
        Uint64           m_Resized = 0;
        Uint64           m_Dropped = 0;
    };
}
