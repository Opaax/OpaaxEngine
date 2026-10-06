#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(SpriteAnimation);

    class World;
    struct WorldContext;
    struct SpriteComponent;
    struct SpriteAnimatorComponent;
    struct AnimationClipData;
    struct AnimationLibraryData;
    struct AnimationStep;
    struct SpriteSheetData;

    // Forward-declared for the caches.
    struct AnimationClipResource;
    struct AnimationLibraryResource;
    struct SpriteSheetResource;

    // =============================================================================
    // SpriteAnimationSubsystem — advances every SpriteAnimatorComponent and writes the frame
    //   into its SpriteComponent. Play worlds only (it changes components; the Play copy is
    //   thrown away, so the authored map is untouched).
    // =============================================================================
    class SpriteAnimationSubsystem final : public WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(SpriteAnimationSubsystem)

        /** Play worlds only. */
        static bool ShouldCreate(const World& InWorld);

        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        explicit SpriteAnimationSubsystem(WorldContext& InContext) : m_Context(&InContext) {}

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin ISubsystem interface
        bool Startup()  override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** One entity: resolve, advance, sample, write. */
        void Advance(SpriteComponent& InSprite, SpriteAnimatorComponent& InAnim, float InDelta);

        /**
         * The clip InAnim plays, or nullptr if none.
         * @param OutClipPath The clip's path id (a change restarts the clip)
         */
        const AnimationClipData* ResolveClip(const SpriteAnimatorComponent& InAnim, OpaaxStringID& OutClipPath);

        /** Writes the step's image into the sprite (only changed values). */
        void ApplyStep(SpriteComponent& InSprite, const AnimationClipData& InClip,
                       const AnimationStep& InStep, Uint32 InStepIndex, OpaaxStringID InClipPath);

        /**
         * Step index -> sheet frame index, built once per clip. An unknown frame name gives -1 and
         * warns once (that step is skipped).
         */
        const TDynArray<Int32>* BindFrames(OpaaxStringID InClipPath, const AnimationClipData& InClip);

        // ---- resource caches ----------------------------------------------------------------
        const AnimationLibraryData* ResolveLibrary(const TResourcePath<AnimationLibraryResource>& InPath);
        const AnimationClipData*    ResolveClipData(const OpaaxString& InAssetPath, OpaaxStringID InKey);
        const SpriteSheetData*      ResolveSheet(const TResourcePath<SpriteSheetResource>& InPath);

        /** Asset-relative -> absolute. */
        OpaaxString ToAbsolute(const OpaaxString& InAssetPath) const;

        /** True the first time InKey is seen (log once). */
        bool ShouldWarnOnce(Uint32 InKey);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // owned by the World

        TUnorderedMap<Uint32, ResourceRef<AnimationLibraryResource>> m_LibraryCache;
        TUnorderedMap<Uint32, ResourceRef<AnimationClipResource>>    m_ClipCache;
        TUnorderedMap<Uint32, ResourceRef<SpriteSheetResource>>      m_SheetCache;

        /** Clip path id -> frame index per step. */
        TUnorderedMap<Uint32, TDynArray<Int32>> m_FrameBindings;

        /** Keys already warned about. */
        TUnorderedSet<Uint32> m_Warned;

        /** Number of entities advanced on the last tick. */
        Uint64 m_LastAdvanced = 0;
    };
}
