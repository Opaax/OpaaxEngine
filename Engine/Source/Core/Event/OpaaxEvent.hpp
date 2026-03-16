#pragma once
#include "OpaaxEventTypes.hpp"

namespace Opaax
{
    /**
     * @class OpaaxEvent
     *
     * Events are stack-allocated value types. They are never heap-allocated by the engine.
     */
    class OPAAX_API OpaaxEvent
    {
        friend class OpaaxEventDispatcher;

        // =============================================================================
        // Interface — implemented by OPAAX_EVENT_CLASS_TYPE / CATEGORY macros below
        // =============================================================================

        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~OpaaxEvent() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        
        // -----------------------------------------------------------------------------
        // Getters
    public:
        virtual EEventType  GetEventType()      const noexcept = 0;
        virtual const char* GetName()           const noexcept = 0;
        virtual Uint32      GetCategoryFlags()  const noexcept = 0;

        FORCEINLINE bool IsInCategory(EEventCategory Category) const noexcept
        {
            return (GetCategoryFlags() & static_cast<Uint32>(Category)) != 0;
        }

        FORCEINLINE bool IsHandled() const noexcept { return bHandled; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        bool bHandled = false;
    };

    // =============================================================================
    // Macro helpers — stamp boilerplate onto concrete event classes
    //
    // OPAAX_EVENT_CLASS_TYPE(T)  — implements GetEventType() and GetName()
    // OPAAX_EVENT_CLASS_CATEGORY(C) — implements GetCategoryFlags()
    //
    // Usage:
    //   class MyEvent : public OpaaxEvent {
    //       OPAAX_EVENT_CLASS_TYPE(EEventType::MyEvent)
    //       OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Input | EEventCategory_Keyboard)
    //   };
    // =============================================================================
#define OPAAX_EVENT_CLASS_TYPE(Type)\
    static  EEventType  GetStaticType() noexcept                { return Type; }\
    virtual EEventType  GetEventType()  const noexcept override { return GetStaticType(); } \
    virtual const char* GetName()       const noexcept override { return #Type; }
 
#define OPAAX_EVENT_CLASS_CATEGORY(Category)\
    virtual Uint32 GetCategoryFlags() const noexcept override { return static_cast<Uint32>(Category); }
}
