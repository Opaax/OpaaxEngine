#include "Editor/Toolbar/EditorNativeViewportTools.h"

#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"   // ToggleButton
#include "Editor/Operation/EditorGizmo.hpp"
#include "Editor/Operation/EditorViewport.hpp"   // the grid toggle

#include "Application/Services/IEngine.h"   // GetDebugDraw
#include "Renderer/DebugDraw.h"

#include <imgui.h>

#include <cstdio>   // snprintf

namespace
{
    /** Minimum snap step (below it every drag collapses onto one point). */
    constexpr float k_MinSnapStep = 0.001f;
}

namespace Opaax::Editor::NativeViewportTools
{
    void DrawGizmoMode(EditorContext& InContext)
    {
        // Dispatched by tag: the same commands as the Edit menu and W/E/R.
        struct ModeEntry { const char* Label; EGizmoMode Mode; const OpaaxTag& Command; };

        const ModeEntry lModes[] = {
            { "Move",   EGizmoMode::Translate, Tags::EDITOR_COMMAND_GIZMO_TRANSLATE },
            { "Rotate", EGizmoMode::Rotate,    Tags::EDITOR_COMMAND_GIZMO_ROTATE },
            { "Scale",  EGizmoMode::Scale,     Tags::EDITOR_COMMAND_GIZMO_SCALE },
        };

        bool bFirst = true;
        for (const ModeEntry& lEntry : lModes)
        {
            if (!bFirst) { ImGui::SameLine(); }
            bFirst = false;

            if (ImguiWidgets::ToggleButton(lEntry.Label, InContext.Gizmo.GetMode() == lEntry.Mode))
            {
                InContext.Extensions.Commands().Execute(lEntry.Command, InContext);
            }
        }
    }

    void DrawSnap(EditorContext& InContext)
    {
        // The toggle and its step together.
        EditorGizmo& lGizmo = InContext.Gizmo;

        if (ImguiWidgets::ToggleButton("Snap", lGizmo.IsSnapEnabled()))
        {
            lGizmo.SetSnapEnabled(!lGizmo.IsSnapEnabled());
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Snap the gizmo to fixed steps.\nHold Ctrl to invert this while dragging.");
        }

        // The step of the active mode only.
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.f);

        const EGizmoMode lMode = lGizmo.GetMode();

        // Degrees for rotate, a fraction for scale, world units otherwise.
        const char* lFormat = lMode == EGizmoMode::Rotate ? "%.0f deg"
                            : lMode == EGizmoMode::Scale  ? "%.2f x"
                                                          : "%.1f u";

        float& lStep = lGizmo.SnapStepRef(lMode);
        if (ImGui::DragFloat("##step", &lStep, lMode == EGizmoMode::Scale ? 0.01f : 0.5f,
                             0.f, 0.f, lFormat))
        {
            // A zero or negative step would snap everything onto one point.
            lStep = lStep < k_MinSnapStep ? k_MinSnapStep : lStep;
        }
    }

    void DrawGrid(EditorContext& InContext)
    {
        EditorViewport& lViewport = InContext.Viewport;

        if (ImguiWidgets::ToggleButton("Grid", lViewport.IsGridVisible()))
        {
            lViewport.SetShowGrid(!lViewport.IsGridVisible());
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Show a grid at the TRANSLATE snap step.\n"
                              "It coarsens by decades as you zoom out.");
        }
    }

    void DrawColliders(EditorContext& InContext)
    {
        // The state is the engine's DebugDraw, not EditorViewport: the collider outlines also exist in a
        // dev Game.exe.
        DebugDraw& lDebug = InContext.Engine.GetDebugDraw();

        const bool bVisible = lDebug.IsChannelEnabled(DebugChannels::Physics);

        if (ImguiWidgets::ToggleButton("Colliders", bVisible))
        {
            lDebug.SetChannelEnabled(DebugChannels::Physics, !bVisible);
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Outline every ColliderComponent.\n"
                              "Green blocks, yellow passes through.\n"
                              "Drawn while editing, not only while playing.");
        }
    }

    void DrawPivot(EditorContext& InContext)
    {
        // One button naming its current state (two values: the label shows it, a click toggles it).
        EditorGizmo& lGizmo = InContext.Gizmo;

        // Three states, so it cycles. The label shows the current one.
        const EGizmoPivot lPivot = lGizmo.GetPivot();

        // "###pivot" keeps the ImGui ID stable while the label changes.
        char lLabel[48];
        std::snprintf(lLabel, sizeof(lLabel), "Pivot: %s###pivot", ToString(lPivot));

        if (ImGui::SmallButton(lLabel))
        {
            lGizmo.SetPivot(lPivot == EGizmoPivot::Center     ? EGizmoPivot::Origin
                          : lPivot == EGizmoPivot::Origin     ? EGizmoPivot::Individual
                                                              : EGizmoPivot::Center);
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Center — one point, the selection's combined bounds.\n"
                              "Origin — one point, the last-picked entity's position.\n"
                              "Individual — each entity turns about ITSELF; nothing orbits.\n"
                              "All three agree for a single entity.");
        }
    }

    void DrawSpace(EditorContext& InContext)
    {
        EditorGizmo& lGizmo = InContext.Gizmo;

        // Scale forces local (a world-axis scale of a rotated entity would be a shear), so the button is disabled.
        const bool bForced = lGizmo.GetMode() == EGizmoMode::Scale;
        const bool bLocal  = lGizmo.GetEffectiveSpace() == EGizmoSpace::Local;

        ImGui::BeginDisabled(bForced);

        if (ImGui::SmallButton(bLocal ? "Space: Local###space" : "Space: World###space"))
        {
            lGizmo.SetSpace(bLocal ? EGizmoSpace::World : EGizmoSpace::Local);
        }

        ImGui::EndDisabled();

        // Outside BeginDisabled: a disabled item reports no hover, and the tooltip explains why it is disabled.
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            ImGui::SetTooltip(bForced
                ? "Scale is always Local — scaling a rotated entity along world axes\n"
                  "is a shear, which a transform cannot represent."
                : "Local — handles follow the last-picked entity's rotation.\n"
                  "World — handles stay axis-aligned.");
        }
    }
}
