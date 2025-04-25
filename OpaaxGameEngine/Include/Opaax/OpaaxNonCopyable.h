#pragma once
#include "Opaax/OpaaxCoreMacros.h"

namespace OPAAX
{
    /**
     * @class OpaaxNonCopyable
     *
     * A lightweight base class designed to restrict copy and assignment operations for derived classes.
     *
     * This class is intended to be inherited by other classes that need to explicitly disable
     * copy constructor and copy assignment operator to prevent inadvertent copying.
     *
     * Usage:
     * - Any class inheriting from OpaaxNonCopyable will automatically become non-copyable.
     * - Attempts to copy or assign instances of such derived classes will result in a compiler error.
     */
    class OPAAX_API OpaaxNonCopyable
    {
    protected:
        OpaaxNonCopyable() = default;
        virtual ~OpaaxNonCopyable() = default;

    public:
        OpaaxNonCopyable(const OpaaxNonCopyable&) = delete;
        OpaaxNonCopyable& operator=(const OpaaxNonCopyable&) = delete;
    };
}
