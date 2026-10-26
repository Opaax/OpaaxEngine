#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/TypeInfo.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/Object.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ObjectClassRegistry);

    // =============================================================================
    // ObjectClassRegistry — the Object classes a file can name: "Class": "MoveMode" finds its class
    //   here, and editors list what can be made from it. Filled by OPAAX_REGISTER_OBJECT before the
    //   first world, then sealed.
    // =============================================================================
    class ObjectClassRegistry
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        ObjectClassRegistry()  = default;
        ~ObjectClassRegistry() = default;

        ObjectClassRegistry(const ObjectClassRegistry&)            = delete;
        ObjectClassRegistry& operator=(const ObjectClassRegistry&) = delete;

        // =============================================================================
        // Registration
        // =============================================================================
    public:
        /**
         * Registers InClass under its name. Refused (and logged) once sealed, or when its name or type is
         * already taken (two namespaces, one class name).
         */
        bool Register(const ObjectClass& InClass);

        template<CObject T>
        bool Register() { return Register(T::StaticClass()); }

        /**
         * Lets files that still name a class InOldName load as the class registered as InName. Register
         * InName first.
         * @return False when sealed, InName is unknown or InOldName is taken
         */
        bool AddAlias(OpaaxStringID InOldName, OpaaxStringID InName);

        /** Closes the registry. Called before the first world. Safe to call twice. */
        void Seal() noexcept;

        // =============================================================================
        // Lookup
        // =============================================================================
    public:
        /** The class InName names, aliases included, or null. */
        const ObjectClass* FindByName(OpaaxStringID InName) const noexcept;

        const ObjectClass* FindByTypeId(TypeId InTypeId) const noexcept;

        /**
         * Every registered class that is InBase or derives from it, in registration order.
         * @param bInConcreteOnly Leaves out the abstract classes (what an editor offers to make)
         */
        TDynArray<const ObjectClass*> GetClassesOf(const ObjectClass& InBase, bool bInConcreteOnly) const;

        /** Every registered class, in registration order. */
        const TDynArray<const ObjectClass*>& GetClasses() const noexcept { return m_Classes; }

        Uint64 Count()    const noexcept { return static_cast<Uint64>(m_Classes.size()); }
        bool   IsSealed() const noexcept { return m_bSealed; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<const ObjectClass*>             m_Classes;

        /** Names (and aliases) by their id. */
        TUnorderedMap<Uint32, const ObjectClass*> m_ByName;

        bool m_bSealed = false;
    };
}
