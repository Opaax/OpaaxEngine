#pragma once
#include <SFML/Graphics/Font.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Mouse.hpp>
#include "../EngineType.hpp"
#include "../Actions/ActionBase.h"

class EntityManager;
class GameEngine;

using ActionMap = std::unordered_map<sf::Keyboard::Key, STDString>;

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
    ActionMap m_keyActions;

private:
    void Init();
    
protected:
    virtual void OnKeyboardPressed(sf::Keyboard::Key Key);
    virtual void OnKeyboardReleased(sf::Keyboard::Key Key) = 0;
    virtual void OnMousePressed(sf::Mouse::Button Button) = 0;
    virtual void OnMouseReleased(sf::Mouse::Button Button) = 0;

    virtual void RegisterActions() {}

    void RegisterKeyAction(sf::Keyboard::Key Key, STDString ActionName);

public:
    explicit SceneBase(const GameEngine& GameEngine);
    virtual ~SceneBase();

    void ProcessKeyboardInput(sf::Keyboard::Key Key, EInputActionType ActionType);
    void ProcessMouseInput(sf::Mouse::Button Button, EInputActionType ActionType);

    virtual void Update() = 0;
    virtual void DoAction(const ActionBase& Action);
    virtual void Render() = 0;

    const GameEngine& GetGameEngine() const {return m_gameEngine;}
};