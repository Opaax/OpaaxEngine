#pragma once
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"

#include "Application/Services/IAppService.h"

namespace Opaax
{
    // =============================================================================
    // IPlatform — OS abstraction (cores, time, env...).
    // =============================================================================
    class OPAAX_API IPlatform : public IAppService
    {
    public:
        OPAAX_SERVICE_TYPE(IPlatform)

        //----- contract -------------------------------------------------------
        virtual Uint32 GetLogicalCoreCount() const = 0;
        virtual double GetTimeSeconds()      const = 0;
        
        /**
         * Absolute path to the running executable (OS call — robust, unlike argv[0]).
         * IPaths derives its base from this.
         * @return 
         */
        virtual OpaaxString GetExecutablePath() const = 0;

        /**
         * 
         * @return Windows Linux Max
         */
        virtual OpaaxString GetPlatformName() const = 0;

        //----- null object ----------------------------------------------------
        static IPlatform& Null();
    };
}
