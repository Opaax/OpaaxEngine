#include "World/Systems/SpriteAnimationSubsystem.h"

#include "Application/Services/IPaths.h"
#include "Core/Profiling/FrameProfiler.h"   // OPAAX_STAT_SCOPE
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipResource.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryResource.h"
#include "Engine/Subsystems/Resources/Types/SpriteSheet/SpriteSheetResource.h"
#include "World/Components/SpriteAnimatorComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

namespace Opaax
{
    bool SpriteAnimationSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    bool SpriteAnimationSubsystem::Startup()
    {
        // No entities yet — the host spawns them in PostEngineStartup, and a PIE clone is
        // instantiated after CreateWorld (WS7). Everything here resolves on the first tick.
        OPAAX_LOG(LogSpriteAnimation, Info, "SpriteAnimation started (Play world)");
        return true;
    }

    void SpriteAnimationSubsystem::Update(const double InDeltaTime)
    {
        OPAAX_STAT_SCOPE(m_Context->Profiler, "SpriteAnimation");

        const float lDelta    = static_cast<float>(InDeltaTime);
        Uint64      lAdvanced = 0;

        m_Context->OwningWorld.Each<SpriteComponent, SpriteAnimatorComponent>(
            [this, lDelta, &lAdvanced](EntityID InEntity, SpriteComponent& InSprite, SpriteAnimatorComponent& InAnim)
            {
                Advance(InEntity, InSprite, InAnim, lDelta);
                ++lAdvanced;
            });

        m_LastAdvanced = lAdvanced;

        // The SUCCESS branch, once ([[L15]]): a subsystem that only logged failures would be
        // indistinguishable from one that found nothing to do.
        if (!m_bLoggedFirstTick)
        {
            m_bLoggedFirstTick = true;
            OPAAX_LOG(LogSpriteAnimation, Info, "Advancing {} animated sprite(s)", lAdvanced);
        }
    }

    void SpriteAnimationSubsystem::Advance(const EntityID InEntity, SpriteComponent& InSprite,
                                           SpriteAnimatorComponent& InAnim, const float InDelta)
    {
        OpaaxStringID lClipPath;

        const AnimationClipData* lClip = ResolveClip(InAnim, lClipPath);

        // Nothing named, or nothing loadable: the sprite keeps its AUTHORED frame. That is the
        // Placeholder policy's whole point — a broken reference degrades, it does not blank.
        if (lClip == nullptr)
        {
            return;
        }

        // A clip SWITCH restarts at zero, which is what Unity and Godot both do and what a state
        // machine expects: entering Run must not resume Run's old cursor.
        if (InAnim.BoundClipPath != lClipPath)
        {
            InAnim.BoundClipPath = lClipPath;
            InAnim.PlayTime      = 0.f;
        }

        if (InAnim.bPlaying)
        {
            InAnim.PlayTime += InDelta * InAnim.Speed;
        }

        const AnimationSample lSample = SampleClip(*lClip, InAnim.PlayTime);

        if (const AnimationStep* lStep = lClip->StepAt(lSample.Step))
        {
            NoteStepApplied(InEntity, lSample.Step);
            ApplyStep(InSprite, *lClip, *lStep, lSample.Step, lClipPath);
        }
    }

    void SpriteAnimationSubsystem::NoteStepApplied(const EntityID InEntity, const Uint32 InStepIndex)
    {
        if (m_bLoggedMotion)
        {
            return;
        }

        // Watch ONE entity: two sprites are legitimately on different steps at the same instant,
        // so comparing across them would report motion that never happened.
        if (m_ProbeStep < 0)
        {
            m_ProbeEntity = InEntity;
            m_ProbeStep   = static_cast<Int32>(InStepIndex);
            return;
        }

        if (InEntity != m_ProbeEntity || static_cast<Int32>(InStepIndex) == m_ProbeStep)
        {
            return;
        }

        m_bLoggedMotion = true;

        OPAAX_LOG(LogSpriteAnimation, Info, "Playing — step {} -> {} on the watched sprite",
                  m_ProbeStep, InStepIndex);
    }

    void SpriteAnimationSubsystem::ApplyStep(SpriteComponent& InSprite, const AnimationClipData& InClip,
                                             const AnimationStep& InStep, const Uint32 InStepIndex,
                                             const OpaaxStringID InClipPath)
    {
        if (!InClip.Sheet.IsEmpty())
        {
            const TDynArray<Int32>* lBinding = BindFrames(InClipPath, InClip);

            if (lBinding == nullptr || InStepIndex >= lBinding->size())
            {
                return;
            }

            const Int32 lFrame = (*lBinding)[InStepIndex];

            if (lFrame < 0)
            {
                return;   // the name did not resolve; BindFrames already said so, once
            }

            // Assigned only when it CHANGED: a TResourcePath assignment is a string copy, and this
            // runs per animated entity per frame.
            if (InSprite.Sheet != InClip.Sheet)
            {
                InSprite.Sheet = InClip.Sheet;
            }

            InSprite.Frame = lFrame;
            return;
        }

        // Texture-list clip: the step carries its own image, and the sheet must get out of the way
        // because SpriteComponent's Sheet WINS over its Texture (SS3).
        if (InStep.Texture.IsEmpty())
        {
            return;
        }

        if (!InSprite.Sheet.IsEmpty())
        {
            InSprite.Sheet = {};
        }

        if (InSprite.Texture != InStep.Texture)
        {
            InSprite.Texture = InStep.Texture;
        }
    }

    const AnimationClipData* SpriteAnimationSubsystem::ResolveClip(const SpriteAnimatorComponent& InAnim,
                                                                   OpaaxStringID& OutClipPath)
    {
        // A Library WINS when set — SpriteComponent's Sheet-over-Texture rule, one layer over.
        if (!InAnim.Library.IsEmpty())
        {
            const AnimationLibraryData* lLibrary = ResolveLibrary(InAnim.Library);

            if (lLibrary == nullptr)
            {
                return nullptr;
            }

            const AnimationLibraryEntry* lEntry = lLibrary->Find(InAnim.Clip);

            if (lEntry == nullptr || lEntry->Clip.IsEmpty())
            {
                const OpaaxStringID lKey(InAnim.Library.Path);

                if (ShouldWarnOnce(lKey.GetId()))
                {
                    OPAAX_LOG(LogSpriteAnimation, Warn,
                              "Library '{}' has no clip '{}' ({} name(s)) — the sprite keeps its authored frame",
                              InAnim.Library.Path.CStr(),
                              InAnim.Clip.IsValid() ? InAnim.Clip.CStr() : "<default>",
                              lLibrary->EntryCount());
                }

                return nullptr;
            }

            OutClipPath = OpaaxStringID(lEntry->Clip.Path);
            return ResolveClipData(lEntry->Clip.Path, OutClipPath);
        }

        if (InAnim.ClipAsset.IsEmpty())
        {
            return nullptr;   // names nothing — a real state, not an error
        }

        OutClipPath = OpaaxStringID(InAnim.ClipAsset.Path);
        return ResolveClipData(InAnim.ClipAsset.Path, OutClipPath);
    }

    const TDynArray<Int32>* SpriteAnimationSubsystem::BindFrames(const OpaaxStringID InClipPath,
                                                                 const AnimationClipData& InClip)
    {
        const auto lFound = m_FrameBindings.find(InClipPath.GetId());

        if (lFound != m_FrameBindings.end())
        {
            return &lFound->second;
        }

        const SpriteSheetData* lSheet = ResolveSheet(InClip.Sheet);

        TDynArray<Int32> lBinding;
        lBinding.reserve(InClip.StepCount());

        Uint32 lResolved = 0;

        for (const AnimationStep& lStep : InClip.Steps)
        {
            Int32 lIndex = -1;

            if (lSheet != nullptr && lStep.Frame.IsValid())
            {
                for (Uint32 lFrame = 0; lFrame < lSheet->FrameCount(); ++lFrame)
                {
                    if (lSheet->Frames[lFrame].Name == lStep.Frame)
                    {
                        lIndex = static_cast<Int32>(lFrame);
                        break;
                    }
                }
            }

            lResolved += (lIndex >= 0) ? 1u : 0u;
            lBinding.emplace_back(lIndex);
        }

        // Logged whichever way it went, and with NUMBERS: "bound" and "bound but resolved nothing"
        // must not look the same in the log.
        if (lResolved == InClip.StepCount())
        {
            OPAAX_LOG(LogSpriteAnimation, Info, "Clip '{}' -> {} step(s) @ {} fps, all frames bound",
                      InClipPath.CStr(), InClip.StepCount(), InClip.Fps);
        }
        else
        {
            OPAAX_LOG(LogSpriteAnimation, Warn,
                      "Clip '{}' -> {} of {} step(s) bound against sheet '{}' — unbound steps are skipped",
                      InClipPath.CStr(), lResolved, InClip.StepCount(), InClip.Sheet.Path.CStr());
        }

        return &m_FrameBindings.emplace(InClipPath.GetId(), Move(lBinding)).first->second;
    }

    const AnimationLibraryData* SpriteAnimationSubsystem::ResolveLibrary(
        const TResourcePath<AnimationLibraryResource>& InPath)
    {
        const OpaaxStringID lKey(InPath.Path);

        auto lIt = m_LibraryCache.find(lKey.GetId());

        if (lIt == m_LibraryCache.end())
        {
            ResourceRef<AnimationLibraryResource> lRef =
                m_Context->Resources.Load<AnimationLibraryResource>(ToAbsolute(InPath.Path).CStr());

            // Cached even when the load FAILED: the empty ref keeps a missing file from being
            // retried once per entity per frame.
            lIt = m_LibraryCache.emplace(lKey.GetId(), Move(lRef)).first;

            if (lIt->second.IsValid())
            {
                OPAAX_LOG(LogSpriteAnimation, Info, "Library '{}' -> {} clip name(s)",
                          InPath.Path.CStr(), lIt->second.Get()->Data.EntryCount());
            }
            else
            {
                OPAAX_LOG(LogSpriteAnimation, Warn, "Library '{}' did not load — nothing animates from it",
                          InPath.Path.CStr());
            }
        }

        const AnimationLibraryResource* lResource = lIt->second.Get();

        return (lResource != nullptr) ? &lResource->Data : nullptr;
    }

    const AnimationClipData* SpriteAnimationSubsystem::ResolveClipData(const OpaaxString& InAssetPath,
                                                                       const OpaaxStringID InKey)
    {
        auto lIt = m_ClipCache.find(InKey.GetId());

        if (lIt == m_ClipCache.end())
        {
            ResourceRef<AnimationClipResource> lRef =
                m_Context->Resources.Load<AnimationClipResource>(ToAbsolute(InAssetPath).CStr());

            lIt = m_ClipCache.emplace(InKey.GetId(), Move(lRef)).first;

            if (!lIt->second.IsValid())
            {
                OPAAX_LOG(LogSpriteAnimation, Warn, "Clip '{}' did not load — the sprite keeps its authored frame",
                          InAssetPath.CStr());
            }
        }

        const AnimationClipResource* lResource = lIt->second.Get();

        return (lResource != nullptr) ? &lResource->Data : nullptr;
    }

    const SpriteSheetData* SpriteAnimationSubsystem::ResolveSheet(const TResourcePath<SpriteSheetResource>& InPath)
    {
        if (InPath.IsEmpty())
        {
            return nullptr;
        }

        const OpaaxStringID lKey(InPath.Path);

        auto lIt = m_SheetCache.find(lKey.GetId());

        if (lIt == m_SheetCache.end())
        {
            ResourceRef<SpriteSheetResource> lRef =
                m_Context->Resources.Load<SpriteSheetResource>(ToAbsolute(InPath.Path).CStr());

            lIt = m_SheetCache.emplace(lKey.GetId(), Move(lRef)).first;

            if (!lIt->second.IsValid())
            {
                OPAAX_LOG(LogSpriteAnimation, Warn, "Sheet '{}' did not load — no frame name can resolve",
                          InPath.Path.CStr());
            }
        }

        const SpriteSheetResource* lResource = lIt->second.Get();

        return (lResource != nullptr) ? &lResource->Data : nullptr;
    }

    OpaaxString SpriteAnimationSubsystem::ToAbsolute(const OpaaxString& InAssetPath) const
    {
        return m_Context->Paths.AssetToAbsolute(InAssetPath);
    }

    bool SpriteAnimationSubsystem::ShouldWarnOnce(const Uint32 InKey)
    {
        return m_Warned.emplace(InKey).second;
    }

    void SpriteAnimationSubsystem::Shutdown()
    {
        // Released here, not in the destructor: DestroyWorld runs this while the ResourceManager is
        // still alive (WS6/LC3), which is the only moment a claim can be given back cleanly.
        m_FrameBindings.clear();
        m_SheetCache.clear();
        m_ClipCache.clear();
        m_LibraryCache.clear();
        m_Warned.clear();

        OPAAX_LOG(LogSpriteAnimation, Info, "SpriteAnimation shutdown ({} sprite(s) on the last tick)",
                  m_LastAdvanced);
    }
}
