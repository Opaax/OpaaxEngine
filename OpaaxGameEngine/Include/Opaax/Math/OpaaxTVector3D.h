#pragma once
#include "OpaaxMath.h"
#include "OpaaxMathConst.h"
#include "Opaax/OpaaxStdTypes.h"
#include "Opaax/OpaaxTRequire.h"
#include "Opaax/Boost/OPBoostTypes.h"

namespace OPAAX
{
    namespace MATH
    {
        template <CONCEPT_TIsFloat T>
        struct OPAAX_API OpaaxTVector3D final
        {
        private:
            using FReal = T;

            //-----------------------------------------------------------------
            // Members
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//

            FReal X;
            FReal Y;
            FReal Z;

            static const OpaaxTVector3D<FReal> ZERO;
            static const OpaaxTVector3D<FReal> XUNIT;
            static const OpaaxTVector3D<FReal> YUNIT;
            static const OpaaxTVector3D<FReal> UP;
            static const OpaaxTVector3D<FReal> LEFT;
            static const OpaaxTVector3D<FReal> DOWN;
            static const OpaaxTVector3D<FReal> RIGHT;

            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
            OpaaxTVector3D();

            /**
             * Constructor initializing both X and Y components to the given values.
             *
             * @param InX Value to set the X component to.
             * @param InY Value to set the Y component to.
             * @param InZ Value to set the Z component to.
             */
            explicit FORCEINLINE OpaaxTVector3D(const FReal InX, const FReal InY, const FReal InZ);

            /**
            * Constructor initializing both components to a single T value.
            *
            * @param InF Value to set both components to.
            */
            explicit FORCEINLINE OpaaxTVector3D(const FReal InF);

            /**
             * Copy constructor that initializes a new TVector2D with the values from the provided TVector2D.
             *
             * @param Other The OpaaxTVector3D object to copy values from.
             */
            FORCEINLINE OpaaxTVector3D(const OpaaxTVector3D& Other);

            /**
             * Constructor initializing both X and Y components to the given values.
             *
             * @param InX Value to set the X component to.
             * @param InY Value to set the Y component to.
             * @param InZ Value to set the Z component to.
             */
            explicit FORCEINLINE OpaaxTVector3D(const Int32 InX, const Int32 InY, const Int32 InZ);

            /**
             * Constructor initializing both X and Y components using the provided integer values.
             *
             * @param InX The integer value to set the X component.
             * @param InY The integer value to set the Y component.
             * @param InZ The integer value to set the Z component.
             */
            explicit FORCEINLINE OpaaxTVector3D(const Int64 InX, const Int64 InY, const Int64 InZ);


            OpaaxTVector3D(OpaaxTVector3D&& Other) noexcept = default;
            ~OpaaxTVector3D() = default;

            //-----------------------------------------------------------------
            // Operation
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
            //-------------------------- Multiplication -----------------------------//
            /**
             * Multiplies the components of this vector by the components of another vector.
             *
             * @param Other The vector whose components will be multiplied with this vector's components.
             * @return A new vector resulting from the component-wise multiplication.
             */
            FORCEINLINE OpaaxTVector3D operator*(const OpaaxTVector3D& Other) const
            {
                return OpaaxTVector3D(X * Other.X, Y * Other.Y, Z * Other.Z);
            }

            /**
             * Multiplies the components of this vector by the components of another vector and assigns the result to this vector.
             *
             * @param Other The vector whose components are multiplied with this vector's components.
             * @return A reference to this vector after the operation.
             */
            FORCEINLINE OpaaxTVector3D operator*=(const OpaaxTVector3D& Other)
            {
                X *= Other.X;
                Y *= Other.Y;
                Z *= Other.Z;
                return *this;
            }

            /**
             * Multiplies both components of the vector by the specified scalar value and updates the vector with the result.
             *
             * @param Scalar The scalar value by which both X and Y components of the vector are multiplied.
             * @return A reference to the modified vector (this) after the multiplication operation.
             */
            FORCEINLINE OpaaxTVector3D& operator*=(const FReal Scalar)
            {
                X *= Scalar;
                Y *= Scalar;
                Z *= Scalar;
                return *this;
            }

            //-------------------------- Addition -----------------------------//
            /**
             * Adds the components of this vector with the components of another vector.
             *
             * @param Other The vector to add to this vector.
             * @return A new vector that is the result of adding the corresponding components of the two vectors.
             */
            FORCEINLINE OpaaxTVector3D operator+(const OpaaxTVector3D& Other) const
            {
                return OpaaxTVector3D{X + Other.X, Y + Other.Y, Z + Other.Z};
            }

            /**
             * Adds a scalar value to both components of the vector.
             *
             * @param Scalar The scalar value to be added to the X and Y components of the vector.
             * @return A new TVector2D instance with the scalar added to its components.
             */
            FORCEINLINE OpaaxTVector3D operator+(const FReal Scalar) const
            {
                return OpaaxTVector3D{X + Scalar, Y + Scalar, Z + Scalar};
            }

            /**
             * Adds the components of another vector to this vector and updates this vector.
             *
             * @param Other The vector whose components are to be added to this vector.
             * @return A reference to this updated vector instance.
             */
            FORCEINLINE OpaaxTVector3D& operator+=(const OpaaxTVector3D& Other)
            {
                X += Other.X;
                Y += Other.Y;
                Z += Other.Z;
                return *this;
            }

            /**
             * Adds the specified scalar value to both components of the vector
             * and updates the vector with the result.
             *
             * @param Scalar The scalar value to add to both the X and Y components.
             * @return A reference to the modified vector (this) after the addition operation.
             */
            FORCEINLINE OpaaxTVector3D& operator+=(const FReal Scalar)
            {
                X += Scalar;
                Y += Scalar;
                Z += Scalar;
                return *this;
            }

            //-------------------------- Subtraction -----------------------------//
            /**
             * Subtracts the components of another vector from this vector and returns the result.
             *
             * @param Other The vector to subtract from this vector.
             * @return A new vector that represents the result of the subtraction.
             */
            FORCEINLINE OpaaxTVector3D operator-(const OpaaxTVector3D& Other) const
            {
                return OpaaxTVector3D(X - Other.X, Y - Other.Y, Z - Other.Z);
            }

            /**
             * Subtracts a scalar value from both components of the vector and returns the resulting vector.
             *
             * @param Scalar The scalar value to subtract from the X and Y components.
             * @return A new TVector2D instance with the scalar subtracted from both components.
             */
            FORCEINLINE OpaaxTVector3D operator-(const FReal Scalar) const
            {
                return OpaaxTVector3D(X - Scalar, Y - Scalar, Z - Scalar);
            }

            /**
             * Subtracts the components of the given vector from the current vector and assigns the result to the current vector.
             *
             * @param Other The vector to be subtracted.
             * @return A reference to this vector after modification.
             */
            FORCEINLINE OpaaxTVector3D& operator-=(const OpaaxTVector3D& Other)
            {
                X -= Other.X;
                Y -= Other.Y;
                Z -= Other.Z;
                return *this;
            }

            //-------------------------- Division -----------------------------//
            /**
             * Divides the current vector by the given vector component-wise.
             *
             * @param Other The vector to divide the current vector by.
             * @return A new vector resulting from the division of corresponding components.
             */
            FORCEINLINE OpaaxTVector3D operator/(const OpaaxTVector3D& Other) const
            {
                return OpaaxTVector3D(X / Other.X, Y / Other.Y, Z / Other.Z);
            }

            /**
             * Divides the components of this vector by a scalar value and returns the resulting vector.
             *
             * @param Scalar The scalar value to divide this vector's components by.
             * @return A new vector with each component divided by the given scalar value.
             */
            FORCEINLINE OpaaxTVector3D operator/(const FReal Scalar) const
            {
                return OpaaxTVector3D(X / Scalar, Y / Scalar, Z / Scalar);
            }

            /**
             * Divides the components of this vector by the corresponding components of another vector
             * and updates this vector with the result.
             *
             * @param Other The vector whose components are used as divisors for the X and Y components of this vector.
             * @return A reference to the modified vector (this) after the division operation.
             */
            FORCEINLINE OpaaxTVector3D operator/=(const OpaaxTVector3D& Other)
            {
                X /= Other.X;
                Y /= Other.Y;
                Z /= Other.Z;
                return *this;
            }

            /**
             * Divides both components of the vector by the specified scalar value and updates the vector with the result.
             *
             * @param Scalar The scalar value by which both X and Y components of the vector are divided.
             * @return A reference to the modified vector (this) after the division operation.
             */
            FORCEINLINE OpaaxTVector3D& operator/=(const FReal Scalar)
            {
                X /= Scalar;
                Y /= Scalar;
                Z /= Scalar;
                return *this;
            }
            
            //-------------------------- Equals -----------------------------//
            OpaaxTVector3D& operator=(const OpaaxTVector3D& Other)
            {
                if (this == &Other)
                {
                    return *this;
                }

                X = Other.X;
                Y = Other.Y;
                Z = Other.Z;

                return *this;
            }

            OpaaxTVector3D& operator=(OpaaxTVector3D&& Other) noexcept = default;

            //-------------------------- Other  -----------------------------//
            friend std::ostream& operator<<(std::ostream& OS, const OpaaxTVector3D& Vector)
            {
                return OS << (BSTFormat("{[X: %1%],[Y: %2%], [Z: %3%]}") % Vector.X % Vector.Y % Vector.Z).str();
            }

            //-------------------------- Dot  -----------------------------//
            /**
            * Calculates dot product of this vector and another.
            * Collinear = 1
            * Opposite = -1
            * Perpendicular = 0
            * Same Direction > 0, angle less than 90
            * Opposite Direction < 0, angle greater than 90
            * @param Other The other vector.
            * @return The dot product.
            */
            FORCEINLINE FReal operator|(const OpaaxTVector3D& Other) const
            {
                return X * Other.X + Y * Other.Y + Z * Other.Z;
            }

            //-------------------------- Cross  -----------------------------//
            /**
             * Calculates the Cross product of this vector and another vector.
             *
             * @param Other The vector to calculate the perpendicular dot product with.
             * @return The Cross product of the two vectors.
             */
            FORCEINLINE OpaaxTVector3D<FReal> operator^(const OpaaxTVector3D& Other) const
            {
                return Y * Other.Z - Z * Other.Y, Z * Other.X - X * Other.Z, X * Other.Y - Y * Other.X;
            }

            //-----------------------------------------------------------------
            // Functions
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
            /**
             * Returns a vector with both X and Y components set to zero.
             *
             * @return A TVector2D instance where both components are zero.
             */
            static FORCEINLINE OpaaxTVector3D<T> Zero() { return OpaaxTVector3D<T>::ZERO; }

            /**
             * Retrieves the unit vector along the X-axis.
             *
             * @return A TVector2D object representing the unit vector (1, 0) along the X-axis.
             */
            static FORCEINLINE OpaaxTVector3D<T> UnitX() { return OpaaxTVector3D<T>::XUNIT; }

            /**
             * Returns a unit vector representing the Y-axis direction.
             *
             * @return A TVector2D object representing the unit vector (0, 1) along the Y-axis.
             */
            static FORCEINLINE OpaaxTVector3D<T> UnitY() { return OpaaxTVector3D<T>::YUNIT; }

            /**
             * Provides the 'Up' direction vector for the TVector2D type.
             *
             * @return A TVector2D object representing the upward direction vector (0, 1).
             */
            static FORCEINLINE OpaaxTVector3D<T> Up() { return OpaaxTVector3D<T>::UP; }

            /**
             * Returns a pre-defined TVector2D representing the left direction.
             *
             * @return A TVector2D object representing the left direction vector (-1, 0)
             */
            static FORCEINLINE OpaaxTVector3D<T> Left() { return OpaaxTVector3D<T>::LEFT; }

            /**
             * Returns a TVector2D instance representing the downward directional vector.
             *
             * @return A TVector2D object representing the down direction vector (0, -1)
             */
            static FORCEINLINE OpaaxTVector3D<T> Down() { return OpaaxTVector3D<T>::DOWN; }

            /**
             * Returns a vector representing the right direction.
             *
             * @return A TVector2D object representing the right direction vector (1, 0)
             */
            static FORCEINLINE OpaaxTVector3D<T> Right() { return OpaaxTVector3D<T>::RIGHT; }

            /**
             * Calculates the squared magnitude of the vector (more efficient than Magnitude for comparisons).
             * Dot to itself
             * 
             * @return The squared magnitude of this vector.
             */
            FORCEINLINE FReal MagnitudeSquared() const
            {
                return this | this;
            }

            /**
             * Calculates the magnitude (length) of the vector.
             *
             * @return The magnitude of this vector.
             */
            FORCEINLINE FReal Magnitude() const
            {
                return OpaaxMath::Sqrt(MagnitudeSquared());
            }

            /**
             * Returns a normalized version of this vector (same direction, magnitude of 1).
             *
             * @return A new TVector2D that is normalized.
             */
            OpaaxTVector3D GetSafeNormalized(FReal Tolerance = OP_SMALL_NUMBER) const;

            /**
             * Normalizes the vector to have a unit magnitude if its magnitude is greater than the specified tolerance.
             *
             * @param Tolerance A float value specifying the threshold below which the vector is treated as zero.
             * @return true if the normalization was successful; false if the vector's magnitude was less than or equal to the tolerance (in which case, the vector is set to zero).
             */
            bool Normalize(FReal Tolerance = OP_SMALL_NUMBER);

            /**
             * Calculates the squared distance to another vector.
             *
             * @param Other The other vector to calculate the squared distance to.
             * @return The squared distance to the other vector as a scalar value.
             */
            FReal DistanceToSquared(const OpaaxTVector3D& Other);

            /**
             * Calculates the Euclidean distance from this vector to another vector.
             *
             * @param Other The vector to calculate the distance to.
             * @return The distance between this vector and the specified vector.
             */
            FReal DistanceTo(const OpaaxTVector3D& Other);

            bool IsZero() const;


            /**
             * Projects this vector onto another vector B.
             *
             * @param B The vector onto which this vector will be projected.
             * @return The projection of this vector onto B, or a zero vector if B's magnitude is zero.
             */
            OpaaxTVector3D ProjectOnB(const OpaaxTVector3D& B)
            {
                const OpaaxTVector3D& A = *this; // Reference to this vector
                const FReal lDotProduct = A | B; // Dot product of this vector and B
                const FReal bMagnitudeSquared = B.MagnitudeSquared(); // Squared magnitude of B
                
                if (bMagnitudeSquared > 0) // Ensure we don't divide by zero
                {
                    return B * (lDotProduct / bMagnitudeSquared); // Vector projection formula: (A·B / B·B) * B
                }
                
                return OpaaxTVector3D::ZERO; // Return a zero vector if B's magnitude squared is zero
            }
        };

        template <CONCEPT_TIsFloat T>
        OpaaxTVector3D<T>::OpaaxTVector3D(): X{0}, Y{0}, Z{0} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector3D<T>::OpaaxTVector3D(const FReal InX, const FReal InY, const FReal InZ): X{InX}, Y{InY}, Z{InZ} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector3D<T>::OpaaxTVector3D(const FReal InF): X{InF}, Y{InF}, Z{InF} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector3D<T>::OpaaxTVector3D(const OpaaxTVector3D& Other): X{Other.X}, Y{Other.Y}, Z{Other.Z} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector3D<T>::OpaaxTVector3D(const Int32 InX, const Int32 InY, const Int32 InZ)
        :   X{static_cast<FReal>(InX)},
            Y{static_cast<FReal>(InY)},
            Z{static_cast<FReal>(InZ)} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector3D<T>::OpaaxTVector3D(const Int64 InX, const Int64 InY, const Int64 InZ)
        :   X{static_cast<FReal>(InX)},
            Y{static_cast<FReal>(InY)},
            Z{static_cast<FReal>(InZ)} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector3D<T> OpaaxTVector3D<T>::GetSafeNormalized(FReal Tolerance) const
        {
            const T lSquareSum = MagnitudeSquared();

            if (lSquareSum > Tolerance)
            {
                const T lScale = OpaaxMath::InvSqrt(lSquareSum);

                return OpaaxTVector3D{X * lScale, Y * lScale, Z * lScale};
            }

            return ZERO;
        }

        template <CONCEPT_TIsFloat T>
        bool OpaaxTVector3D<T>::Normalize(FReal Tolerance)
        {
            const T lSquareSum = MagnitudeSquared();

            if(lSquareSum > Tolerance)
            {
                const T lScale = OpaaxMath::InvSqrt(lSquareSum);
    
                X *= lScale;
                Y *= lScale;
                Z *= lScale;
    
                return true;
            }

            X = 0.0f;
            Y = 0.0f;
            Z = 0.0f;

            return false;
        }

        template <CONCEPT_TIsFloat T>
        typename OpaaxTVector3D<T>::FReal OpaaxTVector3D<T>::DistanceToSquared(const OpaaxTVector3D& Other)
        {
            return OpaaxMath::Square(X - Other.X) + OpaaxMath::Square(Y - Other.Y) + OpaaxMath::Square(Z - Other.Z);
        }

        template <CONCEPT_TIsFloat T>
        typename OpaaxTVector3D<T>::FReal OpaaxTVector3D<T>::DistanceTo(const OpaaxTVector3D& Other)
        {
            return OpaaxMath::Sqrt(OpaaxTVector3D<T>::DistanceToSquared(Other));
        }

        template <CONCEPT_TIsFloat T>
        bool OpaaxTVector3D<T>::IsZero() const
        {
            return OpaaxMath::IsZero(X) && OpaaxMath::IsZero(Y) && OpaaxMath::IsZero(Z);
        }
    }
}
