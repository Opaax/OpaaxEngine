#pragma once

/**
 * @class OpaaxNonCopyableAndMovable
 * @brief A utility base class to make derived classes non-copyable and non-movable.
 *
 * This class is designed to prevent copying and moving of instances of derived classes
 * by deleting the copy constructor, copy assignment operator, move constructor, and
 * move assignment operator. This is useful for classes that manage resources that
 * should not be duplicated or transferred to another instance.
 *
 * Derived classes can inherit from this class to explicitly indicate non-copyable
 * and non-movable behavior.
 *
 * @note The default constructor and destructor are protected, making this class
 * usable only as a base class.
 */
class OPAAX_API OpaaxNonCopyableAndMovable
{
protected:
    OpaaxNonCopyableAndMovable() = default;
    virtual ~OpaaxNonCopyableAndMovable() = default;

public:
    OpaaxNonCopyableAndMovable(const OpaaxNonCopyableAndMovable&) = delete;
    OpaaxNonCopyableAndMovable& operator=(const OpaaxNonCopyableAndMovable&) = delete;

    OpaaxNonCopyableAndMovable(OpaaxNonCopyableAndMovable&&) = delete;
    OpaaxNonCopyableAndMovable& operator=(OpaaxNonCopyableAndMovable&&) = delete;
};
