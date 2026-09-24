#pragma once

#include "Core/OpaaxTypes.h"
#include <type_traits>

#include "Core/String/OpaaxString.hpp"

// =============================================================================
// TResourcePath<T> — a reference to a resource FILE, carrying the type that loads it.
//
//   The path could have been a bare OpaaxString; the type parameter is the whole point. It is what
//   lets the editor's drop target REFUSE a `.wave` dragged onto a texture field, and what makes the
//   next resource-referencing field — a sound, a font, a wave — cost one line and no editor code:
//   one constrained TPropertyDrawer specialization serves every T (I15, the enum dropdown's shape).
//
//   T is only ever NAMED, never completed: ResourceTypeID::Get<T>() hashes a compiler-generated
//   signature, so a component declaring TResourcePath<TextureResource> needs one forward
//   declaration instead of the whole RHI. Nothing in this header includes the resource system.
//
//   ASSET-RELATIVE, always ("Textures/Hero.png"), which is what IPaths::AbsoluteToAsset produces
//   and AssetToAbsolute consumes (MP8) — or a MOUNT ("/Engine/Textures/T_Checker_64.png") for
//   content the engine ships rather than the project. An absolute path here would bake a build
//   machine's layout into a map file; the mount is what makes engine content referenceable without
//   one.
//
//   Header-only value template: no OPAAX_API (I6), no state, no registry.
// =============================================================================
namespace Opaax
{
    // =============================================================================
    // EResourceLoad — WHEN the thing at the other end of a reference is loaded (⑦-C **K5**).
    //
    //   Unreal's `TObjectPtr` / `TWeakObjectPtr` axis, and the component author picks per field.
    //
    //   IT IS THE TYPE, NEVER A SERIALIZED FLAG, and that is the whole design. Eager-ness is a
    //   property of the CODE that reads the field — a gun cannot stall on its first shot — not of
    //   the value an author typed into it. A bool in the file would let one instance be eager and
    //   another lazy for the same component, which is not a thing anyone means.
    // =============================================================================
    enum class EResourceLoad : Uint8
    {
        /**
         * Loaded when something first RESOLVES it, and not before.
         *
         * The right default, and what every existing field already is: a texture is wanted when a
         * sprite is drawn, and a level holding a thousand of them must not pull them all in to open.
         * A spawner that only ever fires on a trigger wants this — and a SELF-REFERENCING prefab
         * can ONLY be expressed this way, since a hard cycle is refused (see Hard).
         */
        Soft,

        /**
         * Resident before the thing holding it is, and for as long as it lives.
         *
         * `LoadContext::Acquire`'s guarantee: loaded inline with the parent and refcount-chained to
         * it, so nothing has to remember to release it. The case that forced it is a gun naming its
         * bullet prefab — a resolve-on-first-touch cache would pay the file read on the one frame
         * that must not stall.
         *
         * A CYCLE IS REFUSED, loudly, because hard references must stay a DAG. That is not a
         * limitation to work around: a prefab that spawns itself is expressible, as `Soft`.
         */
        Hard,
    };

    template<typename TResource, EResourceLoad TLoad = EResourceLoad::Soft>
    struct TResourcePath
    {
        using ResourceType = TResource;

        /** What the loader does about this field. See EResourceLoad. */
        static constexpr EResourceLoad LoadPolicy = TLoad;

        OpaaxString Path;

        /** Empty is a REAL state — "no texture yet" — and never an error. */
        bool IsEmpty() const noexcept { return Path.IsEmpty(); }

        bool operator==(const TResourcePath& InOther) const noexcept { return Path == InOther.Path; }
        bool operator!=(const TResourcePath& InOther) const noexcept { return !(*this == InOther); }
    };

    /**
     * A reference the loader RESIDENTS before the holder runs — `TResourcePath<T, Hard>`.
     *
     * An alias rather than a distinct type, so everything written against `TResourcePath` — the json
     * bridge, the Inspector's drop target, the fold — serves both without knowing there are two.
     * The call site still reads as its own noun, which is what makes a component's intent legible:
     *
     *     TResourcePath<TextureResource>     Texture;   // soft, loaded when drawn
     *     THardResourcePath<PrefabResource>  Bullet;    // resident before the gun fires
     */
    template<typename TResource>
    using THardResourcePath = TResourcePath<TResource, EResourceLoad::Hard>;

    /**
     * Is T a resource reference the loader must resident up front?
     *
     * Detected from the TYPE, which is what lets a component declare its intent and nothing
     * anywhere hand-maintain a list (**I15**'s shape — the same reason a typed drop target costs
     * no editor code).
     */
    template<typename T>
    struct TIsHardResourcePath : std::false_type {};

    template<typename TResource>
    struct TIsHardResourcePath<TResourcePath<TResource, EResourceLoad::Hard>> : std::true_type {};

    template<typename T>
    inline constexpr bool k_IsHardResourcePath = TIsHardResourcePath<T>::value;
}
