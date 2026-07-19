#include "ConfigPanel.h"

#if OPAAX_WITH_EDITOR

#include <cstdio>
#include <imgui.h>

#include "Core/Config/EngineConfig.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax::Editor
{
    namespace
    {
        // Read-only "key : value" row with an optional trailing greyed hint (e.g. "restart").
        void InfoRow(const char* InLabel, const char* InValue, const char* InHint = nullptr)
        {
            ImGui::TextDisabled("%s", InLabel);
            ImGui::SameLine(180.f);
            ImGui::TextUnformatted(InValue);
            if (InHint)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)", InHint);
            }
        }
    }

    void ConfigPanel::Draw()
    {
        ImGui::Begin("Config");

        // -------------------------------------------------------------------------
        // Editable — values the engine re-reads live, so edits apply immediately.
        // -------------------------------------------------------------------------
        ImGui::SeparatorText("Runtime (live)");

        bool lInterp = EngineConfig::RenderInterpolation();
        if (ImGui::Checkbox("Render interpolation", &lInterp))
        {
            EngineConfig::SetRenderInterpolation(lInterp);
            OPAAX_CORE_INFO("ConfigPanel — render.interpolation -> {}", lInterp ? "true" : "false");
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Lerp physics-driven entities between fixed steps (smooth motion >60 Hz).\n"
                              "Off = pixel-locked at the raw fixed-step pose. Takes effect immediately.");
        }

        // -------------------------------------------------------------------------
        // Read-only — consumed once at startup, shown as info.
        // -------------------------------------------------------------------------
        ImGui::Dummy(ImVec2(0.f, 6.f));
        ImGui::SeparatorText("Startup / build (read-only)");

        InfoRow("Render backend",  EngineConfig::RenderBackend().CStr(),  "restart");
        InfoRow("Physics backend", EngineConfig::PhysicsBackend().CStr(), "restart");

        char lBuf[128];

        ImGui::Dummy(ImVec2(0.f, 4.f));
        InfoRow("Window title", EngineConfig::WindowTitle().CStr(), "restart");
        std::snprintf(lBuf, sizeof(lBuf), "%u x %u",
                      EngineConfig::WindowWidth(), EngineConfig::WindowHeight());
        InfoRow("Window size", lBuf, "restart");

        ImGui::Dummy(ImVec2(0.f, 4.f));
        InfoRow("Log level",      EngineConfig::LogLevel().CStr(),          "restart");
        InfoRow("Engine assets",  EngineConfig::EngineAssetsRoot().CStr());
        InfoRow("Asset manifest", EngineConfig::EngineManifestRelPath().CStr());

        ImGui::Dummy(ImVec2(0.f, 4.f));
        InfoRow("World bounds", EngineConfig::PhysicsWorldBoundsEnabled() ? "enabled" : "disabled",
                "applied at startup");
        const Vector2F lMin = EngineConfig::PhysicsWorldBoundsMin();
        const Vector2F lMax = EngineConfig::PhysicsWorldBoundsMax();
        std::snprintf(lBuf, sizeof(lBuf), "[%.0f, %.0f]  ->  [%.0f, %.0f]", lMin.x, lMin.y, lMax.x, lMax.y);
        InfoRow("  bounds min/max", lBuf);
        InfoRow("  bounds response", EngineConfig::PhysicsWorldBoundsResponse().CStr());

        // -------------------------------------------------------------------------
        // Persist — write the current values back to the loaded config file.
        // -------------------------------------------------------------------------
        ImGui::Dummy(ImVec2(0.f, 8.f));
        ImGui::Separator();

        if (ImGui::Button("Save to disk"))
        {
            if (EngineConfig::Save())
            {
                OPAAX_CORE_INFO("ConfigPanel — saved config to '{}'.", EngineConfig::LoadedPath());
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", EngineConfig::LoadedPath().CStr());

        ImGui::End();
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
