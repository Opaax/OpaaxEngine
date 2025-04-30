#pragma once
#include "OpaaxMath.h"
#include "Opaax/OpaaxTRequire.h"
#include "Opaax/Boost/OPBoostTypes.h"

//-----------------------------------------------------------------
// FORWARD
//-----------------------------------------------------------------
namespace OPAAX
{
    namespace MATH
    {
        template <CONCEPT_TIsFloat TReal>
        class OpaaxDegree;
    }
}

namespace OPAAX
{
    namespace MATH
    {
        /**
         * @class OpaaxRadian: Represents an angle in radians.
         *
         * OpaaxRadian class provides functionality to work with angles in Radian and supports conversion to degree when necessary.
         * This class can be constructed with a specified degree value or by converting from an OpaaxDegree object.
         * Users can retrieve the angle value in radians using the GetDegree method.
         *
         * Inspired by Ogre Engine.
         */
        template <CONCEPT_TIsFloat TReal>
        class OpaaxRadian
        {
            //-----------------------------------------------------------------
            // Members
            //-----------------------------------------------------------------
            //-------------------------- Private -----------------------------//
            TReal m_value;

            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
        public:
            explicit OpaaxRadian(TReal Radian = 0) : m_value(Radian) {}
            OpaaxRadian(const OpaaxDegree<TReal>& Degree);
            OpaaxRadian(const OpaaxRadian& Other): m_value{Other.m_value} {}
            OpaaxRadian(OpaaxRadian&& Other) noexcept = default;

            ~OpaaxRadian() = default;

            //-----------------------------------------------------------------
            // Functions
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
            /**
             * Get a string representation of the current angle in radians.
             *
             * @return A string containing the current angle value in radians.
             */
            std::string ToString() const;

            //-------------------------- GETTER -----------------------------//
            /**
             * Get the angle value in degrees corresponding to the current angle value in radians.
             *
             * @return The angle value in degrees calculated from the current angle value in radians.
             */
            [[nodiscard]] FORCEINLINE TReal GetDegree() const;
            /**
             * Get the value of the angle in radians.
             *
             * @return The current value of the angle in radians.
             */
            [[nodiscard]] FORCEINLINE TReal GetValue() const { return m_value; }
            /**
             * Get the angle value in degrees corresponding to the current angle value in radians.
             *
             * @return The angle value in degrees calculated from the current angle value in radians.
             */
            [[nodiscard]] FORCEINLINE OpaaxDegree<TReal> AsDegree() const { return OpaaxDegree{GetDegree()}; }

            //-------------------------- SETTER -----------------------------//
            /**
             * Set the value of the angle in radians.
             *
             * @param NewValue The new value to set for the angle in radians.
             */
            FORCEINLINE void SetValue(const TReal NewValue) { m_value = NewValue; }
            /**
             * Set the value of the angle in radians.
             *
             * @param Radian The new OpaaxRadian object whose value will be used to set the angle in radians.
             */
            FORCEINLINE void SetValue(const OpaaxRadian& Radian) { m_value = Radian.GetValue(); }
            /**
             * Set the value of the angle in radians based on the provided OpaaxDegree object.
             *
             * @param Degree The OpaaxDegree object from which the angle value will be retrieved and set for the angle in radians.
             */
            FORCEINLINE void SetValue(const OpaaxDegree<TReal>& Degree);

            //-----------------------------------------------------------------
            // Operators
            //-----------------------------------------------------------------
            FORCEINLINE OpaaxRadian& operator+() const { return *this; }
            FORCEINLINE OpaaxRadian operator+(TReal Scalar) const { return OpaaxRadian(m_value + Scalar); }
            FORCEINLINE OpaaxRadian operator-(TReal Scalar) const { return OpaaxRadian(m_value - Scalar); }
            FORCEINLINE OpaaxRadian operator*(TReal Scalar) const { return OpaaxRadian(m_value * Scalar); }
            FORCEINLINE OpaaxRadian operator/(TReal Scalar) const { return OpaaxRadian(m_value / Scalar); }

            FORCEINLINE OpaaxRadian operator+(const OpaaxRadian& Other) const
            {
                return OpaaxRadian(m_value + Other.m_value);
            }

            FORCEINLINE OpaaxRadian operator+(const OpaaxDegree<TReal>& Other) const;
            FORCEINLINE OpaaxRadian operator-(const OpaaxRadian& Other) const
            {
                return OpaaxRadian(m_value - Other.m_value);
            }

            FORCEINLINE OpaaxRadian operator-(const OpaaxDegree<TReal>& Other) const;
            FORCEINLINE OpaaxRadian operator*(const OpaaxRadian& Other) const
            {
                return OpaaxRadian(m_value * Other.m_value);
            }

            FORCEINLINE OpaaxRadian operator*(const OpaaxDegree<TReal>& Other) const;
            FORCEINLINE OpaaxRadian operator/(const OpaaxRadian& Other) const
            {
                return OpaaxRadian(m_value / Other.m_value);
            }

            FORCEINLINE OpaaxRadian operator/(const OpaaxDegree<TReal>& Other) const;

            FORCEINLINE OpaaxRadian& operator+=(const OpaaxRadian& Other)
            {
                m_value += Other.m_value;
                return *this;
            }

            FORCEINLINE OpaaxRadian& operator+=(const OpaaxDegree<TReal>& Other);
            FORCEINLINE OpaaxRadian& operator-=(const OpaaxRadian& Other)
            {
                m_value -= Other.m_value;
                return *this;
            }

            FORCEINLINE OpaaxRadian& operator-=(const OpaaxDegree<TReal>& Other);
            FORCEINLINE OpaaxRadian& operator*=(const OpaaxRadian& Other)
            {
                m_value *= Other.m_value;
                return *this;
            }

            FORCEINLINE OpaaxRadian& operator*=(const OpaaxDegree<TReal>& Other);
            FORCEINLINE OpaaxRadian& operator/=(const OpaaxRadian& Other)
            {
                m_value /= Other.m_value;
                return *this;
            }

            FORCEINLINE OpaaxRadian& operator/=(const OpaaxDegree<TReal>& Other);

            bool operator<(const OpaaxRadian& Other) const { return m_value < Other.m_value; }
            bool operator<=(const OpaaxRadian& Other) const { return m_value <= Other.m_value; }
            bool operator==(const OpaaxRadian& Other) const { return m_value == Other.m_value; }
            bool operator!=(const OpaaxRadian& Other) const { return m_value != Other.m_value; }
            bool operator>=(const OpaaxRadian& Other) const { return m_value >= Other.m_value; }
            bool operator>(const OpaaxRadian& Other) const { return m_value > Other.m_value; }

            bool operator<(const OpaaxDegree<TReal>& Other) const;
            bool operator<=(const OpaaxDegree<TReal>& Other) const;
            bool operator==(const OpaaxDegree<TReal>& Other) const;
            bool operator!=(const OpaaxDegree<TReal>& Other) const;
            bool operator>=(const OpaaxDegree<TReal>& Other) const;
            bool operator>(const OpaaxDegree<TReal>& Other) const;

            friend bool operator==(const OpaaxRadian& LHS, const OpaaxRadian& RHS)
            {
                return LHS.m_value == RHS.m_value;
            }

            friend bool operator!=(const OpaaxRadian& LHS, const OpaaxRadian& RHS) { return !(LHS == RHS); }
            friend bool operator<(const OpaaxRadian& LHS, const OpaaxRadian& RHS) { return LHS.m_value < RHS.m_value; }
            friend bool operator<=(const OpaaxRadian& LHS, const OpaaxRadian& RHS) { return !(RHS < LHS); }
            friend bool operator>(const OpaaxRadian& LHS, const OpaaxRadian& RHS) { return RHS < LHS; }
            friend bool operator>=(const OpaaxRadian& LHS, const OpaaxRadian& RHS) { return !(LHS < RHS); }

            OpaaxRadian& operator=(TReal Scalar)
            {
                m_value = Scalar;
                return *this;
            }

            OpaaxRadian& operator=(const OpaaxDegree<TReal>& Degree) const;

            OpaaxRadian& operator=(const OpaaxRadian& other)
            {
                if (this == &other) { return *this; }
                m_value = other.m_value;
                return *this;
            }

            OpaaxRadian& operator=(OpaaxRadian&& other) noexcept = default;

            friend std::ostream& operator<<(std::ostream& OS, const OpaaxRadian& Radian)
            {
                return OS << Radian.m_value << " Radian";
            }
        };

        /**
         * @class OpaaxDegree
         * Represents an angle in degrees.
         *
         * OpaaxDegree class provides functionality to work with angles in degrees and supports conversion to radians when necessary.
         * This class can be constructed with a specified degree value or by converting from an OpaaxRadian object.
         * Users can retrieve the angle value in radians using the GetRadian method.
         *
         * Inspired by Ogre Engine.
         */
        template <CONCEPT_TIsFloat TReal>
        class OpaaxDegree
        {
        private:
            //-----------------------------------------------------------------
            // Members
            //-----------------------------------------------------------------
            //-------------------------- Private -----------------------------//
            TReal m_value;

            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
        public:
            explicit OpaaxDegree(const TReal m_value) : m_value(m_value) {}
            explicit OpaaxDegree(const OpaaxDegree& Other) : m_value{Other.m_value} {}
            explicit OpaaxDegree(const OpaaxRadian<TReal>& Radian);
            OpaaxDegree(OpaaxDegree&& Other) noexcept = default;

            ~OpaaxDegree() = default;

            //-----------------------------------------------------------------
            // Functions
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
            /**
             * Convert the OpaaxDegree object to a string representation.
             *
             * @return The string representation of the OpaaxDegree object, including the degree value.
             */
            std::string ToString() const;

            //-------------------------- GETTER -----------------------------//
            /**
             * Get the value of the angle in radians.
             * Converts the degree value to radians using OPMath::DegreesToRadians utility function.
             *
             * @return The angle value in radians.
             */
            [[nodiscard]] FORCEINLINE TReal GetRadian() const;
            /**
             * Get the value of the OpaaxDegree object.
             *
             * @return The value of the OpaaxDegree object.
             */
            [[nodiscard]] FORCEINLINE TReal GetValue() const { return m_value; }
            /**
             * Convert the OpaaxDegree object to OpaaxRadian type.
             * Calls the GetRadian() method from OpaaxDegree to retrieve the angle value in radians.
             *
             * @return The OpaaxRadian object converted from the angle in radians.
             */
            [[nodiscard]] FORCEINLINE OpaaxRadian<TReal> AsRadian() const { return OpaaxRadian{GetRadian()}; }

            //-------------------------- SETTER -----------------------------//
            /**
             * Set the value.
             * Consider operator =
             *
             * @param NewValue The new value to be set for the OpaaxDegree object.
             */
            FORCEINLINE void SetValue(const TReal NewValue) { m_value = NewValue; }
            /**
             * Set the value based on the provided OpaaxDegree object.
             * Consider operator =
             *
             * @param Degree The OpaaxDegree object from which the value will be retrieved.
             */
            FORCEINLINE void SetValue(const OpaaxDegree& Degree) { m_value = Degree.GetValue(); }
            /**
             * Set the value based on the provided OpaaxRadian object.
             * Consider operator =
             *
             * @param Radian The OpaaxRadian object from which the value will be retrieved and converted to degree.
             */
            FORCEINLINE void SetValue(const OpaaxRadian<TReal>& Radian);

            //-----------------------------------------------------------------
            // Operators
            //-----------------------------------------------------------------
            FORCEINLINE OpaaxDegree& operator+() const { return *this; }
            FORCEINLINE OpaaxDegree operator+(TReal Scalar) const { return OpaaxDegree(m_value + Scalar); }
            FORCEINLINE OpaaxDegree operator-(TReal Scalar) const { return OpaaxDegree(m_value - Scalar); }
            FORCEINLINE OpaaxDegree operator*(TReal Scalar) const { return OpaaxDegree(m_value * Scalar); }
            FORCEINLINE OpaaxDegree operator/(TReal Scalar) const { return OpaaxDegree(m_value / Scalar); }

            FORCEINLINE OpaaxDegree operator+(const OpaaxDegree& Other) const
            {
                return OpaaxRadian(m_value + Other.m_value);
            }

            FORCEINLINE OpaaxDegree operator+(const OpaaxRadian<TReal>& Other) const;
            FORCEINLINE OpaaxDegree operator-(const OpaaxDegree& Other) const
            {
                return OpaaxRadian(m_value - Other.m_value);
            }

            FORCEINLINE OpaaxDegree operator-(const OpaaxRadian<TReal>& Other) const;
            FORCEINLINE OpaaxDegree operator*(const OpaaxDegree& Other) const
            {
                return OpaaxRadian(m_value * Other.m_value);
            }

            FORCEINLINE OpaaxDegree operator*(const OpaaxRadian<TReal>& Other) const;
            FORCEINLINE OpaaxDegree operator/(const OpaaxDegree& Other) const
            {
                return OpaaxRadian(m_value / Other.m_value);
            }

            FORCEINLINE OpaaxDegree operator/(const OpaaxRadian<TReal>& Other) const;

            FORCEINLINE OpaaxDegree& operator+=(const OpaaxDegree& Other)
            {
                m_value += Other.m_value;
                return *this;
            }

            FORCEINLINE OpaaxDegree& operator+=(const OpaaxRadian<TReal>& Other);
            FORCEINLINE OpaaxDegree& operator-=(const OpaaxDegree& Other)
            {
                m_value -= Other.m_value;
                return *this;
            }

            FORCEINLINE OpaaxDegree& operator-=(const OpaaxRadian<TReal>& Other);
            FORCEINLINE OpaaxDegree& operator*=(const OpaaxDegree& Other)
            {
                m_value *= Other.m_value;
                return *this;
            }

            FORCEINLINE OpaaxDegree& operator*=(const OpaaxRadian<TReal>& Other);
            FORCEINLINE OpaaxDegree& operator/=(const OpaaxDegree& Other)
            {
                m_value /= Other.m_value;
                return *this;
            }

            FORCEINLINE OpaaxDegree& operator/=(const OpaaxRadian<TReal>& Other);

            bool operator<(const OpaaxDegree& Other) const { return m_value < Other.m_value; }
            bool operator<=(const OpaaxDegree& Other) const { return m_value <= Other.m_value; }
            bool operator==(const OpaaxDegree& Other) const { return m_value == Other.m_value; }
            bool operator!=(const OpaaxDegree& Other) const { return m_value != Other.m_value; }
            bool operator>=(const OpaaxDegree& Other) const { return m_value >= Other.m_value; }
            bool operator>(const OpaaxDegree& Other) const { return m_value > Other.m_value; }

            bool operator<(const OpaaxRadian<TReal>& Other) const;
            bool operator<=(const OpaaxRadian<TReal>& Other) const;
            bool operator==(const OpaaxRadian<TReal>& Other) const;
            bool operator!=(const OpaaxRadian<TReal>& Other) const;
            bool operator>=(const OpaaxRadian<TReal>& Other) const;
            bool operator>(const OpaaxRadian<TReal>& Other) const;

            friend bool operator==(const OpaaxDegree& LHS, const OpaaxDegree& RHS)
            {
                return LHS.m_value == RHS.m_value;
            }

            friend bool operator!=(const OpaaxDegree& LHS, const OpaaxDegree& RHS) { return !(LHS == RHS); }
            friend bool operator<(const OpaaxDegree& LHS, const OpaaxDegree& RHS) { return LHS.m_value < RHS.m_value; }
            friend bool operator<=(const OpaaxDegree& LHS, const OpaaxDegree& RHS) { return !(RHS < LHS); }
            friend bool operator>(const OpaaxDegree& LHS, const OpaaxDegree& RHS) { return RHS < LHS; }
            friend bool operator>=(const OpaaxDegree& LHS, const OpaaxDegree& RHS) { return !(LHS < RHS); }

            OpaaxDegree& operator=(TReal Scalar)
            {
                m_value = Scalar;
                return *this;
            }

            OpaaxDegree& operator=(const OpaaxRadian<TReal>& Radian) const;

            OpaaxDegree& operator=(const OpaaxDegree& Other)
            {
                if (this == &Other) { return *this; }
                m_value = Other.m_value;
                return *this;
            }

            OpaaxDegree& operator=(OpaaxDegree&& other) noexcept = default;

            friend std::ostream& operator<<(std::ostream& OS, const OpaaxDegree& Degree)
            {
                return OS << Degree.m_value << " Degree";
            }
        };

        /**
 * @class OpaaxAngle: Representing an angle in Radian.
 * Can be initialized with radians or degrees.
 */
template <CONCEPT_TIsFloat TReal>
class OpaaxAngle
{
    //-----------------------------------------------------------------
    // Members
    //-----------------------------------------------------------------
    //-------------------------- Private -----------------------------//
    TReal m_angle;

    //-----------------------------------------------------------------
    // CTOR/DTOR
    //-----------------------------------------------------------------
    //-------------------------- Private -----------------------------//
private:
    /**
     * Make Scalar ctor private to avoid confusion and possible error resulting head neck debugging
     * @param Angle 
     */
    OpaaxAngle(TReal Angle = 0.0f):m_angle{Angle} {}
     
    //-------------------------- Public -----------------------------//
public:
    explicit    OpaaxAngle(OpaaxRadian<TReal> Radian)   :m_angle{Radian} {}
    explicit    OpaaxAngle(OpaaxDegree<TReal> Degree)   :m_angle{Degree.GetRadian()} {}
    OpaaxAngle(const OpaaxAngle& Other)     :m_angle{Other.m_angle} {}
    OpaaxAngle(OpaaxAngle&& other) noexcept = default;
     
    ~OpaaxAngle() = default;

    //-----------------------------------------------------------------
    // Functions
    //-----------------------------------------------------------------
    //-------------------------- Public -----------------------------//
    /**
     * Get a string representation of the current angle in degrees.
     *
     * @return A string containing the current angle value in degrees.
     */
    OSTDString  ToString() const;
     
    //-------------------------- GETTER -----------------------------//
    /**
     * Retrieves the angle value in radians.
     *
     * @return The angle value in radians.
     */
    [[nodiscard]] FORCEINLINE TReal GetAngle() const                   {return m_angle;}
    /**
     * Retrieves the angle value in degrees.
     *
     * @return The angle value in degrees.
     */
    [[nodiscard]] FORCEINLINE TReal GetAngleDegree() const             { return m_angle * static_cast<TReal>(OP_DOUBLE_RAD_TO_DEG); }
    /**
     * Retrieves the angle value as a Radian object.
     *
     * @return OpaaxRadian<TReal> - The angle value as a Radian object.
     */
    [[nodiscard]] FORCEINLINE OpaaxRadian<TReal> GetRadian() const     { return OpaaxRadian{m_angle}; }
    /**
     * Retrieves the angle value as a Degree object.
     *
     * @return OpaaxDegree<TReal> - The degree value.
     */
    [[nodiscard]] FORCEINLINE OpaaxDegree<TReal> GetDegree() const     { return OpaaxDegree{GetRadian()}; }

    /**
     * Calculates and retrieves half of the current angle value in radians.
     *
     * @return Half of the current angle value in radians.
     */
    [[nodiscard]] FORCEINLINE TReal GetHalfAngleRadian() const         { return m_angle / static_cast<TReal>(2); }
    /**
     * Retrieves half of the current angle value in degrees.
     *
     * @return Half of the current angle value in degrees.
     */
    [[nodiscard]] FORCEINLINE TReal GetHalfAngleDegree() const         { return GetAngleDegree() / static_cast<TReal>(2); }
    /**
     * Retrieves half of the current angle value in radians.
     *
     * @return A constant OpaaxRadian<TReal> object representing half of the current angle value in radians.
     */
    [[nodiscard]] FORCEINLINE OpaaxRadian<TReal> GetHalfRadian() const { return OpaaxRadian{GetHalfAngleRadian()}; }
    /**
     * Calculates and retrieves half of the current angle value in degrees.
     *
     * @return An OpaaxDegree<TReal> object representing half of the current angle value in degrees.
     */
    [[nodiscard]] FORCEINLINE OpaaxDegree<TReal> GetHalfDegree() const { return OpaaxDegree{GetHalfRadian()}; }

    //-------------------------- SETTER -----------------------------//
    /**
     * Sets the angle value in radians.
     *
     * @param RadianAngle The angle value to set in radians.
     */
    FORCEINLINE void SetAngle       (TReal RadianAngle)                 {m_angle = RadianAngle;}
    /**
     * Sets the angle value in degrees.
     *
     * @param DegreeAngle The angle value to set in degrees.
     */
    FORCEINLINE void SetAngleDegree (TReal DegreeAngle);
    /**
     * Sets the angle value in radians.
     *
     * @param Radian The OpaaxRadian object representing the angle value in radians to set.
     */
    FORCEINLINE void SetAngle       (const OpaaxRadian<TReal>& Radian)  {m_angle = Radian.GetValue();}
    /**
     * Sets the angle value in degrees.
     *
     * @param Degree The OpaaxDegree object representing the angle value in Degree to set.
     */
    FORCEINLINE void SetAngleDegree (const OpaaxDegree<TReal>& Degree);

    //-----------------------------------------------------------------
    // Operators
    //-----------------------------------------------------------------

    OpaaxAngle operator+(const OpaaxAngle& Angle) const            { return m_angle + Angle.GetAngle();}
    OpaaxAngle operator-(const OpaaxAngle& Angle) const            { return m_angle - Angle.GetAngle();}
    OpaaxAngle operator*(const OpaaxAngle& Angle) const            { return m_angle * Angle.GetAngle();}
    OpaaxAngle operator/(const OpaaxAngle& Angle) const            { return m_angle / Angle.GetAngle();}

    OpaaxAngle operator+(const OpaaxDegree<TReal>& Degree) const   { return m_angle + Degree.GetRadian(); }
    OpaaxAngle operator-(const OpaaxDegree<TReal>& Degree) const   { return m_angle - Degree.GetRadian(); }
    OpaaxAngle operator*(const OpaaxDegree<TReal>& Degree) const   { return m_angle * Degree.GetRadian(); }
    OpaaxAngle operator/(const OpaaxDegree<TReal>& Degree) const   { return m_angle / Degree.GetRadian(); }
                                                                
    OpaaxAngle operator+(const OpaaxRadian<TReal>& Radian) const   { return m_angle + Radian.GetValue(); }
    OpaaxAngle operator-(const OpaaxRadian<TReal>& Radian) const   { return m_angle - Radian.GetValue(); }
    OpaaxAngle operator*(const OpaaxRadian<TReal>& Radian) const   { return m_angle * Radian.GetValue(); }
    OpaaxAngle operator/(const OpaaxRadian<TReal>& Radian) const   { return m_angle / Radian.GetValue(); }
                                                                
    OpaaxAngle& operator+=(const OpaaxAngle& Angle)                { m_angle += Angle.GetAngle(); return *this;}
    OpaaxAngle& operator-=(const OpaaxAngle& Angle)                { m_angle -= Angle.GetAngle(); return *this;}
    OpaaxAngle& operator*=(const OpaaxAngle& Angle)                { m_angle *= Angle.GetAngle(); return *this;}
    OpaaxAngle& operator/=(const OpaaxAngle& Angle)                { m_angle /= Angle.GetAngle(); return *this;}

    OpaaxAngle& operator+=(const OpaaxDegree<TReal>& Degree)       { m_angle += Degree.GetRadian(); return *this;}
    OpaaxAngle& operator-=(const OpaaxDegree<TReal>& Degree)       { m_angle -= Degree.GetRadian(); return *this;}
    OpaaxAngle& operator*=(const OpaaxDegree<TReal>& Degree)       { m_angle *= Degree.GetRadian(); return *this;}
    OpaaxAngle& operator/=(const OpaaxDegree<TReal>& Degree)       { m_angle /= Degree.GetRadian(); return *this;}
                                                                   
    OpaaxAngle& operator+=(const OpaaxRadian<TReal>& Radian)       { m_angle += Radian.GetValue(); return *this;}
    OpaaxAngle& operator-=(const OpaaxRadian<TReal>& Radian)       { m_angle -= Radian.GetValue(); return *this;}
    OpaaxAngle& operator*=(const OpaaxRadian<TReal>& Radian)       { m_angle *= Radian.GetValue(); return *this;}
    OpaaxAngle& operator/=(const OpaaxRadian<TReal>& Radian)       { m_angle /= Radian.GetValue(); return *this;}
     
    //ADD Operator to check Degree / Radian?
     
    friend bool operator==(const OpaaxAngle& LHS, const OpaaxAngle& RHS) { return LHS.m_angle == RHS.m_angle; }
    friend bool operator!=(const OpaaxAngle& LHS, const OpaaxAngle& RHS) { return !(LHS == RHS); }

    friend bool operator< (const OpaaxAngle& LHS, const OpaaxAngle& RHS) { return LHS.m_angle < RHS.m_angle; }
    friend bool operator<=(const OpaaxAngle& LHS, const OpaaxAngle& RHS) { return !(RHS < LHS); }
    friend bool operator> (const OpaaxAngle& LHS, const OpaaxAngle& RHS) { return RHS < LHS; }
    friend bool operator>=(const OpaaxAngle& LHS, const OpaaxAngle& RHS) { return !(LHS < RHS); }

    OpaaxAngle& operator=(const OpaaxAngle& Other)
    {
        if (this == &Other) {return *this;}
        m_angle = Other.m_angle;
        return *this;
    }
     
    OpaaxAngle& operator=(OpaaxAngle&& other) noexcept = default;

    friend std::ostream& operator<<(std::ostream& OS, const OpaaxAngle& Angle)
    {
        return OS << "Angle: " << Angle.GetRadian() << " --- " << Angle.GetDegree();
    }
};

        //-----------------------------------------------------------------
        // Opaax Radian Implementation
        //-----------------------------------------------------------------
#pragma region OpaaxRadianImpl
        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal>::OpaaxRadian(const OpaaxDegree<TReal>& Degree): m_value{Degree.GetRadian()} {}

        template <CONCEPT_TIsFloat TReal>
        std::string OpaaxRadian<TReal>::ToString() const
        {
            return (BSTFormat("%1% Radian") % m_value).str();
        }

        template <CONCEPT_TIsFloat TReal>
        TReal OpaaxRadian<TReal>::GetDegree() const
        {
            return OpaaxMath::RadiansToDegrees(m_value);
        }

        template <CONCEPT_TIsFloat TReal>
        void OpaaxRadian<TReal>::SetValue(const OpaaxDegree<TReal>& Degree)
        {
            m_value = Degree.GetRadian();
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal> OpaaxRadian<TReal>::operator+(const OpaaxDegree<TReal>& Other) const
        {
            return OpaaxRadian(m_value + Other.GetRadian());
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal> OpaaxRadian<TReal>::operator-(const OpaaxDegree<TReal>& Other) const
        {
            return OpaaxRadian(m_value - Other.GetRadian());
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal> OpaaxRadian<TReal>::operator*(const OpaaxDegree<TReal>& Other) const
        {
            return OpaaxRadian(m_value * Other.GetRadian());
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal> OpaaxRadian<TReal>::operator/(const OpaaxDegree<TReal>& Other) const
        {
            return OpaaxRadian(m_value / Other.GetRadian());
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal>& OpaaxRadian<TReal>::operator+=(const OpaaxDegree<TReal>& Other)
        {
            m_value += Other.GetRadian();
            return *this;
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal>& OpaaxRadian<TReal>::operator-=(const OpaaxDegree<TReal>& Other)
        {
            m_value -= Other.GetRadian();
            return *this;
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal>& OpaaxRadian<TReal>::operator*=(const OpaaxDegree<TReal>& Other)
        {
            m_value *= Other.GetRadian();
            return *this;
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal>& OpaaxRadian<TReal>::operator/=(const OpaaxDegree<TReal>& Other)
        {
            m_value /= Other.GetRadian();
            return *this;
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxRadian<TReal>::operator<(const OpaaxDegree<TReal>& Other) const
        {
            return m_value < Other.GetRadian();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxRadian<TReal>::operator<=(const OpaaxDegree<TReal>& Other) const
        {
            return m_value <= Other.GetRadian();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxRadian<TReal>::operator==(const OpaaxDegree<TReal>& Other) const
        {
            return m_value == Other.GetRadian();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxRadian<TReal>::operator!=(const OpaaxDegree<TReal>& Other) const
        {
            return m_value != Other.GetRadian();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxRadian<TReal>::operator>=(const OpaaxDegree<TReal>& Other) const
        {
            return m_value >= Other.GetRadian();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxRadian<TReal>::operator>(const OpaaxDegree<TReal>& Other) const
        {
            return m_value > Other.GetRadian();
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxRadian<TReal>& OpaaxRadian<TReal>::operator=(const OpaaxDegree<TReal>& Degree) const
        {
            m_value = Degree.GetRadian();
            return *this;
        }
#pragma endregion //OpaaxRadianImpl

        //-----------------------------------------------------------------
        // Opaax Degree Implementation
        //-----------------------------------------------------------------
#pragma region OpaaxDegreeImpl
        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal>::OpaaxDegree(const OpaaxRadian<TReal>& Radian): m_value{Radian.GetDegree()} {}

        template <CONCEPT_TIsFloat TReal>
        std::string OpaaxDegree<TReal>::ToString() const
        {
            return (BSTFormat("%1% Degree") % m_value).str();
        }

        template <CONCEPT_TIsFloat TReal>
        TReal OpaaxDegree<TReal>::GetRadian() const
        {
            return OpaaxRadian<TReal>(OpaaxMath::DegreesToRadians(m_value)).GetValue();
        }

        template <CONCEPT_TIsFloat TReal>
        void OpaaxDegree<TReal>::SetValue(const OpaaxRadian<TReal>& Radian)
        {
            m_value = Radian.GetDegree();
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal> OpaaxDegree<TReal>::operator+(const OpaaxRadian<TReal>& Other) const
        {
            return OpaaxDegree(m_value + Other.GetDegree());
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal> OpaaxDegree<TReal>::operator-(const OpaaxRadian<TReal>& Other) const
        {
            return OpaaxDegree(m_value - Other.GetDegree());
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal> OpaaxDegree<TReal>::operator*(const OpaaxRadian<TReal>& Other) const
        {
            return OpaaxDegree(m_value * Other.GetDegree());
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal> OpaaxDegree<TReal>::operator/(const OpaaxRadian<TReal>& Other) const
        {
            return OpaaxDegree(m_value / Other.GetDegree());
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal>& OpaaxDegree<TReal>::operator+=(const OpaaxRadian<TReal>& Other)
        {
            m_value += Other.GetDegree();
            return *this;
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal>& OpaaxDegree<TReal>::operator-=(const OpaaxRadian<TReal>& Other)
        {
            m_value -= Other.GetDegree();
            return *this;
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal>& OpaaxDegree<TReal>::operator*=(const OpaaxRadian<TReal>& Other)
        {
            m_value *= Other.GetDegree();
            return *this;
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal>& OpaaxDegree<TReal>::operator/=(const OpaaxRadian<TReal>& Other)
        {
            m_value /= Other.GetDegree();
            return *this;
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxDegree<TReal>::operator<(const OpaaxRadian<TReal>& Other) const
        {
            return m_value < Other.GetDegree();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxDegree<TReal>::operator<=(const OpaaxRadian<TReal>& Other) const
        {
            return m_value <= Other.GetDegree();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxDegree<TReal>::operator==(const OpaaxRadian<TReal>& Other) const
        {
            return m_value == Other.GetDegree();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxDegree<TReal>::operator!=(const OpaaxRadian<TReal>& Other) const
        {
            return m_value != Other.GetDegree();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxDegree<TReal>::operator>=(const OpaaxRadian<TReal>& Other) const
        {
            return m_value >= Other.GetDegree();
        }

        template <CONCEPT_TIsFloat TReal>
        bool OpaaxDegree<TReal>::operator>(const OpaaxRadian<TReal>& Other) const
        {
            return m_value > Other.GetDegree();
        }

        template <CONCEPT_TIsFloat TReal>
        OpaaxDegree<TReal>& OpaaxDegree<TReal>::operator=(const OpaaxRadian<TReal>& Radian) const
        {
            m_value = Radian.GetDegree();
            return *this;
        }
#pragma endregion //OpaaxDegreeImpl

        //-----------------------------------------------------------------
        // Opaax Angle Implementation
        //-----------------------------------------------------------------
#pragma region OpaaxAngleImpl
        template <CONCEPT_TIsFloat TReal>
        OSTDString  OpaaxAngle<TReal>::ToString() const
        {
            return (BSTFormat("%1% Rad, %2% Deg") %m_angle %GetAngleDegree()).str();
        }

        template <CONCEPT_TIsFloat TReal>
        void OpaaxAngle<TReal>::SetAngleDegree(TReal DegreeAngle)
        {
            m_angle = OpaaxMath::DegreesToRadians(DegreeAngle);
        }

        template <CONCEPT_TIsFloat TReal>
        void OpaaxAngle<TReal>::SetAngleDegree(const OpaaxDegree<TReal>& Degree)
        {
            m_angle = Degree.AsRadian();
        }
#pragma endregion //OpaaxAngleImpl
    }
}
