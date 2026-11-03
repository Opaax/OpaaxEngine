#include "Core/IO/FileIO.h"

#include "Core/String/OpaaxUtf8.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

// The atomic writes flush to the disk and replace a file in one step: OS calls.
#if defined(OPAAX_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Opaax::FileIO
{
    namespace fs = std::filesystem;

    OpaaxString ReadAllText(const OpaaxString& InAbsPath)
    {
        if (InAbsPath.IsEmpty())
        {
            return {};
        }

        // Binary, then CRLF -> LF by hand: the same result on every platform.
        std::ifstream lFile(Utf8::ToFsPath(InAbsPath), std::ios::binary);
        if (!lFile.is_open())
        {
            return {};
        }

        std::stringstream lBuffer;
        lBuffer << lFile.rdbuf();
        std::string lText = lBuffer.str();

        std::string::size_type lWrite = 0;
        for (std::string::size_type lRead = 0; lRead < lText.size(); ++lRead)
        {
            if (lText[lRead] == '\r' && lRead + 1 < lText.size() && lText[lRead + 1] == '\n')
            {
                continue;
            }
            lText[lWrite++] = lText[lRead];
        }
        lText.resize(lWrite);

        return OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size()));
    }

    bool ReadAllBytes(const OpaaxString& InAbsPath, TDynArray<Uint8>& OutBytes)
    {
        if (InAbsPath.IsEmpty())
        {
            return false;
        }

        std::ifstream lFile(Utf8::ToFsPath(InAbsPath), std::ios::binary | std::ios::ate);
        if (!lFile.is_open())
        {
            return false;
        }

        const std::streamsize lSize = lFile.tellg();
        if (lSize < 0)
        {
            return false;
        }

        // Fill a local first: OutBytes stays untouched on failure.
        TDynArray<Uint8> lBytes(static_cast<size_t>(lSize));

        lFile.seekg(0, std::ios::beg);
        if (lSize > 0 && !lFile.read(reinterpret_cast<char*>(lBytes.data()), lSize))
        {
            return false;
        }

        OutBytes = Move(lBytes);
        return true;
    }

    namespace
    {
        /** The parent folders of InPath, created if missing. */
        bool EnsureParentDirectory(const fs::path& InPath)
        {
            if (!InPath.has_parent_path())
            {
                return true;
            }

            // Returns false if the directory already existed, so check that it exists now.
            std::error_code lError;
            fs::create_directories(InPath.parent_path(), lError);
            return fs::is_directory(InPath.parent_path(), lError) && !lError;
        }
    }

    bool WriteAllText(const OpaaxString& InAbsPath, const OpaaxString& InText)
    {
        if (InAbsPath.IsEmpty())
        {
            return false;
        }

        const fs::path lPath = Utf8::ToFsPath(InAbsPath);
        if (!EnsureParentDirectory(lPath))
        {
            return false;
        }

        // Binary: text mode would turn every LF into CRLF on Windows.
        std::ofstream lFile(lPath, std::ios::binary | std::ios::trunc);
        if (!lFile.is_open())
        {
            return false;
        }

        lFile.write(InText.CStr(), static_cast<std::streamsize>(InText.GetLength()));
        return lFile.good();
    }

    bool WriteAllBytes(const OpaaxString& InAbsPath, const Uint8* InData, const Uint64 InSize)
    {
        if (InAbsPath.IsEmpty() || (InData == nullptr && InSize > 0))
        {
            return false;
        }

        const fs::path lPath = Utf8::ToFsPath(InAbsPath);
        if (!EnsureParentDirectory(lPath))
        {
            return false;
        }

        std::ofstream lFile(lPath, std::ios::binary | std::ios::trunc);
        if (!lFile.is_open())
        {
            return false;
        }

        lFile.write(reinterpret_cast<const char*>(InData), static_cast<std::streamsize>(InSize));
        return lFile.good();
    }

    // =============================================================================
    // Atomic writes
    // =============================================================================
    namespace
    {
#if defined(OPAAX_PLATFORM_WINDOWS)
        /** Writes InData to a new file at InPath, flushed to the disk and not only to the OS's cache. */
        bool WriteFlushed(const fs::path& InPath, const Uint8* InData, const Uint64 InSize)
        {
            const HANDLE lFile = CreateFileW(InPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                             FILE_ATTRIBUTE_NORMAL, nullptr);
            if (lFile == INVALID_HANDLE_VALUE)
            {
                return false;
            }

            // WriteFile takes a 32-bit size: larger data goes in chunks.
            constexpr Uint64 MAX_CHUNK = 1ull << 30;

            bool   bWritten = true;
            Uint64 lOffset  = 0;
            while (bWritten && lOffset < InSize)
            {
                const Uint64 lLeft  = InSize - lOffset;
                const DWORD  lChunk = static_cast<DWORD>(lLeft < MAX_CHUNK ? lLeft : MAX_CHUNK);
                DWORD        lDone  = 0;

                bWritten = WriteFile(lFile, InData + lOffset, lChunk, &lDone, nullptr) && lDone == lChunk;
                lOffset += lDone;
            }

            bWritten = bWritten && FlushFileBuffers(lFile);
            return CloseHandle(lFile) && bWritten;
        }

        /** Deletes the temporary file at InPath, if a file is there. */
        void DeleteTemporary(const fs::path& InPath)
        {
            DeleteFileW(InPath.c_str());
        }

        /**
         * Renames InFrom over InTo in one step, on the disk when it returns. Retries for a moment: a
         * virus scanner or a search indexer reading InTo blocks the rename while it does.
         */
        bool ReplaceFlushed(const fs::path& InFrom, const fs::path& InTo)
        {
            constexpr Int32 ATTEMPTS = 5;

            for (Int32 lAttempt = 0; lAttempt < ATTEMPTS; ++lAttempt)
            {
                if (MoveFileExW(InFrom.c_str(), InTo.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                {
                    return true;
                }

                const DWORD lError = GetLastError();
                if ((lError != ERROR_ACCESS_DENIED && lError != ERROR_SHARING_VIOLATION) || lAttempt + 1 == ATTEMPTS)
                {
                    return false;
                }

                Sleep(10u << lAttempt);
            }
            return false;
        }
#else
        /** Writes InData to a new file at InPath, flushed to the disk and not only to the OS's cache. */
        bool WriteFlushed(const fs::path& InPath, const Uint8* InData, const Uint64 InSize)
        {
            const int lFile = ::open(InPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
            if (lFile < 0)
            {
                return false;
            }

            bool   bWritten = true;
            Uint64 lOffset  = 0;
            while (bWritten && lOffset < InSize)
            {
                const ssize_t lDone = ::write(lFile, InData + lOffset, static_cast<size_t>(InSize - lOffset));
                if (lDone < 0 && errno == EINTR)
                {
                    continue;
                }

                bWritten = lDone > 0;
                if (bWritten)
                {
                    lOffset += static_cast<Uint64>(lDone);
                }
            }

            bWritten = bWritten && ::fsync(lFile) == 0;
            return ::close(lFile) == 0 && bWritten;
        }

        /** Deletes the temporary file at InPath, if a file is there. */
        void DeleteTemporary(const fs::path& InPath)
        {
            ::unlink(InPath.c_str());
        }

        /** Renames InFrom over InTo in one step, on the disk when it returns. */
        bool ReplaceFlushed(const fs::path& InFrom, const fs::path& InTo)
        {
            if (std::rename(InFrom.c_str(), InTo.c_str()) != 0)
            {
                return false;
            }

            // The rename lives in the directory: flushed too, or a power cut could undo it. Some file
            // systems cannot flush a directory; the rename is done either way.
            const int lDirectory = ::open(InTo.parent_path().c_str(), O_RDONLY | O_CLOEXEC);
            if (lDirectory >= 0)
            {
                ::fsync(lDirectory);
                ::close(lDirectory);
            }
            return true;
        }
#endif
    }

    bool WriteAllTextAtomic(const OpaaxString& InAbsPath, const OpaaxString& InText)
    {
        return WriteAllBytesAtomic(InAbsPath, reinterpret_cast<const Uint8*>(InText.CStr()), InText.GetLength());
    }

    bool WriteAllBytesAtomic(const OpaaxString& InAbsPath, const Uint8* InData, const Uint64 InSize)
    {
        if (InAbsPath.IsEmpty() || (InData == nullptr && InSize > 0))
        {
            return false;
        }

        const fs::path lPath = Utf8::ToFsPath(InAbsPath);
        if (!EnsureParentDirectory(lPath))
        {
            return false;
        }

        // The old file is never opened: until the rename, it is exactly as it was.
        const fs::path lTemp = Utf8::ToFsPath(AtomicTempPath(InAbsPath));
        if (!WriteFlushed(lTemp, InData, InSize) || !ReplaceFlushed(lTemp, lPath))
        {
            DeleteTemporary(lTemp);
            return false;
        }
        return true;
    }

    OpaaxString AtomicTempPath(const OpaaxString& InAbsPath)
    {
        return InAbsPath + ".tmp";
    }
}
