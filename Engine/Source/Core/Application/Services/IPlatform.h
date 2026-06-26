#pragma once
#include "IAppService.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"

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

        //----- null object ----------------------------------------------------
        static IPlatform& Null();
    };
}
