#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Object/ObjectHandle.h"
#include "Renderer/CameraView.h"
#include "Renderer/Camera/CameraTrace.h"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class Entity;
    class World;
    class WorldManager;
    class Config_Engine;
    struct CameraComponent;

    OPAAX_LOG_CATEGORY(CameraManager);

    // =============================================================================
    // CameraResolution — the active world's camera result for this frame.
    // =============================================================================
    struct CameraResolution
    {
        /** The winning camera's view, or the default view when Count is 0. */
        CameraView View;

        /** The view before the director's modifiers (a shake) changed it: View without them. */
        CameraView Unmodified;

        EntityID   Entity = ENTITY_NONE;
        Uint32     Count  = 0;

        /** The cameras at the winning priority: more than one leaves the choice to storage order. */
        Uint32     Tied   = 0;

        /**
         * While the view blends to Entity (CameraDirector): the blends in flight, the camera the newest
         * leaves (ENTITY_NONE: a view held), and how far it is toward Entity, 0 to 1.
         */
        Uint32   Blends      = 0;
        EntityID BlendFrom   = ENTITY_NONE;
        float    BlendWeight = 1.f;
    };

    /** What GuardView did to a view. */
    enum class EViewGuard : Uint8
    {
        Good,          // drawn as it is
        SizeClamped,   // its size brought into CameraComponent's range
        Replaced       // not a number somewhere: the last good view instead
    };

    // =============================================================================
    // CameraFrame — how the active Play world was framed on the last frame drawn: what camera.info
    //   answers and a trace records.
    // =============================================================================
    struct CameraFrame
    {
        ObjectHandle     World;
        CameraResolution Resolution;

        /** The frame's delta, and the fixed steps the world ran since the frame before (in that world). */
        double FrameDelta = 0.0;
        Uint32 Steps      = 0;

        /** The alpha the world was drawn with; 1 when not blended (Render.bInterpolation off). */
        double Alpha         = 1.0;
        bool   bInterpolated = false;

        /** The world's steps so far, the last one's length, and the game time drawn. */
        Uint64 StepCount = 0;
        double StepDelta = 0.0;
        double GameTime  = 0.0;

        /** The frame was a cut (Cut): the camera framing it jumped. */
        bool bCut = false;
    };

    // =============================================================================
    // CameraTraceRecording — a trace being recorded, or kept for a comparison (camera.trace.*).
    // =============================================================================
    struct CameraTraceRecording
    {
        OpaaxString Name;

        /** The entity whose drawn position is recorded beside the view, by name; empty for none. */
        OpaaxString Target;

        /** The height of the view the summary measures pixels in. */
        float HeightPx = 720.f;

        /** The run it was made in, for whoever reads it: the frame pacing and the fixed rate. */
        OpaaxString Pacing;
        float       FixedRate = 60.f;

        TDynArray<CameraTraceSample> Samples;

        /** Frames past MAX_TRACE_FRAMES were not kept. */
        bool bTruncated = false;

        // The world recorded, and the target found in it.
        ObjectHandle World;
        EntityID     TargetEntity = ENTITY_NONE;
    };

    // =============================================================================
    // CameraManager — sets the view of the active Play world from its CameraComponents: the highest
    //   Priority wins (among equals, the first found, with a warning), and its CameraDirector blends
    //   from one camera to the next. Edit worlds use the editor's camera instead. The view is resolved in the render pass, after the frame's fixed steps, from
    //   the camera's display pose and size: it frames a body where the body is drawn, and a zoom
    //   changed in fixed steps moves smoothly. It also records camera traces (camera.trace.*): each
    //   frame's time and view, measured by CameraTrace.
    // =============================================================================
    class CameraManager final : public EngineSubsystem
    {
        OPAAX_OBJECT(CameraManager, EngineSubsystem)

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        CameraManager()                                = default;
        CameraManager(const CameraManager&)            = delete;
        CameraManager& operator=(const CameraManager&) = delete;
        CameraManager(CameraManager&&)                 = delete;
        CameraManager& operator=(CameraManager&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Which camera frames InWorld: the highest priority, the first found among equals. The view
         * is left at the default: ResolveCamera gives it.
         */
        static CameraResolution FindLiveCamera(World& InWorld);

        /**
         * The view InWorld's cameras give: its CameraDirector's frame (a blend between two cameras),
         * or, in a world without one, the live camera's view (FindLiveCamera); no camera gives the
         * default view. Static and pure (testable with a bare World).
         * @param InAlpha Progress through the fixed step: the camera is placed at its display pose
         *   (EntityHierarchy::DisplayTransform), as bodies are drawn, and framed with its display
         *   size (ResolveDisplayOrthoSize). None: at its current pose and size.
         */
        static CameraResolution Resolve(World& InWorld, TOptional<float> InAlpha = NullOpt);

        /**
         * The view one camera gives, framing the world or not: with an alpha, at its display pose and
         * size (as bodies are drawn); without, at its current ones. Resolve frames the world with it,
         * and every other view of a camera (an editor preview, camera.info) comes from it. An entity
         * with no CameraComponent gives the default view.
         */
        static CameraView ResolveCamera(Entity InCamera, TOptional<float> InAlpha = NullOpt);

        /**
         * InOutView made safe to draw: a position or a size that is not a finite number gives
         * InLastGood back; a size out of CameraComponent's range is clamped into it. Pure.
         */
        static EViewGuard GuardView(CameraView& InOutView, const CameraView& InLastGood) noexcept;

        /**
         * InFrame, framed in InWorld, as a camera trace records it, with where InTarget was drawn when
         * it is valid: what traces and the editor's Camera Stats read.
         */
        static CameraTraceSample SampleFrame(World& InWorld, const CameraFrame& InFrame, Entity InTarget);

        /**
         * Has InWorld record every camera's pose and size around every fixed step
         * (TransformInterpolation, CameraInterpolation), so the camera framing the world is framed
         * between its last two steps, as bodies are drawn, from its first frame: one that takes the
         * view over is already recorded. Calling it again does nothing more.
         */
        static void RecordCameraSteps(World& InWorld);

        /**
         * The next frame of InWorld is a cut (a respawn, a scene change): every camera is framed
         * where it is, with nothing in between, a blend under way ends on the live camera, and a
         * trace marks the frame. Call it after placing the cameras; bodies that jumped too are placed
         * with TransformInterpolation::Teleport.
         */
        static void Cut(World& InWorld);

        // =============================================================================
        // Trace
        // =============================================================================
    public:
        /** The most frames a trace keeps: a trace left running stops growing there. */
        static constexpr Uint64 MAX_TRACE_FRAMES = 200000;

        /** The most stopped traces kept for a comparison; the oldest goes first. */
        static constexpr Uint64 MAX_KEPT_TRACES = 64;

        /**
         * Records every frame of the Play world from the next one: the view, its time and the target
         * InSetup names (camera.trace.start). Replaces a recording under way.
         */
        void StartTrace(const CameraTraceRecording& InSetup);

        bool IsTracing() const noexcept { return m_Trace.has_value(); }

        /**
         * Ends the recording, and keeps it under its name for a comparison (FindTrace).
         * @return False when nothing was recorded
         */
        bool StopTrace(CameraTraceRecording& OutTrace);

        /** The trace kept under InName, or null. */
        const CameraTraceRecording* FindTrace(const OpaaxString& InName) const;

        /** The name of the trace being recorded; empty when none. */
        const OpaaxString& GetTraceName() const noexcept;

        /** How the active Play world was framed on the last frame drawn (camera.info). */
        const CameraFrame& GetLastFrame() const noexcept { return m_LastFrame; }

    private:
        /**
         * Logs the result only when it changes (other world, other camera count, other winner).
         */
        void ReportResolution(World& InWorld, const CameraResolution& InResolution);

        /** Logs a view GuardView changed, naming the camera, once until that camera gives a good one. */
        void ReportGuard(World& InWorld, EntityID InCamera, EViewGuard InGuard, const CameraView& InBad);

        /** Adds InFrame, drawn in InWorld, to the trace. */
        void RecordTraceFrame(World& InWorld, const CameraFrame& InFrame);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin Subsystem interface
    public:
        bool Startup()                  override;
        void Shutdown()                 override;
        void Update(double InDeltaTime) override;
        void Render(double InAlpha)     override;
        //~End Subsystem interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldManager*  m_WorldManager = nullptr; // not owned

        /** Render.bInterpolation: off frames the camera at its current pose. Not owned. */
        Config_Engine* m_EngineConfig = nullptr;

        // Last reported result. The handle starts invalid, so every new world reports once.
        ObjectHandle m_ReportedWorld;
        Uint32       m_ReportedCount  = 0;
        EntityID     m_ReportedCamera = ENTITY_NONE;

        /** The frame's delta, from Update, and how the last frame was framed. */
        double      m_FrameDelta = 0.0;
        CameraFrame m_LastFrame;

        /** The last view drawn that was a number, in its world, and the camera last reported by ReportGuard. */
        ObjectHandle m_GoodViewWorld;
        CameraView   m_LastGoodView;
        EntityID     m_GuardedCamera = ENTITY_NONE;
        EViewGuard   m_GuardReported = EViewGuard::Good;

        /** The trace being recorded, and the ones kept by name. */
        TOptional<CameraTraceRecording> m_Trace;
        TDynArray<CameraTraceRecording> m_Traces;
    };
}
