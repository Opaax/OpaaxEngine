#pragma once

namespace Opaax
{
    /**
     * Owning pointer, like std::unique_ptr.
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
         * Deletes the current pointer and takes NewPtr.
         */
        void Reset(T* NewPtr = nullptr)
        {
            delete Ptr;
            Ptr = NewPtr;
        }

        /**
         * Gives up ownership without deleting.
         */
        T* Release()
        {
            T* Result = Ptr;
            Ptr = nullptr;
            return Result;
        }

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
