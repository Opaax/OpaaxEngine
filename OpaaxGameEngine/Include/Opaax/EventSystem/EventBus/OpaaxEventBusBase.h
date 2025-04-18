#pragma once
#include "Opaax/OpaaxCoreMacros.h"

namespace OPAAX
{
    /**
     * @class OpaaxEventBusBase
     * @brief Serves as the base class for the event bus system in the Opaax Engine.
     *
     * OpaaxEventBusBase provides a polymorphic base to allow derived event types
     * to interact with the Opaax event bus. It establishes the foundation for
     * implementing event handler and dispatch mechanisms within the system.
     *
     * This base class is designed to be extended by specific event implementations,
     * enabling the event bus to manage a collection of strongly-typed events. The
     * virtual destructor ensures proper cleanup of derived class resources.
     */
    class OPAAX_API OpaaxEventBusBase
    {
    public:
        OpaaxEventBusBase() = default;
        virtual ~OpaaxEventBusBase() = default;
    };
}
