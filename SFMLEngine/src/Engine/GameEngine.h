#pragma once

#include <SFML/Graphics.hpp>
#include "EngineType.hpp"
#include "EntityManager.h"

class Entity;

/*
 * 
 */


class GameEngine
{
private:
    std::string m_config_path;

    //Window
    sf::RenderWindow    m_window{};
    unsigned int        m_width         = 1280;
    unsigned int        m_height        = 720;
    int                 m_framerate     = 60;
    int                 m_fullscreen    = 0;

    //Entities
    TUniquePtr<EntityManager> m_entityMgr;

    //Player
    TSharedPtr<Entity>  m_player;
    int                 m_playerShapeRadius         = 30;
    int                 m_playerCollisionRadius     = 30;
    int                 m_playerSpeed               = 8;
    int                 m_playerFillColor[3]        = {250, 100, 30};
    int                 m_playerOutlineColor[3]     = {30, 100, 250};
    int                 m_playerOutlineThickness    = 2;
    int                 m_playerVerticesCount       = 8;

    //Bullet
    int m_bulletShapeRadius         = 15;
    int m_bulletCollisionRadius     = 15;
    int m_bulletSpeed               = 8;
    int m_bulletFillColor[3]        = {181, 184, 232};
    int m_bulletOutlineColor[3]     = {109, 117, 242};
    int m_bulletOutlineThickness    = 1;
    int m_bulletVerticesCount       = 32;
    int m_bulletLifeSpan            = 90;

    //Enemy
    int     m_enemyShapeRadius      = 35;
    int     m_enemyCollisionRadius  = 35;
    float   m_enemyMinSpeed         = -1;
    float   m_enemyMaxSpeed         = 1;
    int     m_enemyFillColor[3]     = {242, 112, 5};
    int     m_enemyOutlineColor[3]  = {242, 64, 5};
    int     m_enemyOutlineThickness = 3;
    int     m_enemyMinVerticesCount = 3;
    int     m_enemyMaxVerticesCount = 12;
    int     m_enemyLifeSpan         = 12;
    int     m_enemySpawnInterval    = 30;
    int     m_currEnemySpawnFrame   = 0;
    int     m_lastEnemySpawnFrame   = 0;
    int     m_enemySpawnXSafeZone   = 30;
    int     m_enemySpawnYSafeZone   = 30;

    //ImGUI
    bool bIsImguiInit{false};
    sf::Clock m_deltaClock;

    //Score
    sf::Font	m_scoreFont;
    sf::Text    m_scoreText;
    int         m_scoreCurrent{0};
    int         m_scoreEnemy{500};
    int         m_scoreSmallEnemy{250};

    //Debug
    bool bCanShoot = true;
    bool bCanEnemyMove = true;
    
private:
    void Init();
    void SetupDefaultConfig();

    void CreateSFMLWindow();
    void CreatePlayer();

    void ConstructGUIGeneralTab();
    void ConstructGUIEntitiesTab() const;

    void OnBulletColliding(TSharedPtr<Entity> Bullet, TSharedPtr<Entity> Enemy);
    void SpawnSmallEnemy(TSharedPtr<Entity> Enemy);

    void sMovement() const;
    void sCollision();
    void sSpawner();
    void sLifeSpan() const;
    void sRender();
    void sUserInput(const std::optional<sf::Event>& Event);
    void sImGUI();
    
public:
    GameEngine();
    GameEngine(const std::string& config_path);
    ~GameEngine() = default;

    void Run();
};
