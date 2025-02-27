#include "GameEngine.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "EntityManager.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui-SFML.h"
#include "Math/Math.hpp"
#include "Math/Vector2D.hpp"
#include "SFMLExtension/SFMLExtension.hpp"

//CTOR Default
GameEngine::GameEngine(): m_scoreText(m_scoreFont)
{
    Init();
}

//CTOR With config path
GameEngine::GameEngine(const std::string& config_path): m_scoreText(m_scoreFont)
{
    m_config_path = config_path;
    Init();
}

void GameEngine::Init()
{
    Math::RandInit();

    
    m_assetMgr = std::make_unique<AssetsManager>();
    m_entityMgr = std::make_unique<EntityManager>();

    if (!m_scoreFont.openFromFile("resources/bell-mt.ttf"))
    {
        std::cerr << "Failed to load font. Filepath: " << std::endl;
    }

    m_scoreText.setFont(m_scoreFont);
    m_scoreText.setCharacterSize(20);
    
    if(!m_config_path.empty())
    {
        //Read in the config file.
        std::fstream lConfigInput{m_config_path};

        //Check if the config is valid, else create game with defaults.
        if (!lConfigInput.is_open())
        {
            std::cerr << "Failed to open: " << m_config_path << '\n' << "The game window will be launch with a default config" << '\n';
            SetupDefaultConfig();
            return;
        }

        std::string lLine;
        while (std::getline(lConfigInput, lLine)) {
            // Do not read comments
            size_t lCommentPos = lLine.find('#');
            if (lCommentPos != std::string::npos) {
                lLine = lLine.substr(0, lCommentPos);
            }
            
            //Read properties from the config. But Ignore Empty
            std::istringstream lLineStream(lLine);
            std::string lPropName{};
            
            if (!(lLineStream >> lPropName))
            {
                continue;
            }

            //Windows properties
            if(lPropName == "Window" &&
                lLineStream >> m_width
                >> m_height
                >> m_framerate
                >> m_fullscreen)
            {
                CreateSFMLWindow();
                continue;
            }

            //Player properties
            if(lPropName == "Player" &&
                lLineStream >> m_playerShapeRadius
                >> m_playerCollisionRadius
                >> m_playerSpeed
                >> m_playerFillColor[0] >> m_playerFillColor[1] >> m_playerFillColor[2]
                >> m_playerOutlineColor[0] >> m_playerOutlineColor[1]  >> m_playerOutlineColor[2]
                >> m_playerOutlineThickness
                >> m_playerVerticesCount)
            {
                CreatePlayer();
            }

            //Bullet properties
            if(lPropName == "Bullet" &&
                lLineStream >> m_bulletShapeRadius
                >> m_bulletCollisionRadius
                >> m_bulletSpeed
                >> m_bulletFillColor[0] >> m_bulletFillColor[1] >> m_bulletFillColor[2]
                >> m_bulletOutlineColor[0] >> m_bulletOutlineColor[1]  >> m_bulletOutlineColor[2]
                >> m_bulletOutlineThickness
                >> m_bulletOutlineThickness
                >> m_bulletLifeSpan)
            {/*Do Things on Bullet properties setup*/}

            //Enemy Properties
            if(lPropName == "Enemy" &&
                lLineStream >> m_enemyShapeRadius
                >> m_enemyCollisionRadius
                >> m_enemyMinSpeed
                >> m_enemyMaxSpeed
                >> m_enemyFillColor[0] >> m_enemyFillColor[1] >> m_enemyFillColor[2]
                >> m_enemyOutlineColor[0] >> m_enemyOutlineColor[1]  >> m_enemyOutlineColor[2]
                >> m_enemyOutlineThickness
                >> m_enemyMinVerticesCount
                >> m_enemyMaxVerticesCount
                >> m_enemyLifeSpan)
            {/*Do Things on Enemy properties setup*/}

            if(lPropName == "EnemySpawn" &&
                lLineStream >> m_enemySpawnXSafeZone
                >> m_enemySpawnYSafeZone
                >> m_enemySpawnInterval)
            { /*Do Things on Enemy Spawn properties setup*/ }

            if(lPropName == "Score" &&
                lLineStream >> m_scoreEnemy
                >> m_scoreSmallEnemy)
            { /*Do Things on Score properties setup*/}
        }
    }else
    {
        SetupDefaultConfig();
    }
}

void GameEngine::SetupDefaultConfig()
{
    std::cout << "The Game is setup with default config" << '\n';
    CreateSFMLWindow();
    CreatePlayer();
}

void GameEngine::CreateSFMLWindow()
{
    sf::VideoMode lVideoMode;
    sf::State lWindowState = m_fullscreen == 1 ? sf::State::Fullscreen : sf::State::Windowed;

    switch (lWindowState)
    {
    case sf::State::Windowed:
        lVideoMode = sf::VideoMode({m_width, m_height});
        break;
    case sf::State::Fullscreen:
        lVideoMode = sf::VideoMode::getFullscreenModes()[0];
        break;
    }

    m_window.create(lVideoMode, "Geometry Wars", lWindowState);
    m_window.setFramerateLimit(m_framerate);

    bIsImguiInit = ImGui::SFML::Init(m_window);
}

void GameEngine::CreatePlayer()
{
    m_player = m_entityMgr->AddEntity(EntityTags::player);
    
    m_player->AddComponent<CCircleShape>(
        m_playerShapeRadius,
        m_playerVerticesCount,
        sf::Color(m_playerFillColor[0], m_playerFillColor[1], m_playerFillColor[2]),
        sf::Color(m_playerOutlineColor[0], m_playerOutlineColor[1], m_playerOutlineColor[2]),
        m_playerOutlineThickness);
    m_player->AddComponent<CTransform>(Vector2D<float>(500,500));
    m_player->AddComponent<CPlayerInput>();
    m_player->AddComponent<CCollision>(m_playerCollisionRadius);
}

void GameEngine::ConstructGUIGeneralTab()
{
    ImGui::Text("Debug General settings.");
    ImGui::Spacing();

    ImGui::Checkbox("Can Shoot", &bCanShoot);
    ImGui::Checkbox("Can Enemy Move", &bCanEnemyMove);
}

void GameEngine::ConstructGUIEntitiesTab() const
{
    ImGui::Text("Debug entities.");
            ImGui::Spacing();

            for (auto& lPair : m_entityMgr->GetEntitiesMap())
            {
                STDString lHeader{};
                bool bDeletable = false;
                switch (lPair.first) {
                case EntityTags::none:
                    lHeader = "None";
                    break;
                case EntityTags::player:
                    lHeader = "Player";
                    break;
                case EntityTags::enemy:
                    lHeader = "Enemy";
                    bDeletable = true;
                    break;
                case EntityTags::smallEnemy:
                    lHeader = "SmallEnemy";
                    bDeletable = true;
                    break;
                case EntityTags::bullet:
                    lHeader = "Bullet";
                    bDeletable = true;
                    break;
                }
                
                if (ImGui::CollapsingHeader(lHeader.c_str()))
                {
                    for (TSharedPtr<Entity> Entity : lPair.second)
                    {
                        if(bDeletable)
                        {
                            ImGui::PushID(Entity->GetID());
                            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(Entity->GetID() / 7.0f, 0.6f, 0.6f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(Entity->GetID() / 7.0f, 0.7f, 0.7f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(Entity->GetID() / 7.0f, 0.8f, 0.8f));                                                        
                            if(ImGui::Button("D"))
                            {
                                Entity->Destroy();
                            }

                            ImGui::PopStyleColor(3);
                            ImGui::PopID();
                        }
                        ImGui::SameLine();
                        ImGui::Text(Entity->ToString().c_str());
                    }
                }
            }
}

void GameEngine::ConstructAssetsTab() const
{
    //Asset can be select from GUI, so we need static index (took from demo)
    static int lAssetCurrentGUIListBox = 1;
    //Convert vector to raw const char* array.
    const char* lItem[EAssetsType::Asset_Count];
    std::transform(m_assetMgr->AllAssetType.begin(), m_assetMgr->AllAssetType.end(), lItem, 
                   [](const std::string& str) { return str.c_str(); });
    ImGui::ListBox("All Type", &lAssetCurrentGUIListBox, lItem, EAssetsType::Asset_Count, 4);
}

void GameEngine::OnBulletColliding(TSharedPtr<Entity> Bullet, TSharedPtr<Entity> Enemy)
{
    if(Enemy->GetEntityTag() == EntityTags::enemy)
    {
        m_scoreCurrent += m_scoreEnemy;
        SpawnSmallEnemy(Enemy);
    }
    else if(Enemy->GetEntityTag() == EntityTags::smallEnemy)
    {
        m_scoreCurrent += m_scoreSmallEnemy;
    }
    
    Bullet->Destroy();
    Enemy->Destroy();
}

void GameEngine::SpawnSmallEnemy(TSharedPtr<Entity> Enemy)
{
    const CTransform& lEnemyTransform = Enemy->GetComponent<CTransform>();
    const int lVerticeCount           = Enemy->GetComponent<CCircleShape>().GetCircleShape().getPointCount();
    sf::Color lColor                  = Enemy->GetComponent<CCircleShape>().GetCircleShape().getFillColor();
    sf::Color lColorOutline           = Enemy->GetComponent<CCircleShape>().GetCircleShape().getOutlineColor();
    const int lSmallShapeRadius       = m_enemyShapeRadius / 2;

    const FVector2D lEnemyPos = lEnemyTransform.GetPosition();
    const float lSpeed = lEnemyTransform.GetVelocity().Length();

    for (int i = 0; i < lVerticeCount; ++i)
    {
        TSharedPtr<Entity> lSmallEnemy = m_entityMgr->AddEntity(EntityTags::smallEnemy);

        lSmallEnemy->AddComponent<CCircleShape>(
        lSmallShapeRadius,
        lVerticeCount,
        lColor,
        lColorOutline,
        m_enemyOutlineThickness);
        
        float lAngleRad = Math::DegreesToRadians((360 / lVerticeCount) * i);
        FVector2D lDir{std::cos(lAngleRad), std::sin(lAngleRad)};

        lSmallEnemy->AddComponent<CTransform>(lEnemyPos, lDir.Normalize() * lSpeed);
        lSmallEnemy->AddComponent<CLifeSpan>(120);
        lSmallEnemy->AddComponent<CCollision>(lSmallShapeRadius);
    }
}

void GameEngine::sMovement() const
{
    //Based on input, apply new velocity to Player.
    for (auto Player : m_entityMgr->GetEntities(EntityTags::player))
    {
        Vector2D<float> lPlayerVelocity{};
        CPlayerInput& lPlayerInput = Player->GetComponent<CPlayerInput>();

        lPlayerVelocity.x -= lPlayerInput.Left ? 1 : 0;
        lPlayerVelocity.x += lPlayerInput.Right ? 1 : 0;
        lPlayerVelocity.y -= lPlayerInput.Up ? 1 : 0;
        lPlayerVelocity.y += lPlayerInput.Down ? 1 : 0;

        Player->GetComponent<CTransform>().SetVelocity(lPlayerVelocity.Normalize() * m_playerSpeed);
    }

    //Move all entities that have velocity
    for (TSharedPtr<Entity> lEntity : m_entityMgr->GetEntities())
    {
        if(lEntity->HasComponent<CTransform>())
        {
            CTransform& lEntityTrans = lEntity->GetComponent<CTransform>();

            bool lbCanMove = true;

            switch (lEntity->GetEntityTag()) {
            case EntityTags::none:
                break;
            case EntityTags::player:
                
                break;
            case EntityTags::enemy:
                lbCanMove = bCanEnemyMove;
                break;
            case EntityTags::smallEnemy:
                break;
            case EntityTags::bullet:
                break;
            }

            if(lbCanMove)
            {
                lEntityTrans.Move();
            }
            lEntityTrans.Rotate();

            FVector2D& lEntityNewPos = lEntityTrans.GetPosition();
            float lEntityRadius = lEntity->GetComponent<CCircleShape>().GetCircleShape().getRadius();
            
            const int lWindowX = m_window.getSize().x;
            const int lWindowY = m_window.getSize().y;
            
            IVector2D lXWindow = {0, lWindowX};
            IVector2D lYWindow = {0, lWindowY};

            if(lEntity->GetEntityTag() == EntityTags::player)
            {
                lEntityNewPos.x = lEntityNewPos.x - lEntityRadius < lXWindow.x ? 0 + lEntityRadius :
                    lEntityNewPos.x + lEntityRadius > lXWindow.y ? lXWindow.y - lEntityRadius : lEntityNewPos.x;

                lEntityNewPos.y = lEntityNewPos.y - lEntityRadius < lYWindow.x ? 0 + lEntityRadius :
                    lEntityNewPos.y + lEntityRadius > lYWindow.y ? lYWindow.y - lEntityRadius : lEntityNewPos.y;
            }
            else if(lEntity->GetEntityTag() == EntityTags::enemy)
            {
                if(lEntityNewPos.x - lEntityRadius < lXWindow.x || lEntityNewPos.x + lEntityRadius > lXWindow.y)
                {
                    lEntityTrans.GetVelocity().x *= -1;
                }

                if(lEntityNewPos.y - lEntityRadius < lYWindow.x || lEntityNewPos.y + lEntityRadius > lYWindow.y)
                {
                    lEntityTrans.GetVelocity().y *= -1;
                }
            }
        }
    }
}

void GameEngine::sCollision()
{
    for (TSharedPtr<Entity> lBullet : m_entityMgr->GetEntities(EntityTags::bullet))
    {
        const CTransform& lBulletTransform = lBullet->GetComponent<CTransform>();
        const CCollision& lBulletCollision = lBullet->GetComponent<CCollision>();

        for (TSharedPtr<Entity> lEnemy : m_entityMgr->GetEntities(EntityTags::enemy))
        {
            const CTransform& lEnemyTransform = lEnemy->GetComponent<CTransform>();
            const CCollision& lEnemyCollision = lEnemy->GetComponent<CCollision>();

            const float lSqDistToBullet = (lBulletTransform.GetPosition() - lEnemyTransform.GetPosition()).SqLength();
            const float lSqRadius{(lBulletCollision.GetRadius() + lEnemyCollision.GetRadius()) * (lBulletCollision.GetRadius() + lEnemyCollision.GetRadius())};

            if(lSqDistToBullet <= lSqRadius)
            {
                OnBulletColliding(lBullet, lEnemy);
                break;
            }
        }

        for (TSharedPtr<Entity> lSmallEnemy : m_entityMgr->GetEntities(EntityTags::smallEnemy))
        {
            const CTransform& lSmallEnemyTransform = lSmallEnemy->GetComponent<CTransform>();
            const CCollision& lSmallEnemyCollision = lSmallEnemy->GetComponent<CCollision>();

            const float lSqDistToBullet = (lBulletTransform.GetPosition() - lSmallEnemyTransform.GetPosition()).SqLength();
            const float lSqRadius{(lBulletCollision.GetRadius() + lSmallEnemyCollision.GetRadius()) * (lBulletCollision.GetRadius() + lSmallEnemyCollision.GetRadius())};

            if(lSqDistToBullet <= lSqRadius)
            {
                OnBulletColliding(lBullet, lSmallEnemy);
                break;
            }
        }
    }
}

void GameEngine::sSpawner()
{
    if(m_player->GetComponent<CPlayerInput>().LeftMouse && bCanShoot)
    {
        m_player->GetComponent<CPlayerInput>().LeftMouse = false;

        TSharedPtr<Entity> lBullet = m_entityMgr->AddEntity(EntityTags::bullet);
        lBullet->AddComponent<CCircleShape>(
        m_bulletShapeRadius,
        m_bulletVerticesCount,
        sf::Color(m_bulletFillColor[0]      ,m_bulletFillColor[1]   ,m_bulletFillColor[2]),
        sf::Color(m_bulletOutlineColor[0]   ,m_bulletOutlineColor[1],m_bulletOutlineColor[2]),
        m_bulletOutlineThickness);

        FVector2D lPlayerPosition   = m_player->GetComponent<CTransform>().GetPosition();
        FVector2D lClickPosition    = m_player->GetComponent<CPlayerInput>().LeftClickPosition;
        FVector2D lBulletPosition   = lPlayerPosition;
        FVector2D lBulletVelocity   = (lClickPosition - lPlayerPosition).Normalize() * m_bulletSpeed;
        
        lBullet->AddComponent<CTransform>(lBulletPosition, lBulletVelocity);
        lBullet->AddComponent<CLifeSpan>(m_bulletLifeSpan);
        lBullet->AddComponent<CCollision>(m_bulletCollisionRadius);
    }

    if(m_currEnemySpawnFrame - m_lastEnemySpawnFrame >= m_enemySpawnInterval)
    {
        TSharedPtr<Entity> lEnemy = m_entityMgr->AddEntity(EntityTags::enemy);

        const int lVerticeCount = Math::RandomRange(m_enemyMinVerticesCount, m_enemyMaxVerticesCount);

        sf::Color lRandomColor          = SFMLExtension::GetRandomColor();
        sf::Color lRandomColorOutline   = SFMLExtension::GetRandomColor();
        
        lEnemy->AddComponent<CCircleShape>(
        m_enemyShapeRadius,
        lVerticeCount,
        lRandomColor,
        lRandomColorOutline,
        m_enemyOutlineThickness);

        const int lMinPosX{ m_enemySpawnXSafeZone };
        const int lMaxPosX{ static_cast<int>(m_window.getSize().x - m_enemySpawnXSafeZone) };
	         
        const int lMinPosY{ m_enemySpawnYSafeZone };
        const int lMaxPosY{ static_cast<int>(m_window.getSize().y - m_enemySpawnYSafeZone) };

        const float XPos = Math::RandomRange(lMinPosX, lMaxPosX);
        const float YPos = Math::RandomRange(lMinPosY, lMaxPosY);
        
        FVector2D lSpawnPosition { XPos,YPos};

        const float lMinSpeed = m_enemyMinSpeed <= 0 ? 1 : m_enemyMinSpeed;
        const float lMaxSpeed = m_enemyMaxSpeed <= 0 ? 1 : m_enemyMaxSpeed;

        const float lSpeed = Math::RandomRange(lMinSpeed, lMaxSpeed);
        FVector2D lDirection = Math::VectorFRand();
        
        lEnemy->AddComponent<CTransform>(lSpawnPosition, lDirection.Normalize() * lSpeed );
        lEnemy->AddComponent<CLifeSpan>(m_enemyLifeSpan);
        lEnemy->AddComponent<CCollision>(m_enemyCollisionRadius);

        m_lastEnemySpawnFrame = m_currEnemySpawnFrame;
    }

    m_currEnemySpawnFrame++;
}

void GameEngine::sLifeSpan() const
{
    for (TSharedPtr<Entity> lEntity : m_entityMgr->GetEntities())
    {
        if(lEntity->HasComponent<CLifeSpan>())
        {
            CLifeSpan& lLifeComp = lEntity->GetComponent<CLifeSpan>();
            lLifeComp.Elapse();

            if(lEntity->HasComponent<CCircleShape>())
            {
                //cache alpha multiplier base on remaining life span.
                float lAlphaMultiplier = lLifeComp.GetLifePercent();

                //stack cache for easy access.
                CCircleShape& lCircleComp = lEntity->GetComponent<CCircleShape>();
                sf::Color lFillColor{ lCircleComp.GetCircleShape().getFillColor()};
                sf::Color lOutlineColor{ lCircleComp.GetCircleShape().getOutlineColor()};

                //Convert float alpha to uint alpha.
                std::uint8_t lAlpha = static_cast<std::uint8_t>(255 * lAlphaMultiplier);
                lFillColor.a = lOutlineColor.a = lAlpha;

                //reapply color with alpha
                lCircleComp.GetCircleShape().setFillColor(lFillColor);
                lCircleComp.GetCircleShape().setOutlineColor(lOutlineColor);
            }

            if(lLifeComp.IsDead())
            {
                lEntity->Destroy();
            }
        }
    }
}

void GameEngine::sRender()
{
    for (const TSharedPtr<Entity> lEntity : m_entityMgr->GetEntities())
    {
        if(lEntity->HasComponent<CCircleShape>())
        {
            const CTransform& lTrans    = lEntity->GetComponent<CTransform>();
            sf::Vector2f lWindowPos     = lTrans.GetPosition().GetSFMLVec2F();
            CCircleShape& lCircleShape  = lEntity->GetComponent<CCircleShape>();
            lCircleShape.GetCircleShape().setPosition(lWindowPos);
            lCircleShape.GetCircleShape().setRotation(sf::radians(Math::DegreesToRadians(lTrans.GetRotation())));
            m_window.draw(lEntity->GetComponent<CCircleShape>().GetCircleShape());
        }
    }

    m_scoreText.setPosition({30, 30});
    m_scoreText.setString("Score: " + std::to_string(m_scoreCurrent));
    m_window.draw(m_scoreText);
}

void GameEngine::sUserInput(const std::optional<sf::Event>& Event)
{
    if(Event.value().getIf<sf::Event::KeyPressed>())
    {
        switch (sf::Keyboard::Key lKey = Event.value().getIf<sf::Event::KeyPressed>()->code)
        {
        case sf::Keyboard::Key::W:
            m_player->GetComponent<CPlayerInput>().Up = true;
            break;
        case sf::Keyboard::Key::S:
            m_player->GetComponent<CPlayerInput>().Down = true;
            break;
        case sf::Keyboard::Key::D:
            m_player->GetComponent<CPlayerInput>().Right = true;
            break;
        case sf::Keyboard::Key::A:
            m_player->GetComponent<CPlayerInput>().Left = true;
            break;
        case sf::Keyboard::Key::P:
            break;
        case sf::Keyboard::Key::Escape:
            m_window.close();
            break;
        default: ;
        }
    }

    if(Event.value().getIf<sf::Event::KeyReleased>())
    {
        switch (sf::Keyboard::Key lKey = Event.value().getIf<sf::Event::KeyReleased>()->code)
        {
        case sf::Keyboard::Key::W:
            m_player->GetComponent<CPlayerInput>().Up = false;
            break;
        case sf::Keyboard::Key::S:
            m_player->GetComponent<CPlayerInput>().Down = false;
            break;
        case sf::Keyboard::Key::D:
            m_player->GetComponent<CPlayerInput>().Right = false;
            break;
        case sf::Keyboard::Key::A:
            m_player->GetComponent<CPlayerInput>().Left = false;
            break;
        case sf::Keyboard::Key::P:
            break;
        case sf::Keyboard::Key::Escape:
            m_window.close();
            break;
        default: ;
        }
    }

    if(const auto& lEvent = Event.value().getIf<sf::Event::MouseButtonPressed>())
    {
        if(lEvent->button == sf::Mouse::Button::Left)
        {
            m_player->GetComponent<CPlayerInput>().LeftMouse = true;
            m_player->GetComponent<CPlayerInput>().LeftClickPosition = Vector2D<float>::IntToFloat({lEvent->position.x, lEvent->position.y});
        }
    }

    if(Event.value().getIf<sf::Event::MouseButtonReleased>())
    {
        if(Event.value().getIf<sf::Event::MouseButtonReleased>()->button == sf::Mouse::Button::Left)
        {
            m_player->GetComponent<CPlayerInput>().LeftMouse = false;
        }
    }
}

void GameEngine::sImGUI()
{
    ImGui::SFML::Update(m_window, m_deltaClock.restart());

    ImGui::ShowDemoWindow();

    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_Always);
    ImGui::SetNextWindowPos({m_width - 310.f, 10}, ImGuiCond_Always);

    ImGuiWindowFlags lDefaultWindowFlag{};
    bool* lDefaultWindowOpenState{};
    
    ImGui::Begin("Opaax Engine", lDefaultWindowOpenState, lDefaultWindowFlag);
    ImGuiTabBarFlags lTabFlags = ImGuiTabBarFlags_None;
    if(ImGui::BeginTabBar("EngineTabBar#Default"))
    {
        if (ImGui::BeginTabItem("General"))
        {
            ConstructGUIGeneralTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Entities"))
        {
            ConstructGUIEntitiesTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Assets"))
        {
            ImGui::SeparatorText("All Assets Type");
            ConstructAssetsTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    
    ImGui::End();
}

void GameEngine::Run()
{
    while (m_window.isOpen())
    {
        if(m_entityMgr)
        {
            m_entityMgr->Update();
        }
        
        while (const std::optional<sf::Event> lEvent = m_window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(m_window, *lEvent);
            
            sUserInput(lEvent);
            
            if(lEvent.value().getIf<sf::Event::Closed>())
            {
                m_window.close();
                break;
            }
        }

        sMovement();
        sCollision();
        sSpawner();
        sLifeSpan();
        sImGUI();

        m_window.clear();
        sRender();
        ImGui::SFML::Render(m_window);
        m_window.display();
    }
}
