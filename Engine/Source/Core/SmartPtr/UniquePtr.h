#pragma once

namespace Opaax
{
    /**
     * @class UniquePtr
     * @tparam T 
     */
    template <typename T>
    class UniquePtr
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit UniquePtr(T* Ptr = nullptr)
        : Ptr(Ptr)
        {
        }

        ~UniquePtr()
        {
            delete Ptr;
        }
        
        // =============================================================================
        // Copy - Delete
        // =============================================================================

        UniquePtr(const UniquePtr& Other) = delete;
        UniquePtr& operator=(const UniquePtr& Other) = delete;
        
        // =============================================================================
        // Move
        // =============================================================================

        UniquePtr(UniquePtr&& Other) noexcept : Ptr(Other.Ptr)
        {
            Other.Ptr = nullptr;
        }

        UniquePtr& operator=(UniquePtr&& Other) noexcept
        {
            if (this == &Other)
            {
                return *this;
            }

            Reset(Other.Ptr);
            Other.Ptr = nullptr;

            return *this;
        }
        
        // =============================================================================
        // Function
        // =============================================================================
    public:
        /**
         * 
         * @param NewPtr 
         */
        void Reset(T* NewPtr = nullptr)
        {
            delete Ptr;
            Ptr = NewPtr;
        }

        /**
         * 
         * @return 
         */
        T* Release()
        {
            T* Result = Ptr;
            Ptr = nullptr;
            return Result;
        }

        /**
         * 
         * @return 
         */
        T* Get() const
        {
            return Ptr;
        }
        
        bool IsValid() const
        {
            return Ptr != nullptr;
        }
        
        // =============================================================================
        // Operators
        // =============================================================================
    public:
        T* operator->() const
        {
            return Ptr;
        }

        T& operator*() const
        {
            return *Ptr;
        }

        explicit operator bool() const
        {
            return IsValid();
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        T* Ptr = nullptr;
    };
}
