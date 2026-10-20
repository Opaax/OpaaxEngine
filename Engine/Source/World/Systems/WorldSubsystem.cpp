#include "World/Systems/WorldSubsystem.h"

#include "World/Systems/WorldContext.h"

namespace Opaax
{
    WorldContext& WorldSubsystem::GetContext() const
    {
        OPAAX_ASSERT(m_Context != nullptr);
        return *m_Context;
    }

    World& WorldSubsystem::GetWorld() const
    {
        return GetContext().OwningWorld;
    }
}
