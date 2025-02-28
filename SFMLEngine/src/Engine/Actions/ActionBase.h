#pragma once
#include "../EngineType.hpp"

enum struct EActionType
{
    EAT_Start = 0,
    EAT_End,
};
class ActionBase
{
    STDString m_actionName{"None"};
    EActionType m_actionType{EActionType::EAT_Start};
    
public:
    ActionBase(const STDString& InActionName, EActionType InActionType):m_actionName(InActionName), m_actionType(InActionType){}
    virtual ~ActionBase() = default;

    EActionType GetActionType() const {return m_actionType;}
    const STDString& GetActionName() const {return m_actionName;}
};
