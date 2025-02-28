#include "Scene_Game.h"
#include "../GameEngine.h"

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