#include "Object/Object.h"

#include "Object/ObjectTable.h"

namespace Opaax
{
    const ObjectClass& Object::StaticClass()
    {
        // The root: no parent, no properties, never made by itself.
        static const ObjectClass s_Class([]
        {
            ObjectClassDesc lDesc;
            lDesc.Name  = DeriveTypeLeafName<Object>();
            lDesc.Id    = TypeIdOf<Object>();
            lDesc.Flags = EClassFlags::Abstract;
            lDesc.Size  = sizeof(Object);
            return lDesc;
        }());

        return s_Class;
    }

    Object::Object()
        : m_Handle(ObjectTable::Get().Add(*this))
    {
    }

    Object::~Object()
    {
        ObjectTable::Get().Remove(m_Handle);
    }

    EObjectFlags Object::GetFlags() const
    {
        return ObjectTable::Get().GetFlags(m_Handle);
    }

    bool Object::HasAnyFlags(const EObjectFlags InFlags) const
    {
        return HasFlag(GetFlags(), InFlags);
    }
}
