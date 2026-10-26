#include "Engine/GameInstance/GameInstanceSubsystem.h"

namespace Opaax
{
    GameInstanceContext& GameInstanceSubsystem::GetContext() const
    {
        OPAAX_ASSERT(m_Context != nullptr);
        return *m_Context;
    }
}
