#include "Renderer/Camera/CameraManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Engine/Config/Config_Engine.h"

#include "Renderer/CameraView.h"

#include "World/World.h"
#include "World/WorldManager.h"
#include "Renderer/Camera/CameraComponent.h"
#include "Renderer/Camera/CameraInterpolation.h"
#include "Renderer/Camera/CameraInterpolationComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Entity/EntityMeta.h"
#include "World/Entity/TransformInterpolation.h"

namespace Opaax
{
    bool CameraManager::Startup()
    {
        m_WorldManager = &OpaaxApplication::GetAppService<IEngine>().GetWorldManager();
        m_EngineConfig = &OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>();

        return true;
    }

    void CameraManager::Shutdown()
    {
        m_WorldManager = nullptr;
        m_EngineConfig = nullptr;
    }

    // =========================================================================
    // Resolve — the highest priority wins, the first found among equals; no camera gives the
    //   default view.
    // =========================================================================
    CameraResolution CameraManager::Resolve(World& InWorld, const TOptional<float> InAlpha)
    {
        CameraResolution lResolution;
        Int32            lBestPriority = 0;

        // Priorities only: the pose is computed once, for the winner.
        InWorld.Each<TransformComponent, CameraComponent>(
            [&](EntityID InEntity, TransformComponent&, CameraComponent& InCamera)
            {
                ++lResolution.Count;

                if (lResolution.Count > 1 && InCamera.Priority == lBestPriority)
                {
                    ++lResolution.Tied;
                    return;
                }

                if (lResolution.Count == 1 || InCamera.Priority > lBestPriority)
                {
                    lResolution.Entity = InEntity;
                    lResolution.Tied   = 1;
                    lBestPriority      = InCamera.Priority;
                }
            });

        if (lResolution.Count == 0)
        {
            return lResolution;
        }

        lResolution.View = ResolveCamera(Entity{ lResolution.Entity, &InWorld }, InAlpha);
        return lResolution;
    }

    // =========================================================================
    // ResolveCamera — the world transform: a camera parented to the player follows it. With an
    //   alpha, its pose and its size are blended between its last two steps.
    // =========================================================================
    CameraView CameraManager::ResolveCamera(Entity InCamera, const TOptional<float> InAlpha)
    {
        const CameraComponent* lCamera = InCamera.IsValid() ? InCamera.TryGet<CameraComponent>() : nullptr;
        if (lCamera == nullptr)
        {
            return CameraView{};
        }

        if (!InAlpha.has_value())
        {
            return CameraView{ EntityHierarchy::WorldTransform(InCamera).Position, lCamera->OrthoSize };
        }

        return CameraView{ EntityHierarchy::DisplayTransform(InCamera, *InAlpha).Position,
                           ResolveDisplayOrthoSize(lCamera->OrthoSize, InCamera.TryGet<CameraInterpolationComponent>(), *InAlpha) };
    }

    void CameraManager::RecordCameraSteps(World& InWorld)
    {
        // Gathered first: recording adds components while the cameras are walked.
        TDynArray<EntityID> lCameras;
        InWorld.Each<TransformComponent, CameraComponent>([&lCameras](const EntityID InEntity, TransformComponent&, CameraComponent&)
        {
            lCameras.push_back(InEntity);
        });

        for (const EntityID lCamera : lCameras)
        {
            TransformInterpolation::RecordSteps(Entity{ lCamera, &InWorld });
            CameraInterpolation::RecordSteps(Entity{ lCamera, &InWorld });
        }
    }

    // =========================================================================
    // Update — after the world's update (this subsystem is added after WorldManager), before the
    //   fixed steps. Every camera is recorded, so one that takes the view over is framed between its
    //   steps from its first frame, as the bodies it frames are.
    // =========================================================================
    void CameraManager::Update(const double InDeltaTime)
    {
        m_FrameDelta = InDeltaTime;

        World* lWorld = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;

        if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Play)
        {
            return;
        }

        RecordCameraSteps(*lWorld);
    }

    // =========================================================================
    // Render — before the renderer draws (it is added after this subsystem), after the fixed steps.
    //   Edit worlds are skipped: the editor's camera sets their view.
    // =========================================================================
    void CameraManager::Render(const double InAlpha)
    {
        World* lWorld = (m_WorldManager != nullptr) ? m_WorldManager->GetActiveWorld() : nullptr;

        if (lWorld == nullptr || lWorld->GetMode() != EWorldMode::Play)
        {
            return;
        }

        // Interpolation off: the camera is where it is, like the bodies.
        const bool lInterpolate = m_EngineConfig == nullptr || m_EngineConfig->GetData().Render.bInterpolation;

        const CameraResolution lResolution = lInterpolate ? Resolve(*lWorld, static_cast<float>(InAlpha))
                                                          : Resolve(*lWorld);

        // Always written, so removing the last camera resets to the default view.
        lWorld->SetCameraView(lResolution.View);

        ReportResolution(*lWorld, lResolution);

        // Blended, the world is drawn between its last step's start and its end; else at its end.
        CameraFrame lFrame;
        lFrame.World         = lWorld->GetHandle();
        lFrame.Resolution    = lResolution;
        lFrame.FrameDelta    = m_FrameDelta;
        lFrame.StepCount     = lWorld->GetStepCount();
        lFrame.Steps         = static_cast<Uint32>(lFrame.World == m_LastFrame.World
                                                       ? lFrame.StepCount - m_LastFrame.StepCount
                                                       : lFrame.StepCount);
        lFrame.Alpha         = lInterpolate ? InAlpha : 1.0;
        lFrame.bInterpolated = lInterpolate;
        lFrame.StepDelta     = lWorld->GetLastStepDelta();
        lFrame.GameTime      = lWorld->GetSteppedTime() - (1.0 - lFrame.Alpha) * lFrame.StepDelta;
        m_LastFrame          = lFrame;

        if (m_Trace.has_value())
        {
            RecordTraceFrame(*lWorld, m_LastFrame);
        }
    }

    // =========================================================================
    // Trace
    // =========================================================================
    void CameraManager::StartTrace(const CameraTraceRecording& InSetup)
    {
        m_Trace = InSetup;
        m_Trace->Samples.clear();
        m_Trace->bTruncated   = false;
        m_Trace->World        = ObjectHandle();
        m_Trace->TargetEntity = ENTITY_NONE;

        OPAAX_LOG(LogCameraManager, Info, "Camera trace '{}' started (target '{}')", InSetup.Name.CStr(),
                  InSetup.Target.CStr());
    }

    bool CameraManager::StopTrace(CameraTraceRecording& OutTrace)
    {
        if (!m_Trace.has_value())
        {
            return false;
        }

        OutTrace = Move(*m_Trace);
        m_Trace.reset();

        // A trace of the same name is replaced.
        for (Uint64 lIndex = 0; lIndex < m_Traces.size(); ++lIndex)
        {
            if (m_Traces[lIndex].Name == OutTrace.Name)
            {
                m_Traces.erase(m_Traces.begin() + static_cast<Int64>(lIndex));
                break;
            }
        }
        if (m_Traces.size() >= MAX_KEPT_TRACES)
        {
            m_Traces.erase(m_Traces.begin());
        }
        m_Traces.push_back(OutTrace);

        OPAAX_LOG(LogCameraManager, Info, "Camera trace '{}' stopped: {} frame(s)", OutTrace.Name.CStr(),
                  OutTrace.Samples.size());
        return true;
    }

    const OpaaxString& CameraManager::GetTraceName() const noexcept
    {
        static const OpaaxString s_None;
        return m_Trace.has_value() ? m_Trace->Name : s_None;
    }

    const CameraTraceRecording* CameraManager::FindTrace(const OpaaxString& InName) const
    {
        for (const CameraTraceRecording& lTrace : m_Traces)
        {
            if (lTrace.Name == InName)
            {
                return &lTrace;
            }
        }
        return nullptr;
    }

    void CameraManager::RecordTraceFrame(World& InWorld, const CameraFrame& InFrame)
    {
        CameraTraceRecording& lTrace = *m_Trace;

        // A new world (a level opened): its target is found again.
        if (lTrace.World != InWorld.GetHandle())
        {
            lTrace.World        = InWorld.GetHandle();
            lTrace.TargetEntity = ENTITY_NONE;
        }

        if (lTrace.Samples.size() >= MAX_TRACE_FRAMES)
        {
            if (!lTrace.bTruncated)
            {
                lTrace.bTruncated = true;
                OPAAX_LOG(LogCameraManager, Warn, "Camera trace '{}' keeps its first {} frames: stop it sooner",
                          lTrace.Name.CStr(), MAX_TRACE_FRAMES);
            }
            return;
        }

        CameraTraceSample lSample;
        lSample.FrameDelta   = InFrame.FrameDelta;
        lSample.Steps        = InFrame.Steps;
        lSample.Alpha        = InFrame.Alpha;
        lSample.StepDelta    = InFrame.StepDelta;
        lSample.GameTime     = InFrame.GameTime;
        lSample.ViewPosition = InFrame.Resolution.View.Position;
        lSample.ViewSize     = InFrame.Resolution.View.OrthoSize;

        if (InFrame.Resolution.Count > 0)
        {
            const EntityMeta* lMeta = Entity(InFrame.Resolution.Entity, &InWorld).TryGet<EntityMeta>();
            lSample.Camera = lMeta != nullptr ? lMeta->Name : OpaaxString("<unnamed>");
        }

        // The target where the renderer draws it.
        Entity lTarget(lTrace.TargetEntity, &InWorld);
        if (!lTrace.Target.IsEmpty() && !lTarget.IsValid())
        {
            InWorld.Each<EntityMeta>([&](const EntityID InEntity, const EntityMeta& InMeta)
            {
                if (lTrace.TargetEntity == ENTITY_NONE && InMeta.Name == lTrace.Target)
                {
                    lTrace.TargetEntity = InEntity;
                }
            });
            lTarget = Entity(lTrace.TargetEntity, &InWorld);
        }

        if (lTarget.IsValid() && lTarget.Has<TransformComponent>())
        {
            const TransformComponent lDrawn = InFrame.bInterpolated
                                                  ? EntityHierarchy::DisplayTransform(lTarget, static_cast<float>(InFrame.Alpha))
                                                  : EntityHierarchy::WorldTransform(lTarget);

            lSample.bHasTarget     = true;
            lSample.TargetPosition = lDrawn.Position;
        }

        lTrace.Samples.push_back(Move(lSample));
    }

    void CameraManager::ReportResolution(World& InWorld, const CameraResolution& InResolution)
    {
        if (InWorld.GetHandle() == m_ReportedWorld && InResolution.Count == m_ReportedCount
            && InResolution.Entity == m_ReportedCamera)
        {
            return;
        }

        m_ReportedWorld  = InWorld.GetHandle();
        m_ReportedCount  = InResolution.Count;
        m_ReportedCamera = InResolution.Entity;

        if (InResolution.Count == 0)
        {
            OPAAX_LOG(LogCameraManager, Warn, "Play world '{}' has no CameraComponent — framing with the default centred view",
                      InWorld.GetName().CStr());
            return;
        }

        Entity            lEntity = Entity(InResolution.Entity, &InWorld);
        const EntityMeta* lMeta   = lEntity.TryGet<EntityMeta>();

        OPAAX_LOG(LogCameraManager, Info, "Active camera in '{}': entity '{}' at ({}, {}), orthoSize {}",
                  InWorld.GetName().CStr(),
                  lMeta != nullptr ? lMeta->Name.CStr() : "<unnamed>",
                  InResolution.View.Position.x, InResolution.View.Position.y, InResolution.View.OrthoSize);

        if (InResolution.Tied > 1)
        {
            OPAAX_LOG(LogCameraManager, Warn, "World '{}': {} cameras share the highest Priority — the first found is "
                                              "used; raise the one that should frame the world",
                      InWorld.GetName().CStr(), InResolution.Tied);
        }
    }
}
