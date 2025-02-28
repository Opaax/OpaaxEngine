#include "SceneBase.h"
#include "../GameEngine.h"

SceneBase::SceneBase(const GameEngine& GameEngine):m_gameEngine{GameEngine}
{
}

SceneBase::~SceneBase() = default;

void Scene_Menu::Render()
{
    m_testFont = m_gameEngine.GetAssetManager()->GetFont("KennyPixel");
    m_text.setFont(m_testFont);
    m_text.setString("Its Menu Scene");

    m_gameEngine.GetWindow().draw(m_text);
}

void Scene_Game::Render()
{
    m_testFont = m_gameEngine.GetAssetManager()->GetFont("KennyPixel");
    m_text.setFont(m_testFont);
    m_text.setString("Its Game Scene");

    m_gameEngine.GetWindow().draw(m_text);
}
