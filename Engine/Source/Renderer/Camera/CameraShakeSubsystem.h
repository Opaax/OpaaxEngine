#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Object/ObjectPtr.h"
#include "Renderer/Camera/CameraModifier.h"
#include "Renderer/Camera/CameraShake.h"
#include "Resources/Asset/AssetHandle.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldSubsystem.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// A hit that shakes the view, from a behaviour's fixed step:
//
//   if (CameraShakeSubsystem* lShakes = GetWorld().GetSubsystems().GetSubsystem<CameraShakeSubsystem>())
//   {
//       lShakes->Play(LoadAsset(..., HitShake), 1.f, lSeed);   // a TAssetRef<CameraShake> field
//   }
//
// A source that shakes harder the more it is hit: a CameraShakeSourceComponent with bUseTrauma and an
// endless shake, then lShakes->AddTrauma(lSource, 0.4f) on each hit.
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    OPAAX_LOG_CATEGORY(CameraShake);

    class CameraShakeSubsystem;

    // =============================================================================
    // CameraShakePlaying — a shake under way.
    // =============================================================================
    struct CameraShakePlaying
    {
        /** What Stop takes. Never 0. */
        Uint32 Id = 0;

        /** Loaded from its file, or made in code. */
        TAssetHandle<CameraShake>     Asset;
        TStrongObjectPtr<CameraShake> Owned;

        float  Scale = 1.f;
        Uint32 Seed  = 0;

        /** The game time it started, and the one it was stopped at. */
        double            Start = 0.0;
        TOptional<double> StoppedAt;

        /** Played by a CameraShakeSourceComponent: its entity, where it was at the last step, its radii. */
        EntityID Source      = ENTITY_NONE;
        Vector2F SourceAt    = { 0.f, 0.f };
        float    InnerRadius = 0.f;
        float    OuterRadius = 0.f;

        /** A trauma source's: the shake plays at its square. */
        bool         bTrauma = false;
        CameraTrauma Trauma;

        /** The shake, or null when its file did not load. */
        const CameraShake* Get() const { return Owned ? Owned.Get() : Asset.Get(); }
    };

    // =============================================================================
    // CameraShakeModifier — the director's modifier that adds the world's shakes to the view.
    // =============================================================================
    class CameraShakeModifier final : public CameraModifier
    {
        OPAAX_OBJECT(CameraShakeModifier, CameraModifier)

    public:
        /** The subsystem whose shakes it adds: nothing once it is gone. */
        TWeakObjectPtr<CameraShakeSubsystem> Shakes;

        //~Begin CameraModifier interface
        void Modify(const CameraModifierContext& InContext, CameraView& InOutView) const override;
        //~End CameraModifier interface
    };

    // =============================================================================
    // CameraShakeSubsystem — the shakes of a Play world. Each is a function of game time and its seed:
    //   started or stopped in a fixed step, it is stamped with that step's start, so it is drawn the same
    //   at any frame rate; between steps, with the last step's end. A paused world holds it still. Its
    //   modifier (given to the CameraDirector at Startup) adds them to the drawn view: their offsets
    //   along the view's axes, their roll; a source's attenuated by the distance from where it was at the
    //   last step to the view's centre.
    // =============================================================================
    class CameraShakeSubsystem final : public WorldSubsystem
    {
        OPAAX_OBJECT(CameraShakeSubsystem, WorldSubsystem)

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Plays InShake from now at InScale (times its amplitudes), its noise or phase picked by InSeed.
         * @return What Stop takes; 0 when InShake is empty or did not load
         */
        Uint32 Play(const TAssetHandle<CameraShake>& InShake, float InScale = 1.f, Uint32 InSeed = 0);
        Uint32 Play(TStrongObjectPtr<CameraShake> InShake, float InScale = 1.f, Uint32 InSeed = 0);

        /** Loads InShake and plays it. */
        Uint32 Play(const TAssetRef<CameraShake>& InShake, float InScale = 1.f, Uint32 InSeed = 0);

        /** Fades the shake InId out over its BlendOut, from now. */
        void Stop(Uint32 InId);

        /** Fades every shake out. */
        void StopAll();

        /**
         * Plays InSource's CameraShakeSourceComponent: attenuated from where the entity is, until its shake
         * ends or the entity is gone. Played again, the one under way stops.
         * @return What Stop takes; 0 when the entity has no source or its shake did not load
         */
        Uint32 PlaySource(EntityID InSource);

        /** Raises InSource's trauma (a source with bUseTrauma) by InAmount, kept in [0, 1]. */
        void AddTrauma(EntityID InSource, float InAmount);

        /** InSource's trauma now (at the world's stepped time), or 0. */
        float GetTrauma(EntityID InSource) const;

        /** The shakes added to a frame drawn at InGameTime, for a view centred on InCentre. */
        CameraShakeOffset Evaluate(double InGameTime, Vector2F InCentre) const;

        /** The shakes under way, and those over but not yet let go (until no frame can show them). */
        const TDynArray<CameraShakePlaying>& GetPlaying() const noexcept { return m_Playing; }

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin WorldSubsystem interface
        /** Play worlds only, like the director. */
        bool ShouldCreate(const World& InWorld) const override;

        /** After gameplay, so a source spawned or hit this step is seen in it. */
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Presentation + 60; }

        bool Startup() override;
        void Shutdown() override;
        void FixedUpdate(double InFixedDeltaTime) override;
        //~End WorldSubsystem interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Starts InPlaying, stamped with the stepped time. */
        Uint32 Start(CameraShakePlaying InPlaying);

        /** The shake InSource's component plays, or null. */
        CameraShakePlaying* FindSource(EntityID InSource);
        const CameraShakePlaying* FindSource(EntityID InSource) const;

        TDynArray<CameraShakePlaying> m_Playing;

        /** Entities whose source was seen, for bPlayOnStart. */
        TUnorderedSet<Uint32> m_SeenSources;

        TStrongObjectPtr<CameraShakeModifier> m_Modifier;

        Uint32 m_NextId = 1;
    };
}
