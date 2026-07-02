#pragma once

#include "IAppService.h"

namespace Opaax
{
    // =============================================================================
    // IEngine
    // =============================================================================
    class OPAAX_API IEngine : public IAppService
    {
    public:
        OPAAX_SERVICE_TYPE(IEngine)
        
        static IEngine& Null();
    };
}