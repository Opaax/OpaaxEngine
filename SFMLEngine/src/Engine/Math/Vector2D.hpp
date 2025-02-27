#pragma once
#include "../../imgui/imgui-SFML.h"

template<typename T>
struct Vector2D
{
    T x;
    T y;

    Vector2D() = default;
    Vector2D(T InX, T InY):
    x{InX}, y{InY}{}

    static Vector2D<float> IntToFloat(const Vector2D<int>& vec) 
    {
        return Vector2D<float>{static_cast<float>(vec.x), static_cast<float>(vec.y)};
    }

    static Vector2D<int> FloatToInt(const Vector2D<float>& vec)
    {
        return Vector2D<int>{static_cast<int>(vec.x), static_cast<int>(vec.y)};
    }

    sf::Vector2f GetSFMLVec2F() {return {x,y};}
    sf::Vector2f GetSFMLVec2F() const {return {x,y};}

    double SqLength() const
    {
        return x * x + y * y;
    }
    
    double SqLength()
    {
        return static_cast<const Vector2D<T>>(*this).SqLength();
    }

    double Length() const
    {
        return std::sqrt(x*x + y*y);
    }

    double Length()
    {
        return static_cast<const Vector2D<T>>(*this).Length();
    }

    Vector2D& Normalize()
    {
        double lLength = Length();

        if(lLength <= 0)
        {
            return *this;
        }
        
        x /= lLength;  // NOLINT(clang-diagnostic-implicit-float-conversion)
        y /= lLength;  // NOLINT(clang-diagnostic-implicit-float-conversion)

        return *this;
    }
    
    static Vector2D& Normalize(Vector2D ToNormalize)
    {
        return *ToNormalize.Normalize();
    }

    template <typename U>
    friend std::ostream& operator<<(std::ostream& stream, const Vector2D<U>& rhs);

    bool operator == (const Vector2D& lhs)
    {
        return x == lhs.x && y == lhs.y;
    }
    
    bool operator != (const Vector2D& rhs)
    {
        return x != rhs.x || y != rhs.y;
    }

    Vector2D operator +  (const Vector2D& rhs)
    {
        return {x + rhs.x, y + rhs};
    }
    Vector2D operator +  (float add)
    {
        return {x + add, y + add};
    }
    Vector2D operator +  (int add)
    {
        return {x + add, y + add};
    }
    
    Vector2D operator -  (const Vector2D& rhs)
    {
        return {x - rhs.x, y - rhs.y};
    }
    Vector2D<float> operator-(const Vector2D<float>& rhs) const {
        return {x - rhs.x, y - rhs.y};
    }
    
    Vector2D operator -  (float subtract)
    {
        return {x - subtract, y - subtract};
    }
    Vector2D operator -  (int subtract)
    {
        return {x - subtract, y - subtract};
    }
    
    Vector2D operator *  (const Vector2D& rhs)
    {
        return {x * rhs.x, y * rhs.y};
    }
    Vector2D operator *  (T scale)
    {
        return {x * scale, y * scale};
    }
    
    Vector2D operator /  (const Vector2D& rhs)
    {
        return {x / rhs.x, y / rhs.y};
    }
    Vector2D operator /  (float divided)
    {
        return {x / divided, y / divided};
    }
    Vector2D operator /  (int divided)
    {
        return {x / divided, y / divided};
    }

    Vector2D& operator +=  (const Vector2D& rhs)
    {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }
    Vector2D& operator +=  (const float add)
    {
        x += add;
        y += add;
        return *this;
    }
    Vector2D& operator +=  (const int add)
    {
        x += add;
        y += add;
        return *this;
    }
    
    Vector2D& operator -=  (const Vector2D& rhs)
    {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }
    Vector2D& operator -=  (const float subtract)
    {
        x -= subtract;
        y -= subtract;
        return *this;
    }
    Vector2D& operator -=  (const int subtract)
    {
        x -= subtract;
        y -= subtract;
        return *this;
    }

    Vector2D& operator *=  (const Vector2D& rhs)
    {
        x *= rhs.x;
        y *= rhs.y;
        return *this;
    }
    Vector2D& operator *=  (const float scale)
    {
        x *= scale;
        y *= scale;
        return *this;
    }
    Vector2D& operator *=  (const int scale)
    {
        x *= scale;
        y *= scale;
        return *this;
    }
    
    Vector2D& operator /=  (const Vector2D& rhs)
    {
        x /= rhs.x;
        y /= rhs.y;
        return *this;
    }
    void operator /=  (const float divided)
    {
        x /= divided;
        y /= divided;
        return *this;
    }
    void operator /=  (const int divided)
    {
        x /= divided;
        y /= divided;
        return *this;
    }
};