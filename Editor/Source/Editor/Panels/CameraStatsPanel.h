#pragma once

#include "Editor/Camera/CameraStatsHistory.hpp"
#include "Editor/Panels/IEditorPanel.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class World;
    struct CameraFrame;
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // CameraStatsPanel — how the playing world's camera framed the last frames, read-only: the live
    //   camera and its rhythm, the frame's time, steps and alpha, the frame clock, and the selected
    //   entity's drift on screen, with graphs of the last frames. Fed by the samples a camera trace
    //   records (CameraManager::SampleFrame), so it shows what camera.trace measures.
    // =============================================================================
    class CameraStatsPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Camera Stats);

        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit CameraStatsPanel(EditorContext& InContext);
        ~CameraStatsPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        CameraStatsPanel(const CameraStatsPanel&)            = delete;
        CameraStatsPanel& operator=(const CameraStatsPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** The live camera, its rhythm, and the frame drawn. */
        void DrawFrame(World& InWorld, const CameraFrame& InFrame);

        /** The frame clock: fixed rate, the refresh it snaps to, the step cap. */
        void DrawClock();

        /** The target and the graphs of the last frames. */
        void DrawGraphs(World& InWorld);

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup() override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        /** Samples the frame just drawn (panels draw after the engine's frame), then draws. */
        void DrawContents() override;

        /** Nothing to release. */
        void Shutdown() override {}

        /** Starts the graphs over: another world's frames are another run. */
        void OnActiveWorldChanged(World* InOld, World* InNew) override;

        PanelWindowStyle GetWindowStyle() const override { return { { 420.f, 480.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        CameraStatsHistory m_History;

        // Whose drift is graphed: the selection when the frame was sampled.
        EntityID m_Target = ENTITY_NONE;
    };
}
