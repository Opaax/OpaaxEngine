#pragma once
#include "Core/OPLogMacro.h"

namespace OPAAX
{
    class OpaaxWindow;

    /**
     * @class IOpaaxRenderer
     * @brief Abstract interface class for rendering systems.
     *
     * The IRenderer class provides an interface for graphics rendering systems
     * with essential methods for initializing, starting, and completing rendering
     * operations, as well as shutting down the renderer.
     */
    class IOpaaxRenderer
    {
        //-----------------------------------------------------------------
        // Members
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        OpaaxWindow* m_window;
        bool bIsInitialized;

        //-----------------------------------------------------------------
        // CTOR/DTOR
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        IOpaaxRenderer(OpaaxWindow* Window): m_window(Window), bIsInitialized(false) {}

        virtual ~IOpaaxRenderer()
        {
            if (bIsInitialized)
            {
                OPAAX_WARNING("Renderer is still initialized! Call Shutdown before destroying the Renderer")
            }
        }

        //Cannot copy the renderer
        IOpaaxRenderer(const IOpaaxRenderer&) = delete;
        IOpaaxRenderer& operator=(const IOpaaxRenderer&) = delete;

        //-----------------------------------------------------------------
        // Functions
        //-----------------------------------------------------------------
        /*---------------------------- PROTECTED ----------------------------*/
    protected:
        FORCEINLINE OpaaxWindow* GetOpaaxWindow() const { return m_window; }
        void SetIsInitialized(bool NewIsInitialized) { bIsInitialized = NewIsInitialized; }
        
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        virtual bool Initialize() = 0;
        virtual void Resize() = 0;
        virtual void RenderFrame() = 0;
        virtual void Shutdown() = 0;
        //virtual void DrawObject(const Object& obj) = 0;

        FORCEINLINE bool IsInitialized() const { return bIsInitialized; }
    };
}