#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/Object.h"
#include "Object/ObjectTable.h"

// =============================================================================
// Pointers to objects. Three kinds, as in Unreal, so that a garbage collector can be added later
// without changing them:
//
//   TStrongObjectPtr<T>  owns: the object lives while one exists (NewObject returns one)
//   TObjectPtr<T>        a reference that a property list exposes: it reads null once the object
//                        is gone, and ObjectClass::CollectReferences finds it
//   TWeakObjectPtr<T>    never keeps the object alive; reads null once it is gone
//
// TObjectPtr and TWeakObjectPtr are handles into the ObjectTable, so neither can dangle.
// =============================================================================

namespace Opaax
{
    namespace ObjectPtrDetail
    {
        /** The name NewObject gives when none is asked for: "<Class>_<slot>". */
        inline OpaaxStringID MakeDefaultName(const ObjectClass& InClass, const ObjectHandle InHandle)
        {
            return OpaaxStringID(OpaaxString::Format("{}_{}", InClass.GetName().CStr(), InHandle.Index));
        }

        /** The object InHandle names, as a T (the handle was taken from a T). */
        template<typename T>
        T* ResolveAs(const ObjectHandle InHandle)
        {
            return static_cast<T*>(ObjectTable::Get().Resolve(InHandle));
        }
    }

    // =============================================================================
    // TStrongObjectPtr<T> — one owner of an object made by NewObject. Copying adds an owner; the last
    //   one destroys the object.
    // =============================================================================
    template<typename T>
    class TStrongObjectPtr
    {
    public:
        TStrongObjectPtr() noexcept = default;
        TStrongObjectPtr(decltype(nullptr)) noexcept {}

        /**
         * Becomes an owner of InObject, which NewObject made. Any other object (on the stack, a member,
         * a class default) is refused with an error: an owner would delete it.
         */
        explicit TStrongObjectPtr(T* InObject)
        {
            if (InObject == nullptr) { return; }

            if (ObjectTable::Get().AddStrongRef(InObject->GetHandle()))
            {
                m_Object = InObject;
            }
            else
            {
                OPAAX_LOG(LogObject, Error, "'{}' was not made by NewObject: it cannot have an owner",
                          InObject->GetName().CStr());
            }
        }

        TStrongObjectPtr(const TStrongObjectPtr& InOther) : TStrongObjectPtr(InOther.m_Object) {}

        TStrongObjectPtr(TStrongObjectPtr&& InOther) noexcept : m_Object(InOther.m_Object)
        {
            InOther.m_Object = nullptr;
        }

        /** From a pointer to a derived class. */
        template<typename U>
        requires CDerivedFrom<U, T>
        TStrongObjectPtr(const TStrongObjectPtr<U>& InOther) : TStrongObjectPtr(static_cast<T*>(InOther.Get())) {}

        template<typename U>
        requires CDerivedFrom<U, T>
        TStrongObjectPtr(TStrongObjectPtr<U>&& InOther) noexcept : m_Object(InOther.Release()) {}

        ~TStrongObjectPtr() { Reset(); }

        TStrongObjectPtr& operator=(const TStrongObjectPtr& InOther)
        {
            if (this != &InOther)
            {
                TStrongObjectPtr lCopy(InOther);
                Swap(m_Object, lCopy.m_Object);
            }
            return *this;
        }

        TStrongObjectPtr& operator=(TStrongObjectPtr&& InOther) noexcept
        {
            if (this != &InOther)
            {
                Reset();
                m_Object         = InOther.m_Object;
                InOther.m_Object = nullptr;
            }
            return *this;
        }

        /** Drops this owner (the object is destroyed if it was the last). */
        void Reset()
        {
            if (m_Object != nullptr)
            {
                const ObjectHandle lHandle = m_Object->GetHandle();
                m_Object = nullptr;
                ObjectTable::Get().ReleaseStrongRef(lHandle);
            }
        }

        T* Get() const noexcept { return m_Object; }
        T* operator->() const noexcept { return m_Object; }
        T& operator*() const noexcept { return *m_Object; }

        bool IsValid() const noexcept { return m_Object != nullptr; }
        explicit operator bool() const noexcept { return IsValid(); }

        bool operator==(const TStrongObjectPtr& InOther) const noexcept { return m_Object == InOther.m_Object; }
        bool operator==(decltype(nullptr)) const noexcept { return m_Object == nullptr; }

    private:
        template<typename U>
        friend class TStrongObjectPtr;

        /** Hands this owner over without releasing it (a converting move). */
        T* Release() noexcept
        {
            T* lObject = m_Object;
            m_Object   = nullptr;
            return lObject;
        }

        T* m_Object = nullptr;
    };

    /** InPointer's object as a T owner when it is one, else empty. */
    template<typename T, typename U>
    TStrongObjectPtr<T> Cast(const TStrongObjectPtr<U>& InPointer)
    {
        return TStrongObjectPtr<T>(Cast<T>(InPointer.Get()));
    }

    // =============================================================================
    // TWeakObjectPtr<T> — names an object without keeping it alive. Reads null once it is gone.
    // =============================================================================
    template<typename T>
    class TWeakObjectPtr
    {
    public:
        TWeakObjectPtr() noexcept = default;
        TWeakObjectPtr(decltype(nullptr)) noexcept {}

        TWeakObjectPtr(T* InObject) noexcept : m_Handle(InObject != nullptr ? InObject->GetHandle() : ObjectHandle{}) {}

        template<typename U>
        requires CDerivedFrom<U, T>
        TWeakObjectPtr(const TStrongObjectPtr<U>& InOwner) noexcept : TWeakObjectPtr(static_cast<T*>(InOwner.Get())) {}

        template<typename U>
        requires CDerivedFrom<U, T>
        TWeakObjectPtr(const TWeakObjectPtr<U>& InOther) noexcept : m_Handle(InOther.GetHandle()) {}

        /** The object, or null once it is gone. */
        T* Get() const { return ObjectPtrDetail::ResolveAs<T>(m_Handle); }
        T* operator->() const { return Get(); }

        bool IsValid() const { return Get() != nullptr; }
        explicit operator bool() const { return IsValid(); }

        void Reset() noexcept { m_Handle = ObjectHandle{}; }

        ObjectHandle GetHandle() const noexcept { return m_Handle; }

        bool operator==(const TWeakObjectPtr& InOther) const noexcept { return m_Handle == InOther.m_Handle; }

    private:
        ObjectHandle m_Handle;
    };

    // =============================================================================
    // TObjectPtr<T> — a reference held in a property list. Reads null once its object is gone; the
    //   property visitor and ObjectClass::CollectReferences see it (what a collector would follow).
    //   It is not saved in files: an asset names another asset by path (TAssetRef).
    // =============================================================================
    template<typename T>
    class TObjectPtr
    {
    public:
        using ObjectType = T;

        /** Lets the property visitor recognise the field without depending on this header. */
        static constexpr bool k_IsObjectPtr = true;

        TObjectPtr() noexcept = default;
        TObjectPtr(decltype(nullptr)) noexcept {}

        TObjectPtr(T* InObject) noexcept : m_Handle(InObject != nullptr ? InObject->GetHandle() : ObjectHandle{}) {}

        template<typename U>
        requires CDerivedFrom<U, T>
        TObjectPtr(const TStrongObjectPtr<U>& InOwner) noexcept : TObjectPtr(static_cast<T*>(InOwner.Get())) {}

        /** The object, or null once it is gone. */
        T* Get() const { return ObjectPtrDetail::ResolveAs<T>(m_Handle); }
        T* operator->() const { return Get(); }

        bool IsValid() const { return Get() != nullptr; }
        explicit operator bool() const { return IsValid(); }

        void Reset() noexcept { m_Handle = ObjectHandle{}; }

        ObjectHandle  GetHandle() const noexcept { return m_Handle; }

        /** The handle itself, for the visitor (a tool may retarget the reference). */
        ObjectHandle& GetMutableHandle() noexcept { return m_Handle; }

        bool operator==(const TObjectPtr& InOther) const noexcept { return m_Handle == InOther.m_Handle; }

    private:
        ObjectHandle m_Handle;
    };

    // =============================================================================
    // Making objects
    // =============================================================================

    /**
     * A new T on the heap, owned by the returned pointer.
     * @param InName Its name. Invalid gives "<Class>_<slot>"
     */
    template<typename T>
    requires CObject<T>
    TStrongObjectPtr<T> NewObject(const OpaaxStringID InName = {})
    {
        static_assert(!TIsAbstract<T> && CDefaultInitializable<T>,
                      "NewObject: the class must be concrete and default-constructible.");

        T* lObject = new T();

        const ObjectHandle lHandle = lObject->GetHandle();
        ObjectTable::Get().AddFlags(lHandle, EObjectFlags::Managed);
        lObject->SetName(InName.IsValid() ? InName : ObjectPtrDetail::MakeDefaultName(T::StaticClass(), lHandle));

        return TStrongObjectPtr<T>(lObject);
    }

    /**
     * A new object of InClass (a class known only at run time), owned by the returned pointer.
     * @return Empty when InClass is abstract
     */
    inline TStrongObjectPtr<Object> NewObject(const ObjectClass& InClass, const OpaaxStringID InName = {})
    {
        return InClass.CreateInstance(InName);
    }
}
