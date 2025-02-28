#pragma once
#include <unordered_map>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>

#include "../EngineType.hpp"

class EntityManager;
class GameEngine;
class SceneBase;
class SceneFactory;

class SceneBase
{
protected:
    const GameEngine& m_gameEngine;
    TUniquePtr<EntityManager> m_entityMgr;

public:
    explicit SceneBase(const GameEngine& GameEngine);
    virtual ~SceneBase();

    virtual void Update() = 0;
    virtual void Render() = 0;

    const GameEngine& GetGameEngine() const {return m_gameEngine;}
};

class Scene_Menu : public SceneBase
{
    sf::Font m_testFont{};
    sf::Text m_text;
    
public:
    explicit Scene_Menu(const GameEngine& GameEngine):SceneBase(GameEngine),m_text{m_testFont}  {}
    ~Scene_Menu() override = default;

    void Update() override {}
    void Render() override;
};

class Scene_Game : public SceneBase
{
    sf::Font m_testFont{};
    sf::Text m_text;
    
public:
    explicit Scene_Game(const GameEngine& GameEngine):SceneBase(GameEngine),m_text{m_testFont}  {}
    ~Scene_Game() override = default;

    void Update() override {}
    void Render() override;
};