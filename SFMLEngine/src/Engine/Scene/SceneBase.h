#pragma once
#include <unordered_map>
#include <SFML/Graphics/Font.hpp>
#include <SFML/Graphics/Text.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>

#include "../EngineType.hpp"

class EntityManager;
class GameEngine;
class SceneBase;
class SceneFactory;

enum  struct EInputActionType
{
    Pressed = 0,
    Released
};

class SceneBase
{
protected:
    const GameEngine& m_gameEngine;
    TUniquePtr<EntityManager> m_entityMgr;

protected:
    virtual void OnKeyboardPressed(sf::Keyboard::Key) = 0;
    virtual void OnKeyboardReleased(sf::Keyboard::Key) = 0;
    virtual void OnMousePressed(sf::Mouse::Button) = 0;
    virtual void OnMouseReleased(sf::Mouse::Button) = 0;

public:
    explicit SceneBase(const GameEngine& GameEngine);
    virtual ~SceneBase();

    void ProcessKeyboardInput(sf::Keyboard::Key Key, EInputActionType ActionType);
    void ProcessMouseInput(sf::Mouse::Button Button, EInputActionType ActionType);

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

protected:
    void OnKeyboardPressed(sf::Keyboard::Key Key)   override;
    void OnKeyboardReleased(sf::Keyboard::Key Key)  override{}
    void OnMousePressed(sf::Mouse::Button Button)   override{}
    void OnMouseReleased(sf::Mouse::Button Button)  override{}
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

protected:
    void OnKeyboardPressed(sf::Keyboard::Key Key)   override;
    void OnKeyboardReleased(sf::Keyboard::Key Key)  override{}
    void OnMousePressed(sf::Mouse::Button Button)      override{}
    void OnMouseReleased(sf::Mouse::Button Button)     override{}
};