#pragma once

#include <json.hpp>
#include "MathTypes.h"

namespace glm
{
    
    using namespace Opaax;
    
    // =============================================================================
    // U32
    // =============================================================================
    
    // =============================================================================
    // Vector 2u32

    inline void to_json(nlohmann::json& Json, const Vector2u32& Vector2u32)
    {
        Json = {
            {"x", Vector2u32.x},
            {"y", Vector2u32.y}
        };
    }

    inline void from_json(const nlohmann::json& Json, Vector2u32& Vector2u32)
    {
        Json.at("x").get_to(Vector2u32.x);
        Json.at("y").get_to(Vector2u32.y);
    }

    // End Vector 2u32
    // =============================================================================
    
    // =============================================================================
    // FLOAT
    // =============================================================================
    
    // =============================================================================
    // Vector 2F

    inline void to_json(nlohmann::json& Json, const Vector2F& Vector2)
    {
        Json = {
            {"x", Vector2.x},
            {"y", Vector2.y}
        };
    }

    inline void from_json(const nlohmann::json& Json, Vector2F& Vector2)
    {
        Json.at("x").get_to(Vector2.x);
        Json.at("y").get_to(Vector2.y);
    }

    // End Vector 2F
    // =============================================================================

    // =============================================================================
    // Vector 3F

    inline void to_json(nlohmann::json& Json, const Vector3F& Vector3)
    {
        Json = {
            {"x", Vector3.x}, 
            {"y", Vector3.y}, 
            {"z", Vector3.z}
        };
    }

    inline void from_json(const nlohmann::json& Json, Vector3F& Vector3)
    {
        Json.at("x").get_to(Vector3.x);
        Json.at("y").get_to(Vector3.y);
        Json.at("z").get_to(Vector3.z);
    }
    
    // End Vector 3F
    // =============================================================================

    // =============================================================================
    // Vector 4F
    inline void to_json(nlohmann::json& Json, const Vector4F& Vector4)
    {
        Json = {
            {"x", Vector4.x},
            {"y", Vector4.y},
            {"z", Vector4.z},
            {"w", Vector4.w}
        };
    }

    inline void from_json(const nlohmann::json& Json, Vector4F& Vector4)
    {
        Json.at("x").get_to(Vector4.x);
        Json.at("y").get_to(Vector4.y);
        Json.at("z").get_to(Vector4.z);
        Json.at("w").get_to(Vector4.w);
    }
    // End Vector 4F
    // =============================================================================
    
    // =============================================================================
    // DOUBLE
    // =============================================================================
    
    // =============================================================================
    // Vector 2D
    
    inline void to_json(nlohmann::json& Json, const Vector2D& Vector2D)
    {
        Json = {
            {"x", Vector2D.x},
            {"y", Vector2D.y}
        };
    }

    inline void from_json(const nlohmann::json& Json, Vector2D& Vector2D)
    {
        Json.at("x").get_to(Vector2D.x);
        Json.at("y").get_to(Vector2D.y);
    }
    // End Vector 2D
    // =============================================================================
    
    // =============================================================================
    // Vector 3D
    inline void to_json(nlohmann::json& Json, const Vector3D& Vector3D)
    {
        Json = {
            {"x", Vector3D.x},
            {"y", Vector3D.y},
            {"z", Vector3D.z}
        };
    }

    inline void from_json(const nlohmann::json& Json, Vector3D& Vector3D)
    {
        Json.at("x").get_to(Vector3D.x);
        Json.at("y").get_to(Vector3D.y);
        Json.at("z").get_to(Vector3D.z);
    }
    
    // End Vector 3D
    // =============================================================================
    
    // =============================================================================
    // Vector 4D
    inline void to_json(nlohmann::json& Json, const Vector4D& Vector4D)
    {
        Json = {
            {"x", Vector4D.x},
            {"y", Vector4D.y},
            {"z", Vector4D.z},
            {"w", Vector4D.w}
        };
    }

    inline void from_json(const nlohmann::json& Json, Vector4D& Vector4D)
    {
        Json.at("x").get_to(Vector4D.x);
        Json.at("y").get_to(Vector4D.y);
        Json.at("z").get_to(Vector4D.z);
        Json.at("w").get_to(Vector4D.w);
    }
    // End Vector 4D
    // =============================================================================
}
