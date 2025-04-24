#pragma once

/**
 * @class OpaaxNonMovable
 *
 * @brief A base class designed to prevent derived classes from being move-constructed
 * or move-assigned while still allowing inheritance.
 *
 * The move constructor and move assignment operator are explicitly deleted
 * to ensure any derived class cannot be moved.
 *
 * This class provides a default constructor and a virtual destructor
 * to support inheritance and safe polymorphic behavior.
 */
class OPAAX_API OpaaxNonMovable
{
protected:
    // Default constructor and destructor to allow inheritance
    OpaaxNonMovable() = default;
    virtual ~OpaaxNonMovable() = default;
    
public:
    // Delete the move constructor and move assignment operator
    OpaaxNonMovable(OpaaxNonMovable&&) = delete;
    OpaaxNonMovable& operator=(OpaaxNonMovable&&) = delete;
};
