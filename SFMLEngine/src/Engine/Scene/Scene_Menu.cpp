#include "Scene_Menu.h"
#include "../GameEngine.h"

void Scene_Menu::Update()
{
    if(m_entityMgr)
    {
        m_entityMgr->Update();

        for (auto lEntity : m_entityMgr->GetEntities())
        {
            if(lEntity->HasComponent<CAnimation>())
            {
                lEntity->GetComponent<CAnimation>().Update();
            }
        }
    }
}

void Scene_Menu::Render()
{
    if(!bIsInit || m_entityMgr->GetEntities().empty())
    {
        return;
    }
    
    for (const TSharedPtr<Entity> lEntity : m_entityMgr->GetEntities())
    {
        if(lEntity->HasComponent<CRender>())
        {
            const CTransform& lTrans    = lEntity->GetComponent<CTransform>();
            sf::Vector2f lWindowPos     = lTrans.GetPosition().GetSFMLVec2F();
            CRender& lRender  = lEntity->GetComponent<CRender>();
            lRender.GetSprite().setPosition(lWindowPos);
            m_gameEngine.GetWindow().draw(lRender.GetSprite());
        }
    }

    
    m_testFont = m_gameEngine.GetAssetManager()->GetFont("KennyPixel");
    m_text.setFont(m_testFont);
    m_text.setString("Its Menu Scene");

    sf::CircleShape lShape{16, 32};
    sf::Texture& lTexture = m_gameEngine.GetAssetManager()->GetTexture("T_Default_6464");
    sf::Sprite lSprite{lTexture};
    sf::Sprite lSprite2{lTexture};
    lSprite2.setColor(sf::Color::Green);
    lSprite.setOrigin({16,0});
    lShape.setOrigin({16,16});
    lSprite.setPosition({100,100});
    lSprite2.setPosition({100,100});
    lShape.setPosition({100,100});

    m_gameEngine.GetWindow().draw(m_text);
    m_gameEngine.GetWindow().draw(lSprite);
    m_gameEngine.GetWindow().draw(lSprite2);
    m_gameEngine.GetWindow().draw(lShape);
}

void Scene_Menu::OnKeyboardPressed(sf::Keyboard::Key Key)
{
    SceneBase::OnKeyboardPressed(Key);
    
    if(Key == sf::Keyboard::Key::W)
    {
        m_text.setPosition({500,500});
    }
}

void Scene_Menu::OnInit()
{
    if(m_entityMgr)
    {
        m_player = m_entityMgr->AddEntity(EntityTags::player);
        m_player->AddComponent<CTransform>(FVector2D{500,500});
        CRender& lRender = m_player->AddComponent<CRender>(m_gameEngine.GetAssetManager()->GetTexture("T_CharacterGreen"));
        m_player->AddComponent<CAnimation>(lRender, m_gameEngine.GetAssetManager()->GetSpriteSheet("GreenCharacter"));
    }
}
