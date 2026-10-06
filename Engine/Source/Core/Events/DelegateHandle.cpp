#include "DelegateHandle.h"

namespace Opaax
{
    DelegateHandle DelegateHandle::Generate() noexcept
    {
        // One counter for the whole process. 0 is the invalid handle.
        static TAtomic<Uint64> s_Next{0};
        return DelegateHandle(s_Next.fetch_add(1, std::memory_order_relaxed) + 1);
    }
}
