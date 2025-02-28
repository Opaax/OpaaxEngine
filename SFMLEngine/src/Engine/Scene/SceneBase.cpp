#include "SceneBase.h"
#include "../GameEngine.h"

SceneBase::SceneBase(const GameEngine& GameEngine):m_gameEngine{GameEngine}
{
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

void Scene_Menu::Render()
{
    m_testFont = m_gameEngine.GetAssetManager()->GetFont("KennyPixel");
    m_text.setFont(m_testFont);
    m_text.setString("Its Menu Scene");

    m_gameEngine.GetWindow().draw(m_text);
}

void Scene_Menu::OnKeyboardPressed(sf::Keyboard::Key Key)
{
    if(Key == sf::Keyboard::Key::W)
    {
        m_text.setPosition({500,500});
    }
}

void Scene_Game::Render()
{
    m_testFont = m_gameEngine.GetAssetManager()->GetFont("KennyPixel");
    m_text.setFont(m_testFont);
    m_text.setString("Its Game Scene");

    m_gameEngine.GetWindow().draw(m_text);
}

void Scene_Game::OnKeyboardPressed(sf::Keyboard::Key Key)
{
    if(Key == sf::Keyboard::Key::W)
    {
        m_text.setPosition({500,500});
    }
}
