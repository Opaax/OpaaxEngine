#pragma once
#include "SceneBase.h"
#include <SFML/Graphics/Text.hpp>

class Entity;

class Scene_Menu : public SceneBase
{
    sf::Font m_testFont{};
    sf::Text m_text;

    TSharedPtr<Entity> m_player;

public:
    explicit Scene_Menu(const GameEngine& GameEngine):SceneBase(GameEngine),m_text{m_testFont}  {}
    ~Scene_Menu() override = default;
    
    void Update() override;
    void Render() override;

protected:
    void OnKeyboardPressed(sf::Keyboard::Key Key)   override;
    void OnKeyboardReleased(sf::Keyboard::Key Key)  override{}
    void OnMousePressed(sf::Mouse::Button Button)   override{}
    void OnMouseReleased(sf::Mouse::Button Button)  override{}
    void RegisterActions() override{}
    void OnInit() override;

public:
    void DoAction(const ActionBase& Action) override {SceneBase::DoAction(Action);}
};
