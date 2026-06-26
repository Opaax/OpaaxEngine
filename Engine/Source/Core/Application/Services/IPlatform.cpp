#include "IPlatform.h"

namespace Opaax
{
    namespace
    {
        class NullPlatform final : public IPlatform
        {
        public:
            bool        IsNull()              const noexcept override { return true; }
            Uint32      GetLogicalCoreCount() const override { return 1; }
            double      GetTimeSeconds()      const override { return 0.0; }
            OpaaxString GetExecutablePath()   const override { return OpaaxString(); }
        };
    }

    ServiceTypeID IPlatform::StaticTypeID() noexcept { 
		static const int s_Tag = 0; return reinterpret_cast<ServiceTypeID>(&s_Tag); 
	}

    IPlatform& IPlatform::Null() 
    { 
		static NullPlatform s_Null; 
        return s_Null; 
	}   // one definition → DLL-safe
}
