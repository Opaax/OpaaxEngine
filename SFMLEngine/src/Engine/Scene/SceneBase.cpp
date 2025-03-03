#include "SceneBase.h"
#include "../GameEngine.h"

void SceneBase::Init()
{
    RegisterKeyAction(sf::Keyboard::Key::Escape, "QUIT");
    RegisterActions();
    OnInit();
    bIsInit = true;
}

void SceneBase::OnKeyboardPressed(sf::Keyboard::Key Key)
{
    if(m_keyActions.find(Key) != m_keyActions.end())
    {
        EActionType lType = EActionType::EAT_Start;
        ActionBase lAction(m_keyActions.at(Key), lType);
        DoAction(lAction);
    }
}

void SceneBase::RegisterKeyAction(sf::Keyboard::Key Key, STDString ActionName)
{
    m_keyActions[Key] = ActionName;
}

SceneBase::SceneBase(const GameEngine& GameEngine):m_gameEngine{GameEngine}
{
    m_entityMgr = MakeUnique<EntityManager>();
}

SceneBase::~SceneBase() = default;

void SceneBase::ProcessKeyboardInput(sf::Keyboard::Key Key, EInputActionType ActionType)
{
    switch (ActionType)
    {
    case EInputActionType::Pressed:
        OnKeyboardPressed(Key);
        break;
    case EInputActionType::Released:
        OnKeyboardReleased(Key);
        break;
    }
}

void SceneBase::ProcessMouseInput(sf::Mouse::Button Button, EInputActionType ActionType)
{
    switch (ActionType)
    {
    case EInputActionType::Pressed:
        OnMousePressed(Button);
        break;
    case EInputActionType::Released:
        OnMouseReleased(Button);
        break;
    }
}

void SceneBase::DoAction(const ActionBase& Action)
{
    switch (Action.GetActionType())
    {
    case EActionType::EAT_Start:
        if(Action.GetActionName() == "QUIT")
        {
            m_gameEngine.Quit();
        }
        break;
    case EActionType::EAT_End:
        break;
    default: ;
    }
}
