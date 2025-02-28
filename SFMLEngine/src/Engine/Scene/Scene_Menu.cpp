#include "Scene_Menu.h"
#include "../GameEngine.h"

void Scene_Menu::Render()
{
    m_testFont = m_gameEngine.GetAssetManager()->GetFont("KennyPixel");
    m_text.setFont(m_testFont);
    m_text.setString("Its Menu Scene");

    m_gameEngine.GetWindow().draw(m_text);
}

void Scene_Menu::OnKeyboardPressed(sf::Keyboard::Key Key)
{
    SceneBase::OnKeyboardPressed(Key);
    
    if(Key == sf::Keyboard::Key::W)
    {
        m_text.setPosition({500,500});
    }
}