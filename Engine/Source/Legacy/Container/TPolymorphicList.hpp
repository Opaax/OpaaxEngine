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
