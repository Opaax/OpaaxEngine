#pragma once

#include "Core/OpaaxTypes.h"
#include "Object/ObjectPtr.h"
#include "Renderer/Camera/CameraBlend.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/CameraModifier.h"
#include "Renderer/CameraView.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    class World;

    // =============================================================================
    // CameraShot — a camera the view is drawn from, and how it blended in over the shots before it.
    // =============================================================================
    struct CameraShot
    {
        EntityID Camera = ENTITY_NONE;

        /** Where the camera was last seen: the shot is drawn frozen there once its entity is gone. */
        CameraView LastView;

        /** The game time its blend started, its length in seconds (0: a cut) and its curve. */
        double       Start    = 0.0;
        float        Duration = 0.f;
        ECameraBlend Curve    = ECameraBlend::Cut;
    };

    // =============================================================================
    // CameraDirector — which camera frames a Play world, and the hand-over from one to the next.
    //   The highest Priority is live (the first found among equals). When another camera becomes live,
    //   the view blends to it over its BlendIn, or cuts to it. The camera left keeps moving while the
    //   view leaves it; a blend interrupted by a third camera goes on under the new one, so the view
    //   never jumps (Cinemachine's blend of a blend); a camera destroyed is held where it was last seen.
    //   A change is stamped on the steps' timeline: made in a fixed step, at that step's start, so it
    //   is drawn the same at any frame rate; made between steps, at the last step's end. A frame draws
    //   the blend at its own game time, so a paused world holds still. CameraManager::Cut ends it.
    //   Then its modifiers (CameraModifier: a shake) change the view, by priority.
    // =============================================================================
    class CameraDirector final : public WorldSubsystem
    {
        OPAAX_OBJECT(CameraDirector, WorldSubsystem)

    public:
        /** The most shots kept: a blend started over this many drops the oldest. */
        static constexpr Uint64 MAX_SHOTS = 8;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Makes the highest-priority camera live, starting a blend (or a cut) to it when it changed,
         * stamped with the world's stepped time. Runs in every fixed step, after the camera rigs, and
         * before every frame is framed (CameraManager), so a change made between steps is caught.
         */
        void Direct();

        /**
         * The view a frame drawn at InAlpha shows: each shot at its display pose (its current pose
         * without an alpha), blended at the frame's game time (GameTimeAt), then changed by the
         * modifiers. Before the first Direct, the live camera alone.
         */
        CameraResolution Frame(TOptional<float> InAlpha) const;

        /** Ends every blend: the live camera alone frames the world from the next frame. */
        void Cut();

        /** The game time a frame of InWorld drawn at InAlpha shows: its stepped time less the rest of its last step. */
        static double GameTimeAt(const World& InWorld, TOptional<float> InAlpha) noexcept;

        // =============================================================================
        // Modifiers
        // =============================================================================
    public:
        /**
         * Gives the director InModifier to run on every frame from the next, by its Priority (among
         * equals, in the order added). A priority changed later counts from the next Direct.
         */
        CameraModifier& AddModifier(TStrongObjectPtr<CameraModifier> InModifier);

        /** A new modifier of class T, run from the next frame. */
        template<typename T>
        requires CDerivedFrom<T, CameraModifier>
        T& AddModifier()
        {
            return static_cast<T&>(AddModifier(TStrongObjectPtr<CameraModifier>(NewObject<T>())));
        }

        /** Stops running InModifier and lets it go. */
        void RemoveModifier(const CameraModifier& InModifier);

        /** The modifiers, in the order they run (as of the last Direct). */
        const TDynArray<TStrongObjectPtr<CameraModifier>>& GetModifiers() const noexcept { return m_Modifiers; }

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** The shots: the first frames the world, each next blends in over those before; the last is live. */
        const TDynArray<CameraShot>& GetShots() const noexcept { return m_Shots; }

        /** The live camera, or ENTITY_NONE. */
        EntityID GetLive() const noexcept { return m_Shots.empty() ? ENTITY_NONE : m_Shots.back().Camera; }

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin WorldSubsystem interface
        /** Play worlds only: the editor's camera frames edit worlds. */
        bool ShouldCreate(const World& InWorld) const override;

        /** After the camera rigs (Presentation), so it sees where this step put the cameras. */
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Presentation + 50; }

        void FixedUpdate(double InFixedDeltaTime) override;
        //~End WorldSubsystem interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Frame without the modifiers: the shots blended. */
        CameraResolution Blend(TOptional<float> InAlpha) const;

        TDynArray<CameraShot> m_Shots;

        /** Sorted by Priority, the lowest first, at every Direct. */
        TDynArray<TStrongObjectPtr<CameraModifier>> m_Modifiers;

        /** Cameras found and tied at the top on the last Direct. */
        Uint32 m_Count = 0;
        Uint32 m_Tied  = 0;

        /** Said once: blends started faster than they end. */
        bool m_bWarnedShots = false;
    };
}
