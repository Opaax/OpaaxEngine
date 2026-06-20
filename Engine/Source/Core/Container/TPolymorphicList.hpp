#pragma once
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * @class TPolymorphicList
     *
     * Static, insertion-ordered list of polymorphic owning pointers, backed by
     * a Meyers-singleton TDynArray<UniquePtr<TInterface>>. Each TInterface
     * instantiation gets its own storage — distinct derived registries do not
     * collide. Inherit and add domain-specific lookup / dispatch as needed.
     *
     * Threading contract: Register/Clear mutate shared storage with NO synchronization.
     * Call them only during single-threaded startup/shutdown (subsystem Startup, game
     * OnInitialize). GetAll iteration must not overlap a Register/Clear on another thread.
     *
     * DLL HAZARD: the function-local static lives ONCE PER MODULE. This template is not
     * exported, so the engine DLL and a game exe each instantiate their own s_Storage —
     * a Register from the exe is invisible to a GetAll in the DLL. Use this ONLY for
     * registries populated entirely engine-side (ComponentRegistry, AssetTypeRegistry).
     * For a list a game exe contributes to, own it on a subsystem instead (the pattern
     * WorldSubsystem / RenderSubsystem use for IWorldSystem / IOverlayRenderSystem).
     */
    template<typename TInterface>
    class TPolymorphicList
    {
        //-----------------------------------------------------------------------------
        // Types
        //-----------------------------------------------------------------------------
    public:
        using ElementType = UniquePtr<TInterface>;
        using StorageType = TDynArray<ElementType>;

        //-----------------------------------------------------------------------------
        // Statics
        //-----------------------------------------------------------------------------
    protected:
        /***/
        static StorageType& GetStorage()
        {
            static StorageType s_Storage;
            return s_Storage;
        }

    public:
        /***/
        static void Register(UniquePtr<TInterface> InElement)
        {
            GetStorage().push_back(Move(InElement));
        }

        /***/
        static void Clear() { GetStorage().clear(); }

        /***/
        static const StorageType& GetAll() { return GetStorage(); }
    };
}
