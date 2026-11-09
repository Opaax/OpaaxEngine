#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // DynamicLibrary — a shared library opened while the program runs (a .dll, .so or .dylib), and
    //   the functions found in it. For a library the program must run without, such as a store's SDK:
    //   nothing links it, so a missing file is an error to report, not a program that cannot start.
    //   Closed when destroyed. Implemented in Platform/Windows and Platform/Posix.
    // =============================================================================
    class DynamicLibrary
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        DynamicLibrary() = default;
        ~DynamicLibrary() { Close(); }

        DynamicLibrary(const DynamicLibrary&)            = delete;
        DynamicLibrary& operator=(const DynamicLibrary&) = delete;

        DynamicLibrary(DynamicLibrary&& InOther) noexcept
            : m_Handle(InOther.m_Handle), m_Path(Move(InOther.m_Path)), m_Error(Move(InOther.m_Error))
        {
            InOther.m_Handle = nullptr;
        }

        DynamicLibrary& operator=(DynamicLibrary&& InOther) noexcept
        {
            if (this != &InOther)
            {
                Close();
                m_Handle         = InOther.m_Handle;
                m_Path           = Move(InOther.m_Path);
                m_Error          = Move(InOther.m_Error);
                InOther.m_Handle = nullptr;
            }
            return *this;
        }

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Opens InPath, closing what was open. A full path is the safe form for a library shipped
         * beside the game; a bare file name is looked for where the system keeps its libraries (and,
         * on Windows, beside the executable), never in the working directory.
         * @return False when the file is missing or is not a library this program can load; GetError
         *         says why
         */
        bool Open(const OpaaxString& InPath);

        /** Closes the library. Pointers found in it must not be used after. */
        void Close() noexcept;

        /**
         * The address of the exported symbol InName.
         * @return Null when the library is not open or does not export it; GetError says why
         */
        void* FindSymbol(const char* InName);

        /** FindSymbol, as a function pointer of type TFunctionPtr. */
        template<typename TFunctionPtr>
        TFunctionPtr FindFunction(const char* InName)
        {
            return reinterpret_cast<TFunctionPtr>(FindSymbol(InName));
        }

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        bool               IsOpen() const noexcept   { return m_Handle != nullptr; }
        const OpaaxString& GetPath() const noexcept  { return m_Path; }

        /** Why the last Open or FindSymbol failed. Empty after a success. */
        const OpaaxString& GetError() const noexcept { return m_Error; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        void*       m_Handle = nullptr;
        OpaaxString m_Path;
        OpaaxString m_Error;
    };
}
