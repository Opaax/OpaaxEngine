#pragma once
#include "OpaaxMath.h"
#include "OpaaxMathConst.h"
#include "Opaax/Boost/OPBoostTypes.h"

namespace OPAAX
{
    namespace MATH
    {
        /**
         *@struct OpaaxTVector2D
         * Represents a 2D vector that supports various arithmetic operations such
         * as addition, subtraction, multiplication, and division, both component-wise
         * and with scalar values.
         *
         * @tparam T The template type representing the data type of the vector's components.
         */
        template <CONCEPT_TIsFloat T>
        struct OPAAX_API OpaaxTVector2D final
        {
        private:
            using FReal = T;

            //-----------------------------------------------------------------
            // Members
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
        public:
            FReal X;
            FReal Y;

            static const OpaaxTVector2D<FReal> ZERO;
            static const OpaaxTVector2D<FReal> XUNIT;
            static const OpaaxTVector2D<FReal> YUNIT;
            static const OpaaxTVector2D<FReal> UP;
            static const OpaaxTVector2D<FReal> LEFT;
            static const OpaaxTVector2D<FReal> DOWN;
            static const OpaaxTVector2D<FReal> RIGHT;

            //-----------------------------------------------------------------
            // CTOR/DTOR
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
            OpaaxTVector2D();
            
            /**
             * Constructor initializing both X and Y components to the given values.
             *
             * @param InX Value to set the X component to.
             * @param InY Value to set the Y component to.
             * @return None
             */
            explicit FORCEINLINE OpaaxTVector2D(const FReal InX, const FReal InY);

            /**
            * Constructor initializing both components to a single T value.
            *
            * @param InF Value to set both components to.
            */
            explicit FORCEINLINE OpaaxTVector2D(const FReal InF);

            /**
             * Copy constructor that initializes a new TVector2D with the values from the provided TVector2D.
             *
             * @param Other The TVector2D object to copy values from.
             */
            FORCEINLINE OpaaxTVector2D(const OpaaxTVector2D& Other);

            /**
             * Constructor initializing both X and Y components to the given values.
             *
             * @param InX Value to set the X component to.
             * @param InY Value to set the Y component to.
             * @return None
             */
            explicit FORCEINLINE OpaaxTVector2D(const Int32 InX, const Int32 InY);

            /**
             * Constructor initializing both X and Y components using the provided integer values.
             *
             * @param InX The integer value to set the X component.
             * @param InY The integer value to set the Y component.
             * @return None
             */
            explicit FORCEINLINE OpaaxTVector2D(const Int64 InX, const Int64 InY);
            
            OpaaxTVector2D(OpaaxTVector2D&& Other) noexcept = default;
            ~OpaaxTVector2D();

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
            FORCEINLINE OpaaxTVector2D operator*(const OpaaxTVector2D& Other) const
            {
                return OpaaxTVector2D(X * Other.X, Y * Other.Y);
            }

            /**
             * Multiplies the components of this vector by the components of another vector and assigns the result to this vector.
             *
             * @param Other The vector whose components are multiplied with this vector's components.
             * @return A reference to this vector after the operation.
             */
            FORCEINLINE OpaaxTVector2D operator*=(const OpaaxTVector2D& Other)
            {
                X *= Other.X;
                Y *= Other.Y;
                return *this;
            }

            /**
             * Multiplies both components of the vector by the specified scalar value and updates the vector with the result.
             *
             * @param Scalar The scalar value by which both X and Y components of the vector are multiplied.
             * @return A reference to the modified vector (this) after the multiplication operation.
             */
            FORCEINLINE OpaaxTVector2D& operator*=(const FReal Scalar)
            {
                X *= Scalar;
                Y *= Scalar;
                return *this;
            }

            //-------------------------- Addition -----------------------------//
            /**
             * Adds the components of this vector with the components of another vector.
             *
             * @param Other The vector to add to this vector.
             * @return A new vector that is the result of adding the corresponding components of the two vectors.
             */
            FORCEINLINE OpaaxTVector2D operator+(const OpaaxTVector2D& Other) const
            {
                return OpaaxTVector2D{X + Other.X, Y + Other.Y};
            }

            /**
             * Adds a scalar value to both components of the vector.
             *
             * @param Scalar The scalar value to be added to the X and Y components of the vector.
             * @return A new TVector2D instance with the scalar added to its components.
             */
            FORCEINLINE OpaaxTVector2D operator+(const FReal Scalar) const
            {
                return OpaaxTVector2D{X + Scalar, Y + Scalar};
            }

            /**
             * Adds the components of another vector to this vector and updates this vector.
             *
             * @param Other The vector whose components are to be added to this vector.
             * @return A reference to this updated vector instance.
             */
            FORCEINLINE OpaaxTVector2D& operator+=(const OpaaxTVector2D& Other)
            {
                X += Other.X;
                Y += Other.Y;
                return *this;
            }

            /**
             * Adds the specified scalar value to both components of the vector
             * and updates the vector with the result.
             *
             * @param Scalar The scalar value to add to both the X and Y components.
             * @return A reference to the modified vector (this) after the addition operation.
             */
            FORCEINLINE OpaaxTVector2D& operator+=(const FReal Scalar)
            {
                X += Scalar;
                Y += Scalar;
                return *this;
            }

            //-------------------------- Subtraction -----------------------------//
            /**
             * Subtracts the components of another vector from this vector and returns the result.
             *
             * @param Other The vector to subtract from this vector.
             * @return A new vector that represents the result of the subtraction.
             */
            FORCEINLINE OpaaxTVector2D operator-(const OpaaxTVector2D& Other) const
            {
                return OpaaxTVector2D(X - Other.X, Y - Other.Y);
            }

            /**
             * Subtracts a scalar value from both components of the vector and returns the resulting vector.
             *
             * @param Scalar The scalar value to subtract from the X and Y components.
             * @return A new TVector2D instance with the scalar subtracted from both components.
             */
            FORCEINLINE OpaaxTVector2D operator-(const FReal Scalar) const
            {
                return OpaaxTVector2D(X - Scalar, Y - Scalar);
            }

            /**
             * Subtracts the components of the given vector from the current vector and assigns the result to the current vector.
             *
             * @param Other The vector to be subtracted.
             * @return A reference to this vector after modification.
             */
            FORCEINLINE OpaaxTVector2D& operator-=(const OpaaxTVector2D& Other)
            {
                X -= Other.X;
                Y -= Other.Y;
                return *this;
            }

            //-------------------------- Division -----------------------------//
            /**
             * Divides the current vector by the given vector component-wise.
             *
             * @param Other The vector to divide the current vector by.
             * @return A new vector resulting from the division of corresponding components.
             */
            FORCEINLINE OpaaxTVector2D operator/(const OpaaxTVector2D& Other) const
            {
                return OpaaxTVector2D(X / Other.X, Y / Other.Y);
            }

            /**
             * Divides the components of this vector by a scalar value and returns the resulting vector.
             *
             * @param Scalar The scalar value to divide this vector's components by.
             * @return A new vector with each component divided by the given scalar value.
             */
            FORCEINLINE OpaaxTVector2D operator/(const FReal Scalar) const
            {
                return OpaaxTVector2D(X / Scalar, Y / Scalar);
            }

            /**
             * Divides the components of this vector by the corresponding components of another vector
             * and updates this vector with the result.
             *
             * @param Other The vector whose components are used as divisors for the X and Y components of this vector.
             * @return A reference to the modified vector (this) after the division operation.
             */
            FORCEINLINE OpaaxTVector2D operator/=(const OpaaxTVector2D& Other)
            {
                X /= Other.X;
                Y /= Other.Y;
                return *this;
            }

            /**
             * Divides both components of the vector by the specified scalar value and updates the vector with the result.
             *
             * @param Scalar The scalar value by which both X and Y components of the vector are divided.
             * @return A reference to the modified vector (this) after the division operation.
             */
            FORCEINLINE OpaaxTVector2D& operator/=(const FReal Scalar)
            {
                X /= Scalar;
                Y /= Scalar;
                return *this;
            }
            
            //-------------------------- Equals -----------------------------//
            OpaaxTVector2D& operator=(const OpaaxTVector2D& Other)
            {
                if (this == &Other)
                {
                    return *this;
                }

                X = Other.X;
                Y = Other.Y;
                return *this;
            }

            OpaaxTVector2D& operator=(OpaaxTVector2D&& Other) noexcept = default;

            //-------------------------- Other  -----------------------------//
            friend std::ostream& operator<<(std::ostream& OS, const OpaaxTVector2D& Vector)
            {
                return OS << (BSTFormat("{[X: %1%],[Y: %2%]}") %Vector.X %Vector.Y).str();
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
            FORCEINLINE FReal operator|(const OpaaxTVector2D& Other) const
            {
                return X * Other.X + Y * Other.Y;
            }

            //-----------------------------------------------------------------
            // Functions
            //-----------------------------------------------------------------
            //-------------------------- Public -----------------------------//
            /**
             * Returns a vector with X, Y and Z components set to zero.
             *
             * @return [0,0,0] vector.
             */
            static FORCEINLINE OpaaxTVector2D<FReal> Zero() { return OpaaxTVector2D<T>::ZERO; }

            /**
             * Returns a unit vector representing the X axis.
             *
             * @return A unit vector along the X axis.
             */
            static FORCEINLINE OpaaxTVector2D<FReal> UnitX() { return OpaaxTVector2D<T>::XUNIT; }

            /**
             * Returns a vector representing the unit vector along the Y-axis.
             *
             * @return A vector with a magnitude of 1 pointing in the positive Y direction.
             */
            static FORCEINLINE OpaaxTVector2D<FReal> UnitY() { return OpaaxTVector2D<T>::YUNIT; }

            /**
             * Returns the constant vector representing the upward direction.
             *
             * @return A constant vector pointing upwards.
             */
            static FORCEINLINE OpaaxTVector2D<FReal> Up() { return OpaaxTVector2D<T>::UP; }

            /**
             * Returns a vector representing the left direction.
             *
             * @return A vector constant representing the left direction.
             */
            static FORCEINLINE OpaaxTVector2D<FReal> Left() { return OpaaxTVector2D<T>::LEFT; }

            /**
             * Returns a vector representing the down direction.
             *
             * @return A vector pointing in the down direction.
             */
            static FORCEINLINE OpaaxTVector2D<FReal> Down() { return OpaaxTVector2D<T>::DOWN; }

            /**
             * Retrieves a vector representing the right direction.
             *
             * @return A vector pointing to the right direction.
             */
            static FORCEINLINE OpaaxTVector2D<FReal> Right() { return OpaaxTVector2D<T>::RIGHT; }

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
             * Returns a normalized vector if the magnitude squared exceeds the specified tolerance, otherwise returns a zero vector.
             *
             * @param Tolerance The threshold below which the vector is considered too small to be normalized.
             * @return A safely normalized vector or a zero vector if below the tolerance.
             */
            OpaaxTVector2D GetSafeNormalized(FReal Tolerance = OP_SMALL_NUMBER) const;

            /**
             * Normalizes the vector to have a magnitude of 1 if its magnitude is above the specified tolerance.
             * If the magnitude is below or equal to the tolerance, the vector is set to zero.
             *
             * @param Tolerance The threshold below which the vector is considered too small to normalize.
             * @return True if the vector was successfully normalized, false if the vector was set to zero.
             */
            bool Normalize(FReal Tolerance = OP_SMALL_NUMBER);

            /**
             * Computes the squared distance from this vector to another vector.
             *
             * @param Other The other vector to which the squared distance is calculated.
             * @return The squared distance between this vector and the specified vector.
             */
            FReal DistanceToSquared(const OpaaxTVector2D& Other);

            /**
             * Calculates the Euclidean distance from this vector to another vector.
             *
             * @param Other The other vector to calculate the distance to.
             * @return The Euclidean distance between this vector and the specified vector.
             */
            FReal DistanceTo(const OpaaxTVector2D& Other);

            bool IsZero() const;


            /**
             * Projects this vector onto another vector.
             *
             * @param Other The vector onto which this vector will be projected.
             * @return The projection of this vector onto the other, or a zero vector if B's magnitude is zero.
             */
            OpaaxTVector2D ProjectOn(const OpaaxTVector2D& Other)
            {
                const OpaaxTVector2D& A = *this; // Reference to this vector
                const FReal lDotProduct = A | Other; // Dot product of this vector and Other
                const FReal bMagnitudeSquared = Other.MagnitudeSquared(); // Squared magnitude of Other
                
                if (bMagnitudeSquared > 0) // Ensure we don't divide by zero
                {
                    return OpaaxTVector2D((lDotProduct / bMagnitudeSquared) * Other.X, (lDotProduct / bMagnitudeSquared) * Other.Y); // Vector projection for 2D
                }
                
                return OpaaxTVector2D::ZERO; // Return a zero vector if Other's magnitude squared is zero
            }
        };

        template <CONCEPT_TIsFloat T>
        OpaaxTVector2D<T>::OpaaxTVector2D():X{0}, Y {0}{}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector2D<T>::OpaaxTVector2D(const FReal InX, const FReal InY):X{InX}, Y {InY} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector2D<T>::OpaaxTVector2D(const FReal InF):X{InF}, Y {InF} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector2D<T>::OpaaxTVector2D(const OpaaxTVector2D& Other):X{Other.X}, Y {Other.Y} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector2D<T>::OpaaxTVector2D(const Int32 InX, const Int32 InY):X{static_cast<FReal>(InX)}, Y {static_cast<FReal>(InY)} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector2D<T>::OpaaxTVector2D(const Int64 InX, const Int64 InY):X{static_cast<FReal>(InX)}, Y {static_cast<FReal>(InY)} {}

        template <CONCEPT_TIsFloat T>
        OpaaxTVector2D<T> OpaaxTVector2D<T>::GetSafeNormalized(FReal Tolerance) const
        {
            const T lSquareSum = MagnitudeSquared();
    
            if(lSquareSum > Tolerance)
            {
                const T lScale = OpaaxMath::InvSqrt(lSquareSum);
        
                return OpaaxTVector2D{X * lScale, Y * lScale};
            }

            return ZERO;
        }

        template <CONCEPT_TIsFloat T>
        bool OpaaxTVector2D<T>::Normalize(FReal Tolerance)
        {
            const T lSquareSum = MagnitudeSquared();

            if(lSquareSum > Tolerance)
            {
                const T lScale = OpaaxMath::InvSqrt(lSquareSum);
    
                X *= lScale;
                Y *= lScale;
    
                return true;
            }

            X = 0.0f;
            Y = 0.0f;

            return false;
        }

        template <CONCEPT_TIsFloat T>
        typename OpaaxTVector2D<T>::FReal OpaaxTVector2D<T>::DistanceToSquared(const OpaaxTVector2D& Other)
        {
            return OpaaxMath::Square(X - Other.X) + OpaaxMath::Square(Y - Other.Y);
        }

        
        template <CONCEPT_TIsFloat T>
        typename OpaaxTVector2D<T>::FReal OpaaxTVector2D<T>::DistanceTo(const OpaaxTVector2D& Other)
        {
            return OpaaxMath::Sqrt(OpaaxTVector2D<T>::DistanceToSquared(Other));
        }

        template <CONCEPT_TIsFloat T>
        bool OpaaxTVector2D<T>::IsZero() const
        {
            return OpaaxMath::IsZero(X) && OpaaxMath::IsZero(Y);
        }

        template <CONCEPT_TIsFloat T>
        OpaaxTVector2D<T>::~OpaaxTVector2D() = default;
    }
}