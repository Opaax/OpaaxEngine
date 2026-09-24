#pragma once

#include "Core/EngineAPI.h"

namespace Opaax
{
    struct OPAAX_API FrameInfo
    {
    public:
        FrameInfo() : 
    		m_LastTime(0),
    		m_DeltaTime(0), 
    		m_FixedDeltaTime(0), 
    		m_AccumulatedDeltaTime(0), 
    		m_AlphaPhysic(0) 
    	{}
    	
        ~FrameInfo() = default;
        
        double m_LastTime;
        double m_DeltaTime;
		double m_FixedDeltaTime;
		double m_AccumulatedDeltaTime;
		double m_AlphaPhysic;
    };
}