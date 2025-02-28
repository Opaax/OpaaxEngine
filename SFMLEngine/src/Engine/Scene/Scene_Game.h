#pragma once
#include "SceneBase.h"
#include <SFML/Graphics/Text.hpp>

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
    void RegisterActions() override{}

public:
    void DoAction(const ActionBase& Action) override {SceneBase::DoAction(Action);}
};
