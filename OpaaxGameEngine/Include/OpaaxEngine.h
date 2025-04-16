#pragma once
#include "OpaaxTypes.h"

namespace OPAAX
{
    class OpaaxWindow;
    class IOpaaxRenderer;
    
    class Engine
    {
        //-----------------------------------------------------------------
        // Members
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        TUniquePtr<OpaaxWindow>     m_pWindow   = nullptr;
        TUniquePtr<IOpaaxRenderer>  m_pRenderer = nullptr;

    private:
        ~Engine();

        //-----------------------------------------------------------------
        // Functions
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        static Engine& Get();

        void Init();
        void Run();
        void ShutDown();
    };
}
