#include "Animation/SpriteAnimationSubsystem.h"

#include "Application/Services/IPaths.h"
#include "Core/Profiling/Profiler.h"   // OPAAX_STAT_SCOPE
#include "Resources/ResourceManager.h"
#include "Animation/Assets/AnimationClip.h"
#include "Animation/Assets/AnimationLibrary.h"
#include "Renderer/Textures/SpriteSheetResource.h"
#include "Animation/SpriteAnimatorComponent.h"
#include "Renderer/Components/SpriteComponent.h"
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
        // No entities yet: everything is resolved on the first tick.
        return true;
    }

    void SpriteAnimationSubsystem::Update(const double InDeltaTime)
    {
        OPAAX_STAT_SCOPE("SpriteAnimation");

        const float lDelta    = static_cast<float>(InDeltaTime);
        Uint64      lAdvanced = 0;

        m_Context->OwningWorld.Each<SpriteComponent, SpriteAnimatorComponent>(
            [this, lDelta, &lAdvanced](EntityID, SpriteComponent& InSprite, SpriteAnimatorComponent& InAnim)
            {
                Advance(InSprite, InAnim, lDelta);
                ++lAdvanced;
            });

        m_LastAdvanced = lAdvanced;
    }

    void SpriteAnimationSubsystem::Advance(SpriteComponent& InSprite, SpriteAnimatorComponent& InAnim,
                                           const float InDelta)
    {
        OpaaxStringID lClipPath;

        const AnimationClip* lClip = ResolveClip(InAnim, lClipPath);

        // Nothing set, or nothing loadable: the sprite keeps its authored frame.
        if (lClip == nullptr)
        {
            return;
        }

        // A clip change restarts at zero.
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
            ApplyStep(InSprite, *lClip, *lStep, lSample.Step, lClipPath);
        }
    }

    void SpriteAnimationSubsystem::ApplyStep(SpriteComponent& InSprite, const AnimationClip& InClip,
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
                return;   // unknown frame name, already warned
            }

            // Assigned only when changed (a path assignment copies a string).
            if (InSprite.Sheet != InClip.Sheet)
            {
                InSprite.Sheet = InClip.Sheet;
            }

            InSprite.Frame = lFrame;
            return;
        }

        // Texture clip: clear the sheet, since a sheet wins over a texture.
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

    const AnimationClip* SpriteAnimationSubsystem::ResolveClip(const SpriteAnimatorComponent& InAnim,
                                                               OpaaxStringID& OutClipPath)
    {
        // A Library wins when set.
        if (!InAnim.Library.IsEmpty())
        {
            const AnimationLibrary* lLibrary = ResolveLibrary(InAnim.Library);

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
            return ResolveClipAsset(lEntry->Clip, OutClipPath);
        }

        if (InAnim.ClipAsset.IsEmpty())
        {
            return nullptr;   // nothing set: not an error
        }

        OutClipPath = OpaaxStringID(InAnim.ClipAsset.Path);
        return ResolveClipAsset(InAnim.ClipAsset, OutClipPath);
    }

    const TDynArray<Int32>* SpriteAnimationSubsystem::BindFrames(const OpaaxStringID InClipPath,
                                                                 const AnimationClip& InClip)
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

        // Logged either way, with counts.
        if (lResolved == InClip.StepCount())
        {
            OPAAX_LOG(LogSpriteAnimation, Trace, "Clip '{}' -> {} step(s) @ {} fps, all frames bound",
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

    const AnimationLibrary* SpriteAnimationSubsystem::ResolveLibrary(const TAssetRef<AnimationLibrary>& InRef)
    {
        const OpaaxStringID lKey(InRef.Path);

        auto lIt = m_LibraryCache.find(lKey.GetId());

        if (lIt == m_LibraryCache.end())
        {
            // Cached even when loading failed, so a missing file is not retried every frame.
            lIt = m_LibraryCache.emplace(lKey.GetId(), LoadAsset(m_Context->Resources, m_Context->Paths, InRef)).first;

            if (const AnimationLibrary* lLibrary = lIt->second.Get())
            {
                OPAAX_LOG(LogSpriteAnimation, Trace, "Library '{}' -> {} clip name(s)",
                          InRef.Path.CStr(), lLibrary->EntryCount());
            }
            else
            {
                OPAAX_LOG(LogSpriteAnimation, Warn, "Library '{}' did not load — nothing animates from it",
                          InRef.Path.CStr());
            }
        }

        return lIt->second.Get();
    }

    const AnimationClip* SpriteAnimationSubsystem::ResolveClipAsset(const TAssetRef<AnimationClip>& InRef,
                                                                    const OpaaxStringID InKey)
    {
        auto lIt = m_ClipCache.find(InKey.GetId());

        if (lIt == m_ClipCache.end())
        {
            lIt = m_ClipCache.emplace(InKey.GetId(), LoadAsset(m_Context->Resources, m_Context->Paths, InRef)).first;

            if (lIt->second.Get() == nullptr)
            {
                OPAAX_LOG(LogSpriteAnimation, Warn, "Clip '{}' did not load — the sprite keeps its authored frame",
                          InRef.Path.CStr());
            }
        }

        return lIt->second.Get();
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
        // Released here (the ResourceManager is still alive), not in the destructor.
        m_FrameBindings.clear();
        m_SheetCache.clear();
        m_ClipCache.clear();
        m_LibraryCache.clear();
        m_Warned.clear();
    }
}
