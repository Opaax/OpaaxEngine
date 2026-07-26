#pragma once

#include <json.hpp>
#include "MathTypes.h"

namespace glm
{
    // =============================================================================
    // Vector 3F
    
    inline void to_json(nlohmann::json& Json, const vec3& Vector3)
    {
        Json = {
            {"x", Vector3.x}, 
            {"y", Vector3.y}, 
            {"z", Vector3.z}
        };
    }

    inline void from_json(const nlohmann::json& Json, vec3& Vector3)
    {
        Json.at("x").get_to(Vector3.x);
        Json.at("y").get_to(Vector3.y);
        Json.at("z").get_to(Vector3.z);
    }
    
    // End Vector 3F
    // =============================================================================

    // =============================================================================
    // Vector 4F
    inline void to_json(nlohmann::json& Json, const vec4& Vector4)
    {
        Json = {
            {"x", Vector4.x},
            {"y", Vector4.y},
            {"z", Vector4.z},
            {"w", Vector4.w}
        };
    }

    inline void from_json(const nlohmann::json& Json, vec4& Vector4)
    {
        Json.at("x").get_to(Vector4.x);
        Json.at("y").get_to(Vector4.y);
        Json.at("z").get_to(Vector4.z);
        Json.at("w").get_to(Vector4.w);
    }
    // End Vector 4F
    // =============================================================================
}
