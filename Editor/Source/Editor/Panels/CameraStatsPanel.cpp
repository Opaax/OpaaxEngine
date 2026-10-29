#include "Editor/Panels/CameraStatsPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EditorViewport.hpp"

#include "Application/Services/IEngine.h"
#include "Core/Maths/Maths.h"
#include "Engine/FrameClock.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "World/Entity/Entity.h"
#include "World/World.h"
#include "World/WorldManager.h"

#include <imgui.h>

namespace Opaax::Editor
{
    namespace
    {
        constexpr float k_GraphHeight = 44.f;

        /** How a camera's rhythm reads in the panel. */
        const char* RhythmLabel(const ECameraRhythm InRhythm)
        {
            switch (InRhythm)
            {
                case ECameraRhythm::Still:  return "not moving";
                case ECameraRhythm::Steps:  return "moved in fixed steps";
                case ECameraRhythm::Update: return "moved in Update";
                case ECameraRhythm::Mixed:  return "moved both in fixed steps and in Update";
                default:                    return "measuring how it moves";
            }
        }

        /** InSeries as a graph from 0 to InCeiling, labelled InLabel. */
        void Graph(const char* InId, const TDynArray<float>& InSeries, const float InCeiling, const char* InLabel)
        {
            ImGui::PlotLines(InId, InSeries.data(), static_cast<int>(InSeries.size()), 0, InLabel, 0.f, InCeiling,
                             ImVec2(-1.f, k_GraphHeight));
        }
    }

    CameraStatsPanel::CameraStatsPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    CameraStatsPanel::~CameraStatsPanel() = default;

    void CameraStatsPanel::OnActiveWorldChanged(World* /*InOld*/, World* /*InNew*/)
    {
        m_History.Clear();
        m_Target = ENTITY_NONE;
    }

    void CameraStatsPanel::DrawContents()
    {
        World* const         lWorld   = m_Context.Worlds.GetActiveWorld();
        const CameraManager* lCameras = m_Context.Engine.GetCameras();

        if (lWorld == nullptr || lCameras == nullptr || lWorld->GetMode() != EWorldMode::Play)
        {
            ImGui::TextDisabled("Not playing: the edit world is framed by the editor's camera.");
            ImGui::TextDisabled("Play to see how the game's camera frames it.");
            return;
        }

        const CameraFrame& lFrame = lCameras->GetLastFrame();
        if (lFrame.World != lWorld->GetHandle())
        {
            ImGui::TextDisabled("Waiting for the first frame.");
            return;
        }

        // The selection is the target: a new one starts the graphs over (another entity's drift).
        Entity lTarget = m_Context.Selection.Get();
        if (!lTarget.IsValid() || lTarget.GetWorld() != lWorld)
        {
            lTarget = Entity{};
        }
        if (lTarget.GetHandle() != m_Target)
        {
            m_History.Clear();
            m_Target = lTarget.GetHandle();
        }

        // Measured in the viewport's pixels, where the game is drawn.
        const float lHeightPx = m_Context.Viewport.IsValid() ? m_Context.Viewport.GetSizePx().y : 720.f;
        m_History.Push(CameraManager::SampleFrame(*lWorld, lFrame, lTarget), lHeightPx);

        DrawFrame(*lWorld, lFrame);
        ImGui::Separator();
        DrawClock();
        ImGui::Separator();
        DrawGraphs(*lWorld);
    }

    void CameraStatsPanel::DrawFrame(World& InWorld, const CameraFrame& InFrame)
    {
        if (InFrame.Resolution.Count == 0)
        {
            ImGui::Text("No camera: the default view frames the world.");
        }
        else
        {
            Entity                              lLive{ InFrame.Resolution.Entity, &InWorld };
            const CameraInterpolationComponent* lHistory = lLive.TryGet<CameraInterpolationComponent>();
            const ECameraRhythm                 lRhythm  = lHistory != nullptr ? lHistory->Rhythm.Last : ECameraRhythm::Unknown;

            ImGui::Text("Camera  %s", lLive.GetName().CStr());
            ImGui::SameLine();
            ImGui::TextDisabled("(%u in the world)", InFrame.Resolution.Count);
            if (lRhythm == ECameraRhythm::Mixed)
            {
                ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f), "%s: it jitters on screens faster than the steps (see "
                                   "the log).", RhythmLabel(lRhythm));
            }
            else
            {
                ImGui::TextDisabled("%s", RhythmLabel(lRhythm));
            }
        }

        const CameraView& lView = InFrame.Resolution.View;
        ImGui::Text("View    (%.2f, %.2f)  size %.2f", lView.Position.x, lView.Position.y, lView.OrthoSize);
        ImGui::Text("Frame   %.2f ms   %u step%s   alpha %.3f   %s%s", InFrame.FrameDelta * 1000.0, InFrame.Steps,
                    InFrame.Steps == 1 ? "" : "s", InFrame.Alpha,
                    InFrame.bInterpolated ? "between steps" : "as is", InFrame.bCut ? "   cut" : "");
    }

    void CameraStatsPanel::DrawClock()
    {
        const FrameClock& lClock   = m_Context.Engine.GetFrameClock();
        const double      lRefresh = lClock.GetRefreshInterval();

        ImGui::Text("Steps   %.0f a second, at most %u a frame   %llu dropped", 1.0 / lClock.GetFixedDelta(),
                    lClock.GetMaxStepsPerFrame(), static_cast<unsigned long long>(lClock.GetDroppedSteps()));
        if (lRefresh > 0.0)
        {
            ImGui::Text("Screen  %.1f Hz   %s", 1.0 / lRefresh,
                        lClock.IsSnapping() ? "frame times snapped to it" : "frame times as measured");
        }
        else
        {
            ImGui::Text("Screen  rate unknown: frame times as measured");
        }
    }

    void CameraStatsPanel::DrawGraphs(World& InWorld)
    {
        const TDynArray<float>& lDrift = m_History.GetDrift();

        if (m_Target == ENTITY_NONE)
        {
            ImGui::TextDisabled("Select an entity to graph its drift on screen.");
        }
        else
        {
            const Entity lTarget{ m_Target, &InWorld };
            const float  lLargest = CameraStatsHistory::Largest(lDrift);
            ImGui::Text("Target  %s   drift %.3f px   largest %.3f px", lTarget.GetName().CStr(),
                        lDrift.empty() ? 0.f : lDrift.back(), lLargest);
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("How far it moved on screen from one frame to the next, against the view.\n"
                                  "0 for an entity the camera follows; camera.trace measures the same.");
            }

            // At least a pixel tall, so a locked target draws a flat line at the bottom.
            Graph("##drift", lDrift, Maths::Max(1.f, lLargest * 1.1f), "drift px");
        }

        const TDynArray<float>& lFrameMs = m_History.GetFrameMs();
        Graph("##frame", lFrameMs, Maths::Max(1000.f / 30.f, CameraStatsHistory::Largest(lFrameMs) * 1.1f), "frame ms");

        // A frame that takes 0 or 2 steps at a screen rate equal to the fixed rate shows here.
        Graph("##steps", m_History.GetSteps(), Maths::Max(3.f, CameraStatsHistory::Largest(m_History.GetSteps()) + 1.f),
              "steps");
        Graph("##alpha", m_History.GetAlpha(), 1.f, "alpha");

        ImGui::TextDisabled("Last %llu frames.", static_cast<unsigned long long>(m_History.Count()));
    }
}
