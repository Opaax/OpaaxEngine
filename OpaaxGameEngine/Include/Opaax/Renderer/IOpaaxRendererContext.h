#pragma once
#include <SDL3/SDL_video.h>

#include "Opaax/OpaaxNonCopyable.h"
#include "Opaax/Log/OPLogMacro.h"

namespace OPAAX
{
    class OpaaxWindow;

    /**
     * @class IOpaaxRendererContext
     * @brief Abstract base class for rendering context in the Opaax Engine.
     *
     * IOpaaxRendererContext defines the interface and common functionality for a rendering context.
     * This class provides pure virtual for initializing, rendering frames, resizing, and shutting
     * down a renderer. Additionally, it manages the underlying window and initialization state
     * required for rendering operations.
     *
     * Derived classes should implement the pure virtual methods to provide specific rendering
     * functionality, such as Vulkan or OpenGL implementations.
     */
    class OPAAX_API IOpaaxRendererContext : public OpaaxNonCopyable
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
        IOpaaxRendererContext(): m_window(nullptr), bIsInitialized(false) {}

        virtual ~IOpaaxRendererContext()
        {
            if (bIsInitialized)
            {
                OPAAX_WARNING("Renderer is still initialized! Call Shutdown before destroying the Renderer")
            }
        }

        //-----------------------------------------------------------------
        // Functions
        //-----------------------------------------------------------------
        /*---------------------------- PROTECTED ----------------------------*/
        /*---------------------------- Getter - Setter ----------------------------*/
    protected:
        FORCEINLINE OPAAX::OpaaxWindow* GetOpaaxWindow() const { return m_window; }
        FORCEINLINE void SetOpaaxWindow(OPAAX::OpaaxWindow* Window) { m_window = Window; }
        void SetIsInitialized(bool NewIsInitialized) { bIsInitialized = NewIsInitialized; }

        /*---------------------------- PUBLIC ----------------------------*/
    public:
        virtual bool            Initialize(OpaaxWindow& OpaaxWindow)           = 0;
        virtual void            Resize()                                       = 0;
        virtual void            DrawImgui()                                    = 0;
        virtual void            RenderFrame()                                  = 0;
        virtual void            Shutdown()                                     = 0;
        virtual SDL_WindowFlags GetWindowFlags()                               = 0;
        //virtual void DrawObject(const Object& obj) = 0;

        /*---------------------------- Getter - Setter ----------------------------*/
        FORCEINLINE bool IsInitialized() const { return bIsInitialized; }
    };
}
