#include "DelegateHandle.h"

namespace Opaax
{
    DelegateHandle DelegateHandle::Generate() noexcept
    {
        // Out-of-line + a single .cpp-local static => ONE counter shared across the
        // DLL/exe boundary (exe callers invoke this exported function, not their own
        // copy). Handles start at 1; 0 stays reserved for the invalid handle.
        static TAtomic<Uint64> s_Next{0};
        return DelegateHandle(s_Next.fetch_add(1, std::memory_order_relaxed) + 1);
    }
}
