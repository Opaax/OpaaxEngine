#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Application/Services/ILogger.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"   // the caches hold Refs BY VALUE
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

    // Only NAMED by the held claims.
    struct AnimationClipResource;
    struct AnimationLibraryResource;
    struct SpriteSheetResource;

    // =============================================================================
    // SpriteAnimationSubsystem — advances every SpriteAnimatorComponent and writes the result
    //   into the SpriteComponent beside it.
    //
    //   THE FIRST ENGINE-OWNED WORLD SUBSYSTEM. Registered by Engine::RegisterNativeWorldSubsystems,
    //   the fourth native route beside components, resource formats and engine subsystems.
    //
    //   PLAY WORLDS ONLY, and that is what makes the whole design safe: it MUTATES authored
    //   components, so in an Edit world it must not exist at all. A PIE clone is a separate world
    //   (WM6), so what it writes is thrown away with the clone and the authored map is untouched.
    //   Authoring preview is the clip panel's job, on its own copy (SS4).
    //
    //   IT ADDS NO RENDER PATH. RendererManager::ResolveSpriteDraw already turns Sheet + Frame
    //   into UVs, so animation is one writer upstream of a reader that already existed.
    // =============================================================================
    class OPAAX_API SpriteAnimationSubsystem final : public WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(SpriteAnimationSubsystem)

        /** Gameplay: Play worlds only. Omitting this would animate the editor's authored entities. */
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
        /** One entity's tick: resolve, advance, sample, write. */
        void Advance(EntityID InEntity, SpriteComponent& InSprite, SpriteAnimatorComponent& InAnim, float InDelta);

        /**
         * The clip InAnim plays this frame, or nullptr when it names none (a real state).
         *
         * @param OutClipPath The resolved clip's interned PATH — the identity a clip SWITCH is
         *   detected on, because it is the one thing both routes share.
         */
        const AnimationClipData* ResolveClip(const SpriteAnimatorComponent& InAnim, OpaaxStringID& OutClipPath);

        /** Write the step's picture into the sprite. Only writes a path when it actually changed. */
        void ApplyStep(SpriteComponent& InSprite, const AnimationClipData& InClip,
                       const AnimationStep& InStep, Uint32 InStepIndex, OpaaxStringID InClipPath);

        /**
         * Step index -> the sheet frame index it draws, built ONCE per clip and cached.
         *
         * This is where a frame NAME becomes an index — never per tick and never per entity. A
         * name the sheet does not have resolves to -1 and warns once, so that step is skipped
         * rather than drawing some other frame (BO4c one level down).
         */
        const TDynArray<Int32>* BindFrames(OpaaxStringID InClipPath, const AnimationClipData& InClip);

        // ---- the three ref caches, RendererManager::ResolveTexture's shape -----------
        const AnimationLibraryData* ResolveLibrary(const TResourcePath<AnimationLibraryResource>& InPath);
        const AnimationClipData*    ResolveClipData(const OpaaxString& InAssetPath, OpaaxStringID InKey);
        const SpriteSheetData*      ResolveSheet(const TResourcePath<SpriteSheetResource>& InPath);

        /** Asset-relative -> absolute, through the context's IPaths. */
        OpaaxString ToAbsolute(const OpaaxString& InAssetPath) const;

        /** True the FIRST time InKey is passed, so a per-frame path logs once and never again. */
        bool ShouldWarnOnce(Uint32 InKey);

        /**
         * Say ONCE that a clip actually MOVED — two different steps, on the same entity, over time.
         *
         * The instrument has to discriminate ([[L15]]) and must not share a failure mode with what
         * it measures ([[L21]]). "Advancing N sprite(s)" prints the same whether the clip runs or
         * is bound-and-frozen; so does "the sprite's frame changed", because the authored Frame is
         * -1 and the FIRST write always differs from it. Only a second, different step proves
         * motion — which is why this watches one entity rather than any write.
         */
        void NoteStepApplied(EntityID InEntity, Uint32 InStepIndex);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        TUnorderedMap<Uint32, ResourceRef<AnimationLibraryResource>> m_LibraryCache;
        TUnorderedMap<Uint32, ResourceRef<AnimationClipResource>>    m_ClipCache;
        TUnorderedMap<Uint32, ResourceRef<SpriteSheetResource>>      m_SheetCache;

        /** Clip path id -> one frame index per step. Cleared with the caches. */
        TUnorderedMap<Uint32, TDynArray<Int32>> m_FrameBindings;

        /** Keys already warned about, so a missing clip does not print 60 lines a second. */
        TUnorderedSet<Uint32> m_Warned;

        /** How many entities were advanced on the last tick — the number the Startup log promises. */
        Uint64 m_LastAdvanced = 0;
        bool   m_bLoggedFirstTick = false;

        /** The one entity NoteStepApplied watches, and the first step it was seen on. */
        EntityID m_ProbeEntity     = ENTITY_NONE;
        Int32    m_ProbeStep       = -1;
        bool     m_bLoggedMotion   = false;
    };
}
