#include "Object/ObjectTable.h"

#include "Object/Object.h"

namespace Opaax
{
    ObjectTable& ObjectTable::Get()
    {
        // Never deleted: a static object's destructor may still leave the table during exit.
        static ObjectTable* s_Table = new ObjectTable();
        return *s_Table;
    }

    ObjectHandle ObjectTable::Add(Object& InObject)
    {
        TUniqueLock<SharedMutex> lLock(m_Mutex);

        Uint32 lIndex = 0;

        if (!m_FreeSlots.empty())
        {
            lIndex = m_FreeSlots.back();
            m_FreeSlots.pop_back();
        }
        else
        {
            lIndex = static_cast<Uint32>(m_Slots.size());
            m_Slots.emplace_back();
        }

        Slot& lSlot = m_Slots[lIndex];

        // Generation 0 means "no object": skip it when the counter wraps.
        lSlot.Generation = (lSlot.Generation == TNumericLimits<Uint32>::max()) ? 1 : lSlot.Generation + 1;
        lSlot.Address    = &InObject;
        lSlot.StrongRefs = 0;
        lSlot.Flags      = EObjectFlags::None;

        ++m_LiveCount;
        return ObjectHandle{ lIndex, lSlot.Generation };
    }

    void ObjectTable::Remove(const ObjectHandle InHandle)
    {
        TUniqueLock<SharedMutex> lLock(m_Mutex);

        Slot* lSlot = FindSlot(InHandle);
        if (lSlot == nullptr)
        {
            return;
        }

        // The generation stays: the next Add bumps it, so this handle never names the slot's next object.
        lSlot->Address    = nullptr;
        lSlot->StrongRefs = 0;
        lSlot->Flags      = EObjectFlags::None;

        m_FreeSlots.emplace_back(InHandle.Index);
        --m_LiveCount;
    }

    Object* ObjectTable::Resolve(const ObjectHandle InHandle) const
    {
        TSharedLock<SharedMutex> lLock(m_Mutex);

        const Slot* lSlot = FindSlot(InHandle);
        if (lSlot == nullptr || HasFlag(lSlot->Flags, EObjectFlags::Destroying))
        {
            return nullptr;
        }

        return lSlot->Address;
    }

    EObjectFlags ObjectTable::GetFlags(const ObjectHandle InHandle) const
    {
        TSharedLock<SharedMutex> lLock(m_Mutex);

        const Slot* lSlot = FindSlot(InHandle);
        return lSlot != nullptr ? lSlot->Flags : EObjectFlags::None;
    }

    void ObjectTable::AddFlags(const ObjectHandle InHandle, const EObjectFlags InFlags)
    {
        TUniqueLock<SharedMutex> lLock(m_Mutex);

        if (Slot* lSlot = FindSlot(InHandle))
        {
            lSlot->Flags = lSlot->Flags | InFlags;
        }
    }

    void ObjectTable::RemoveFlags(const ObjectHandle InHandle, const EObjectFlags InFlags)
    {
        TUniqueLock<SharedMutex> lLock(m_Mutex);

        if (Slot* lSlot = FindSlot(InHandle))
        {
            lSlot->Flags = lSlot->Flags & ~InFlags;
        }
    }

    bool ObjectTable::AddStrongRef(const ObjectHandle InHandle)
    {
        TUniqueLock<SharedMutex> lLock(m_Mutex);

        Slot* lSlot = FindSlot(InHandle);
        if (lSlot == nullptr || !HasFlag(lSlot->Flags, EObjectFlags::Managed)
            || HasFlag(lSlot->Flags, EObjectFlags::Destroying))
        {
            return false;
        }

        ++lSlot->StrongRefs;
        return true;
    }

    void ObjectTable::ReleaseStrongRef(const ObjectHandle InHandle)
    {
        Object* lToDestroy = nullptr;

        {
            TUniqueLock<SharedMutex> lLock(m_Mutex);

            Slot* lSlot = FindSlot(InHandle);
            if (lSlot == nullptr || lSlot->StrongRefs == 0)
            {
                return;
            }

            --lSlot->StrongRefs;

            if (lSlot->StrongRefs == 0 && HasFlag(lSlot->Flags, EObjectFlags::Managed))
            {
                // Handles read null from here on, before any destructor runs.
                lSlot->Flags = lSlot->Flags | EObjectFlags::Destroying;
                lToDestroy   = lSlot->Address;
            }
        }

        // Outside the lock: the destructor removes the slot, and may make or destroy other objects.
        delete lToDestroy;
    }

    Uint32 ObjectTable::GetStrongRefCount(const ObjectHandle InHandle) const
    {
        TSharedLock<SharedMutex> lLock(m_Mutex);

        const Slot* lSlot = FindSlot(InHandle);
        return lSlot != nullptr ? lSlot->StrongRefs : 0;
    }

    Uint32 ObjectTable::Count() const
    {
        TSharedLock<SharedMutex> lLock(m_Mutex);
        return m_LiveCount;
    }

    TDynArray<ObjectHandle> ObjectTable::Snapshot() const
    {
        TSharedLock<SharedMutex> lLock(m_Mutex);

        TDynArray<ObjectHandle> lHandles;
        lHandles.reserve(m_LiveCount);

        for (Uint32 lIndex = 0; lIndex < static_cast<Uint32>(m_Slots.size()); ++lIndex)
        {
            const Slot& lSlot = m_Slots[lIndex];

            if (lSlot.Address != nullptr && !HasFlag(lSlot.Flags, EObjectFlags::Destroying))
            {
                lHandles.emplace_back(ObjectHandle{ lIndex, lSlot.Generation });
            }
        }

        return lHandles;
    }

    ObjectTable::Slot* ObjectTable::FindSlot(const ObjectHandle InHandle) noexcept
    {
        if (!InHandle.IsValid() || InHandle.Index >= m_Slots.size())
        {
            return nullptr;
        }

        Slot& lSlot = m_Slots[InHandle.Index];
        return (lSlot.Address != nullptr && lSlot.Generation == InHandle.Generation) ? &lSlot : nullptr;
    }

    const ObjectTable::Slot* ObjectTable::FindSlot(const ObjectHandle InHandle) const noexcept
    {
        if (!InHandle.IsValid() || InHandle.Index >= m_Slots.size())
        {
            return nullptr;
        }

        const Slot& lSlot = m_Slots[InHandle.Index];
        return (lSlot.Address != nullptr && lSlot.Generation == InHandle.Generation) ? &lSlot : nullptr;
    }
}
