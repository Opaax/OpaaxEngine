#pragma once

#include <SFML/Graphics/CircleShape.hpp>
#include "Math/Vector2D.hpp"

//For now component are just Data.
class Component
{
    bool bExist{false};
public:
    Component() = default;

public:
    bool IsExisting() const {return bExist;}
    void SetExisting(bool NewExistState) {bExist = NewExistState;}
};

class CTransform : public Component
{
    Vector2D<float> m_position{0,0};
    Vector2D<float> m_velocity{0,0};
    float           m_rotation{0};
    
public:
    CTransform() = default;
    CTransform(const CTransform& InTransform):m_position{InTransform.m_position},m_velocity{InTransform.m_velocity}{}
    CTransform(const Vector2D<float>& InPosition):m_position(InPosition), m_velocity(){}
    CTransform(const Vector2D<float>& InPosition, const Vector2D<float>& InVelocity):m_position(InPosition), m_velocity(InVelocity){}
    ~CTransform() = default;

    Vector2D<float>&    GetPosition() {return m_position;}
    float               GetRotation() {return m_rotation;}
    Vector2D<float>&    GetVelocity() {return m_velocity;}
    
    void SetVelocity(const Vector2D<float>& InVel) {m_velocity = InVel;}
    
    const Vector2D<float>&  GetPosition() const {return m_position;}
    const float             GetRotation() const {return m_rotation;}
    const Vector2D<float>&  GetVelocity() const {return m_velocity;}

    void Move()
    {
        m_position += m_velocity;
    }

    void Rotate()
    {
        m_rotation += 2.f;
    }
};

class CPlayerInput : public Component
{
public:
    bool Up{false};
    bool Down{false};
    bool Right{false};
    bool Left{false};
    bool LeftMouse{false};
    Vector2D<float> LeftClickPosition{};
public:
    CPlayerInput() = default;
};

class CCircleShape : public Component
{
private:
    sf::CircleShape m_circle{};
    
public:
    CCircleShape() = default;
    CCircleShape(float InRadius, int InVerticesCount, const sf::Color& FillColor, const sf::Color& OutlineColor, float OutlineThickness)
        : m_circle(InRadius, InVerticesCount) 
    {
        m_circle.setFillColor(FillColor);
        m_circle.setOutlineColor(OutlineColor);
        m_circle.setOutlineThickness(OutlineThickness);
        m_circle.setOrigin({InRadius, InRadius});
    }

public:
    const sf::CircleShape& GetCircleShape() const {return m_circle;}
    sf::CircleShape& GetCircleShape() {return m_circle;}
};

class CLifeSpan : public Component
{
    int m_remaining{ 0 };
    int m_total{ 0 };
    
public:
    CLifeSpan() = default;
    CLifeSpan(int total)
        : m_remaining(total), m_total(total) {}

    /*
     * Remove one frame on remaining
     */
    void Elapse()
    {
        m_remaining--;
    }

    int GetRemaining() {return m_remaining;}
    int GetRemaining() const {return m_remaining;}

    int GetTotal() {return m_total;}
    int GetTotal() const {return m_total;}
    
    bool IsDead() const {return m_remaining <= 0;}

    float GetLifePercent() const {return static_cast<float>(m_remaining) / static_cast<float>(m_total);}
    float GetLifePercent() {return static_cast<const CLifeSpan>(*this).GetLifePercent();} //cast to const otherwise call no const version
};

class CCollision : public Component
{
private:
    float m_radius{0};
    
public:
    CCollision() = default;
    CCollision(float InRadius): m_radius{InRadius}{}
    CCollision(int InRadius): m_radius{static_cast<float>(InRadius)}{}

    float GetRadius() const {return m_radius;}
};