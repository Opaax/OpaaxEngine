#pragma once
#include "OpaaxEventBusBase.h"
#include "OpaaxEventBus.h"

#define OPEventBus OPAAX::OpaaxEventBus::Get()

#define DECLARE_EVENT_BUS(EventName)\
    struct EventName : public OPAAX::OpaaxEventBusBase\
    {\
    public:\
        EventName() {}\
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

#define DECLARE_EVENT_BUS_ARG_1(EventName, T1, V1)\
    class EventName : public OPAAX::OpaaxEventBusBase\
    {\
    public:\
        T1 V1;\
        EventName(T1 In##V1) : V1(In##V1) {}\
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

#define DECLARE_EVENT_BUS_ARGS_2(EventName, T1, V1, T2, V2)\
    class EventName : public OPAAX::OpaaxEventBusBase\
    {\
    public:\
        T1 V1;\
        T2 V2;\
        EventName(T1 In##V1, T2 In##V2)\
            : V1(In##V1), V2(In##V2) {}\
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

#define DECLARE_EVENT_BUS_ARGS_3(EventName, T1, V1, T2, V2, T3, V3) \
    class EventName : public OPAAX::OpaaxEventBusBase\
    {\
    public:\
        T1 V1;\
        T2 V2;\
        T3 V3;\
        EventName(T1 In##V1, T2 In##V2, T3 In##V3)\
            : V1(In##V1), V2(In##V2), V3(In##V3) {}\
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

#define DECLARE_EVENT_BUS_ARGS_4(EventName, T1, V1, T2, V2, T3, V3, T4, V4) \
    class EventName : public OPAAX::OpaaxEventBusBase\
    {\
    public:\
        T1 V1;\
        T2 V2;\
        T3 V3;\
        T4 V4;\
        EventName(T1 In##V1, T2 In##V2, T3 In##V3, T4 In##V4)\
            : V1(In##V1), V2(In##V2), V3(In##V3), V4(In##V4) {}\
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

#define DECLARE_EVENT_BUS_ARGS_5(EventName, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5) \
    class EventName : public OPAAX::OpaaxEventBusBase\
    {\
    public:\
        T1 V1;\
        T2 V2;\
        T3 V3;\
        T4 V4;\
        T5 V5;\
        EventName(T1 In##V1, T2 In##V2, T3 In##V3, T4 In##V4, T5 In##V5)\
            : V1(In##V1), V2(In##V2), V3(In##V3), V4(In##V4), V5(In##V5) {} \
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

#define DECLARE_EVENT_BUS_ARGS_6(EventName, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6) \
    class EventName : public OPAAX::OpaaxEventBusBase                                  \
    {                                                                              \
    public:                                                                        \
        T1 V1;                                                                     \
        T2 V2;                                                                     \
        T3 V3;                                                                     \
        T4 V4;                                                                     \
        T5 V5;                                                                     \
        T6 V6;                                                                     \
        EventName(T1 In##V1, T2 In##V2, T3 In##V3, T4 In##V4, T5 In##V5, T6 In##V6) \
            : V1(In##V1), V2(In##V2), V3(In##V3), V4(In##V4), V5(In##V5), V6(In##V6) {} \
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

#define DECLARE_EVENT_BUS_ARGS_7(EventName, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7) \
    class EventName : public OPAAX::OpaaxEventBusBase                                        \
    {                                                                                      \
    public:                                                                                \
        T1 V1;                                                                             \
        T2 V2;                                                                             \
        T3 V3;                                                                             \
        T4 V4;                                                                             \
        T5 V5;                                                                             \
        T6 V6;                                                                             \
        T7 V7;                                                                             \
        EventName(T1 In##V1, T2 In##V2, T3 In##V3, T4 In##V4, T5 In##V5, T6 In##V6, T7 In##V7) \
            : V1(In##V1), V2(In##V2), V3(In##V3), V4(In##V4),                          \
              V5(In##V5), V6(In##V6), V7(In##V7) {}                                     \
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

#define DECLARE_EVENT_BUS_ARGS_8(EventName, T1, V1, T2, V2, T3, V3, T4, V4, T5, V5, T6, V6, T7, V7, T8, V8)\
    class EventName : public OPAAX::OpaaxEventBusBase\
    {\
    public:\
        T1 V1; T2 V2; T3 V3; T4 V4;\
        T5 V5; T6 V6; T7 V7; T8 V8;\
        EventName(T1 In##V1, T2 In##V2, T3 In##V3, T4 In##V4,\
                   T5 In##V5, T6 In##V6, T7 In##V7, T8 In##V8)\
            : V1(In##V1), V2(In##V2), V3(In##V3), V4(In##V4),\
              V5(In##V5), V6(In##V6), V7(In##V7), V8(In##V8) {}\
        static const char* GetName() { return OP_TO_STRING(EventName); }\
    };

