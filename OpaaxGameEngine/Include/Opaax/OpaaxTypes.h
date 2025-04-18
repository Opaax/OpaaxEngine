#pragma once
#include "OpaaxStdTypes.h"
#include "EventSystem/EventBus/OpaaxEventBusBase.h"

//------------------------------------------------
// Events
//------------------------------------------------
template<typename EventType>
using OPEventCallback   = OPSTDFunc<void(const EventType&)>;
using OPEventBusFunc    = OPSTDFunc<void(const OPAAX::OpaaxEventBusBase&)>;
using EventBusHandlerID = UInt64;
