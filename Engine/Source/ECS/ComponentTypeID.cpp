#include "ComponentTypeID.h"

namespace Opaax::ECS::Internal
{
    Uint32 AllocateTypeID() noexcept
    {
        // NOTE: Starts at 1 — 0 is reserved as "invalid/unregistered".
        static Atomic<Uint32> s_Counter { 1 };
        return s_Counter.fetch_add(1, std::memory_order_relaxed);
    }

} // namespace Opaax::ECS::Internal