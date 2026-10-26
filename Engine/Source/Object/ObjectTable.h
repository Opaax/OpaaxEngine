#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Object/ObjectHandle.h"

namespace Opaax
{
    class Object;

    // =============================================================================
    // ObjectTable — every live Object, by slot: its address, the slot's generation, its flags and its
    //   strong references. Handles resolve through it, so a handle to a destroyed object reads null.
    //   One table for the process, like the name table: an object can be made on a loading thread and
    //   a handle resolved anywhere. Adding and removing are thread-safe; a resolved pointer stays valid
    //   only while something keeps its object alive (destroy objects on the main thread).
    // =============================================================================
    class ObjectTable
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        ObjectTable()  = default;
        ~ObjectTable() = default;

        ObjectTable(const ObjectTable&)            = delete;
        ObjectTable& operator=(const ObjectTable&) = delete;

        /** The process's table. Never destroyed, so objects with static lifetime can leave it at exit. */
        static ObjectTable& Get();

        // =============================================================================
        // Slots
        // =============================================================================
    public:
        /**
         * Gives InObject a slot. Called by Object's constructor.
         * @return Its handle (a reused slot gets the next generation)
         */
        ObjectHandle Add(Object& InObject);

        /** Frees InHandle's slot. Called by Object's destructor. A stale handle does nothing. */
        void Remove(ObjectHandle InHandle);

        /**
         * The object InHandle names.
         * @return Null when the handle is invalid or stale, or the object's destruction has begun
         */
        Object* Resolve(ObjectHandle InHandle) const;

        // =============================================================================
        // Flags
        // =============================================================================
    public:
        /** @return None for a stale handle */
        EObjectFlags GetFlags(ObjectHandle InHandle) const;

        void AddFlags(ObjectHandle InHandle, EObjectFlags InFlags);
        void RemoveFlags(ObjectHandle InHandle, EObjectFlags InFlags);

        // =============================================================================
        // Strong references (TStrongObjectPtr)
        // =============================================================================
    public:
        /**
         * Counts one more owner of a Managed object.
         * @return False when the handle is stale or the object is not Managed (nothing counted)
         */
        bool AddStrongRef(ObjectHandle InHandle);

        /**
         * Drops one owner. The last one destroys the object (outside the table's lock, so its destructor
         * may make and destroy objects).
         */
        void ReleaseStrongRef(ObjectHandle InHandle);

        /** @return 0 for a stale handle */
        Uint32 GetStrongRefCount(ObjectHandle InHandle) const;

        // =============================================================================
        // Queries
        // =============================================================================
    public:
        /** The number of live objects. */
        Uint32 Count() const;

        /**
         * The handle of every live object, in slot order. Resolve each one before use: an object can be
         * gone by then.
         */
        TDynArray<ObjectHandle> Snapshot() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct Slot
        {
            Object*      Address    = nullptr;
            Uint32       Generation = 0;
            Uint32       StrongRefs = 0;
            EObjectFlags Flags      = EObjectFlags::None;
        };

        /** The live slot InHandle names, or null. Call with the lock held. */
        Slot*       FindSlot(ObjectHandle InHandle) noexcept;
        const Slot* FindSlot(ObjectHandle InHandle) const noexcept;

        mutable SharedMutex m_Mutex;
        TDynArray<Slot>     m_Slots;
        TDynArray<Uint32>   m_FreeSlots;
        Uint32              m_LiveCount = 0;
    };
}
