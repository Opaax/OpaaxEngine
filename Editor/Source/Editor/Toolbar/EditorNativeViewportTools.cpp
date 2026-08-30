#include "Editor/Toolbar/EditorNativeViewportTools.h"

#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/ImguiLibrary/ImguiWidgets.h"   // ToggleButton — the mode, snap and grid buttons
#include "Editor/Operation/EditorGizmo.hpp"
#include "Editor/Operation/EditorViewport.hpp"   // the grid toggle lives on the viewport (③b)

#include <imgui.h>

#include <cstdio>   // snprintf — the pivot button's state-carrying label

namespace
{
    /** Below this a snap step collapses every drag onto one point, so the toolbar clamps to it. */
    constexpr float k_MinSnapStep = 0.001f;
}

namespace Opaax::Editor::NativeViewportTools
{
    void DrawGizmoMode(EditorContext& InContext)
    {
        // BY TAG, so this is a THIRD front-end onto the same commands the Edit menu and W/E/R use.
        // Calling EditorGizmo::SetMode directly here would be a fourth place to keep correct.
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
        // The toggle and the STEP together: a toggle over a number you cannot change is half a
        // control, and the step is what an author actually tunes per map.
        EditorGizmo& lGizmo = InContext.Gizmo;

        if (ImguiWidgets::ToggleButton("Snap", lGizmo.IsSnapEnabled()))
        {
            lGizmo.SetSnapEnabled(!lGizmo.IsSnapEnabled());
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Snap the gizmo to fixed steps.\nHold Ctrl to invert this while dragging.");
        }

        // The step for the ACTIVE mode only — three fields at once would be a settings popup,
        // and the one an author wants is always the one they are about to drag with.
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.f);

        const EGizmoMode lMode = lGizmo.GetMode();

        // Degrees for rotate, a fraction for scale, world units otherwise — the format says
        // which, so the number is never ambiguous.
        const char* lFormat = lMode == EGizmoMode::Rotate ? "%.0f deg"
                            : lMode == EGizmoMode::Scale  ? "%.2f x"
                                                          : "%.1f u";

        float& lStep = lGizmo.SnapStepRef(lMode);
        if (ImGui::DragFloat("##step", &lStep, lMode == EGizmoMode::Scale ? 0.01f : 0.5f,
                             0.f, 0.f, lFormat))
        {
            // A zero or negative step would make ImGuizmo snap everything onto one point.
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

    void DrawPivot(EditorContext& InContext)
    {
        // One button that NAMES ITS CURRENT STATE rather than a pair of radio buttons: there are
        // only two values, so the label is the readout and clicking is the toggle.
        EditorGizmo& lGizmo = InContext.Gizmo;

        // THREE states, so it cycles rather than toggles. The label is the readout.
        const EGizmoPivot lPivot = lGizmo.GetPivot();

        // "###pivot" pins the ImGui ID to the part after it, so a label that changes with the
        // state does not make this a different widget every time it is clicked.
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

        // SCALE FORCES LOCAL, so the button says so and refuses rather than lying. A world-axis
        // non-uniform scale of a rotated entity is a SHEAR, which TransformComponent cannot hold.
        const bool bForced = lGizmo.GetMode() == EGizmoMode::Scale;
        const bool bLocal  = lGizmo.GetEffectiveSpace() == EGizmoSpace::Local;

        ImGui::BeginDisabled(bForced);

        if (ImGui::SmallButton(bLocal ? "Space: Local###space" : "Space: World###space"))
        {
            lGizmo.SetSpace(bLocal ? EGizmoSpace::World : EGizmoSpace::Local);
        }

        ImGui::EndDisabled();

        // OUTSIDE BeginDisabled: a disabled item does not report hover, and the one moment the
        // tooltip is most needed is when the button will not move.
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
