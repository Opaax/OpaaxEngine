#include "AssetRefBlock.hpp"

namespace Opaax
{
    void AssetRefBlock::Release() noexcept
    {
        // fetch_sub returns the OLD value — only the thread that observes "1" owns the delete.
        if (RefCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            delete this;
        }
    }
}
