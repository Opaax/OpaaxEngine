#include "GroundMoveParams.h"

#if OPAAX_WITH_EDITOR
#include <imgui.h>
#endif

namespace Opaax
{
    nlohmann::json GroundMoveParams::ToJson() const
    {
        return {
            { "gravity_scale",       GravityScale },
            { "max_speed",           MaxSpeed },
            { "acceleration",        Acceleration },
            { "ground_deceleration", GroundDeceleration },
            { "stop_speed",          StopSpeed },
            { "min_speed",           MinSpeed },
            { "air_steer",           AirSteer },
            { "jump_speed",          JumpSpeed }
        };
    }

    void GroundMoveParams::FromJson(const nlohmann::json& InJson)
    {
        if (InJson.contains("gravity_scale"))       { GravityScale       = InJson["gravity_scale"].get<float>(); }
        if (InJson.contains("max_speed"))           { MaxSpeed           = InJson["max_speed"].get<float>(); }
        if (InJson.contains("acceleration"))        { Acceleration       = InJson["acceleration"].get<float>(); }
        if (InJson.contains("ground_deceleration")) { GroundDeceleration = InJson["ground_deceleration"].get<float>(); }
        if (InJson.contains("stop_speed"))          { StopSpeed          = InJson["stop_speed"].get<float>(); }
        if (InJson.contains("min_speed"))           { MinSpeed           = InJson["min_speed"].get<float>(); }
        if (InJson.contains("air_steer"))           { AirSteer           = InJson["air_steer"].get<float>(); }
        if (InJson.contains("jump_speed"))          { JumpSpeed          = InJson["jump_speed"].get<float>(); }
    }

    void GroundMoveParams::DrawEditor()
    {
#if OPAAX_WITH_EDITOR
        ImGui::DragFloat("Gravity Scale",       &GravityScale,       0.05f, 0.f, 0.f, "%.2f");
        ImGui::DragFloat("Max Speed",           &MaxSpeed,           1.f,   0.f, 0.f, "%.0f");
        ImGui::DragFloat("Acceleration",        &Acceleration,       0.1f,  0.f, 0.f, "%.1f");
        ImGui::DragFloat("Ground Deceleration", &GroundDeceleration, 0.1f,  0.f, 0.f, "%.1f");
        ImGui::DragFloat("Stop Speed",          &StopSpeed,          1.f,   0.f, 0.f, "%.0f");
        ImGui::DragFloat("Min Speed",           &MinSpeed,           1.f,   0.f, 0.f, "%.0f");
        ImGui::DragFloat("Air Steer",           &AirSteer,           0.01f, 0.f, 1.f, "%.2f");
        ImGui::DragFloat("Jump Speed",          &JumpSpeed,          1.f,   0.f, 0.f, "%.0f");
#endif
    }

} // namespace Opaax
