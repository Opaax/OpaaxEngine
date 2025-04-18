#pragma once
#include "OpaaxWindowSpecs.h"
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    /**
     * @class OpaaxWindow
     * @brief An abstract interface representing a graphical window.
     *
     * The OpaaxWindow class provides an interface for managing and interacting with a graphical window.
     * Derived classes must implement the pure virtual functions to define specific behaviors
     * for updating the window, retrieving its dimensions, toggling vertical synchronization (VSync),
     * accessing window-specific native handles, and other platform-specific features.
     */
    class OPAAX_API OpaaxWindow
    {
    public:
        virtual ~OpaaxWindow() {}

        virtual void OnUpdate() = 0;
        virtual bool ShouldClose() = 0;
        virtual void Init() = 0;
        virtual void Shutdown() = 0;

        virtual UInt32 GetWidth() const = 0;
        virtual UInt32 GetHeight() const = 0;

        // Window attributes
        //virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
        virtual void SetVSync(bool Enabled) = 0;
        virtual bool IsVSync() const = 0;

        virtual void* GetNativeWindow() const = 0;

        static OpaaxWindow* Create(const OpaaxWindowSpecs& Specs = OpaaxWindowSpecs());
    };
}
