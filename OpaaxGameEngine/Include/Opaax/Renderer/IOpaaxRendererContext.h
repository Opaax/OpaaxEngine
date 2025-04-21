#pragma once
#include <SDL3/SDL_video.h>
#include "Opaax/Log/OPLogMacro.h"

namespace OPAAX
{
    class OpaaxWindow;

    class OPAAX_API IOpaaxRendererContext
    {
        //-----------------------------------------------------------------
        // Members
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        OPAAX::OpaaxWindow* m_window;
        bool bIsInitialized;

        //-----------------------------------------------------------------
        // CTOR/DTOR
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        IOpaaxRendererContext(OPAAX::OpaaxWindow* Window): m_window(Window), bIsInitialized(false) {}

        virtual ~IOpaaxRendererContext()
        {
            if (bIsInitialized)
            {
                OPAAX_WARNING("Renderer is still initialized! Call Shutdown before destroying the Renderer")
            }
        }

        //Cannot copy the renderer
        IOpaaxRendererContext(const IOpaaxRendererContext&) = delete;
        IOpaaxRendererContext& operator=(const IOpaaxRendererContext&) = delete;

        //-----------------------------------------------------------------
        // Functions
        //-----------------------------------------------------------------
        /*---------------------------- PROTECTED ----------------------------*/
    protected:
        FORCEINLINE OPAAX::OpaaxWindow* GetOpaaxWindow() const { return m_window; }
        void SetIsInitialized(bool NewIsInitialized) { bIsInitialized = NewIsInitialized; }

        /*---------------------------- PUBLIC ----------------------------*/
    public:
        virtual bool Initialize() = 0;
        virtual void Resize() = 0;
        virtual void RenderFrame() = 0;
        virtual void Shutdown() = 0;
        virtual SDL_WindowFlags GetWindowFlags() = 0;
        //virtual void DrawObject(const Object& obj) = 0;

        FORCEINLINE bool IsInitialized() const { return bIsInitialized; }
    };
}
