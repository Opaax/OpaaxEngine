#pragma once
#include <mutex>
#include "OpaaxEventBusBase.h"
#include "OpaaxEventBusHandle.h"
#include "Opaax/OpaaxNonCopyableAndMovable.h"
#include "Opaax/OpaaxTypes.h"

namespace OPAAX
{
    /**
     * For the moment this event bus can be unsafe. for example:
     * DECLARE_EVENT_BUS(PlayerEvent)
     *
     * class TestEvent
     * {
     * public:
     * void OnPlayerEvent(const PlayerEvent& Event) { OPAAX_DEBUG("OnPlayerEvent") }
     * OPAAX::OpaaxEventBusHandle m_playerEventHandle{}; };
     *
     * TestEvent* lT = new TestEvent{};
     * lT->m_playerEventHandle = OPEventBus.Bind<PlayerEvent>([lT](const PlayerEvent& Event){lT->OnPlayerEvent(Event);});
     * delete lT;
     * OPEventBus.Send(PlayerEvent{});
     *
     * This code above might work. if the lT ptr is not relocated the event will be called event lT is deleted.
     * This allows crash easily and very hard debugging session. need to improve that in the future.
     * 
     */
    
    /**
     * How to use the bus event.
     *
     *  - Simple bus event
     *  DECLARE_EVENT_BUS(PlayerEvent)
     *  - Event bus with 2 args. Macro can go up to 8 args
     *  DECLARE_EVENT_BUS_ARGS_2(PlayerDeathEvent, Int32, PlayerID, OSTDString, DeadReason)
     * class MyClass{
     *  - Callbacks and handles
     *  void OnPlayerEvent(const PlayerEvent& Event) { OPAAX_DEBUG("OnPlayerEvent") }
     *  void OnPlayerDead(const PlayerDeathEvent& DeathEvent) { OPAAX_DEBUG("DeathEvent %1% -- %2%", %DeathEvent.PlayerID %DeathEvent.DeadReason) }
     *  OPAAX::OpaaxEventBusHandle m_playerEventHandle{};
     *  OPAAX::OpaaxEventBusHandle m_playerDeathEventHandle{};
     *  };
     *
     *  - Lets imagine this happen on some function
     *  - Bindinds
     *  OPEventBus macro for OPAAX::OpaaxEventBus::Get()
     *  m_playerEventHandle = OPEventBus.Bind<PlayerEvent>([this](const PlayerEvent& Event){OnPlayerEvent(Event);});
     *  m_playerDeathEventHandle = OPEventBus.Bind<PlayerDeathEvent>([this](const PlayerDeathEvent& Event){OnPlayerDead(Event);});
     *
     *  - The bus sending an event
     *  OPEventBus.Send(PlayerEvent{});
     *  OPEventBus.Send(PlayerDeathEvent{0, "Out Of The World"});
     *  - Unbinding
     *  OPEventBus.UnBind(m_playerEventHandle);
     *  OPEventBus.UnBind(m_playerDeathEventHandle);    
     */
    
    /**
     * @class OpaaxEventBus
     * 
     * Represents a globally accessible event bus designed to manage event listeners
     * and dispatch events in a thread-safe manner.
     * The OpaaxEventBus class ensures that
     * event listeners can be dynamically registered, unregistered, or triggered for a
     * specified event type.
     *
     * This class inherits from OpaaxNonCopyableAndMovable, enforcing a non-copyable
     * and non-movable nature to prevent accidental duplication or transfer of the
     * event bus instance.
     */
    class OPAAX_API OpaaxEventBus : public OpaaxNonCopyableAndMovable
    {
        //------------------------------------------------
        // Members
        //------------------------------------------------
        //-------------------- PRIVATE -----------------------//
    private:
        // Warning C4251 fix: use forward-declared classes and export the container explicitly if needed.
        // Here we suppress the warning via pragma or document its cause.
#pragma warning(push)
#pragma warning(disable: 4251)
        std::unordered_map<OPSTDTypeID, std::vector<OpaaxEventBusHandle>> m_listeners;
        std::mutex m_mutex;
#pragma warning(pop)

        //------------------------------------------------
        // CTOR / DTOR
        //------------------------------------------------
        //-------------------- PUBLIC -----------------------//
    public:
        OpaaxEventBus() = default;
        ~OpaaxEventBus() override;

        //------------------------------------------------
        // Function
        //------------------------------------------------
        //-------------------- PUBLIC -----------------------//
    public:
        /**
         * Binds a callback function to the event bus for a specific event type. The registered callback
         * will be invoked when an event of the specified type is dispatched through the event bus.
         *
         * @param Callback The callback function to bind for the specified event type.
         * @return A reference to the associated OpaaxEventBusHandle that represents the binding.
         */
        template<typename EventType>
        OpaaxEventBusHandle& Bind(OPEventCallback<EventType> Callback);

        /**
         * Unbinds an event handler from the event bus, effectively removing the listener
         * corresponding to the specified event bus handle.
         *
         * @param Handle The handle representing the event listener to be unbound.
         * @return Returns true if the event handler was successfully unbound; otherwise, false.
         */
        bool UnBind(const OpaaxEventBusHandle& Handle);

        /**
         * Unbinds all event listeners from the event bus, removing all associations between
         * events and their respective handlers. This operation effectively clears the entire
         * event bus of its registered callbacks.
         */
        void UnBindAll();

        /**
         * Dispatches the given event to all registered listeners subscribed to the event bus
         * for the specified event type. Each listener receives the event and executes
         * its associated callback function.
         *
         * @param Event The event instance to be sent to the registered listeners.
         */
        template<typename EventType>
        void Send(const EventType& Event);

        /**
         * Retrieves the singleton instance of the OpaaxEventBus. This provides
         * global access to the single instance of the event bus used for managing
         * event listeners and dispatching events.
         *
         * @return A reference to the singleton instance of OpaaxEventBus.
         */
        static OpaaxEventBus& Get()
        {
            static OpaaxEventBus s_instance;
            return s_instance;
        }
    };

    template <typename EventType>
    OpaaxEventBusHandle& OpaaxEventBus::Bind(OPEventCallback<EventType> Callback)
    {
        std::unique_lock<std::mutex> lLock(m_mutex);
            
        OPSTDTypeID lTypeID = typeid(EventType);
        auto& lSubscribers = m_listeners[lTypeID];
            
        lSubscribers.push_back(OpaaxEventBusHandle([Callback](const OpaaxEventBusBase& Event)
        {
            Callback(static_cast<const EventType&>(Event));
        }, lTypeID));

        return lSubscribers.back();
    }

    template <typename EventType>
    void OpaaxEventBus::Send(const EventType& Event)
    {
        std::unique_lock<std::mutex> lLock(m_mutex);
            
        auto lIt = m_listeners.find(typeid(EventType));

        if(lIt != m_listeners.end())
        {
            for (auto lHandle : lIt->second)
            {
                if (lIt != m_listeners.end())
                {
                    for (const auto& lListener : lIt->second)
                    {
                        lListener.GetEventHandlerFunc()(Event);
                    }
                }
            }
        }
    }
}
