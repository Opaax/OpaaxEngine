#include "Application/Services/ISaveStorage.h"

#include "Application/Services/IPaths.h"
#include "Core/Algo/Algo.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/Char.h"
#include "Core/String/OpaaxPathString.h"

namespace Opaax
{
    namespace
    {
        // =====================================================================
        // NullSaveStorage — keeps nothing.
        // =====================================================================
        class NullSaveStorage final : public ISaveStorage
        {
        public:
            bool                   IsNull()                                   const noexcept override { return true; }
            OpaaxString            GetSaveDirectory()                         const override { return OpaaxString(); }
            ESaveResult            Read(OpaaxStringView, OpaaxString&)        const override { return ESaveResult::Missing; }
            ESaveResult            Write(OpaaxStringView, const OpaaxString&)       override { return ESaveResult::IoError; }
            bool                   Exists(OpaaxStringView)                    const override { return false; }
            ESaveResult            Delete(OpaaxStringView)                          override { return ESaveResult::Missing; }
            TDynArray<OpaaxString> ListSlots()                                const override { return {}; }
        };

        /** True for a name Windows opens as a device, whatever the extension: CON, PRN, AUX, NUL, COM0-9, LPT0-9. */
        bool IsWindowsDeviceName(const OpaaxStringView InName)
        {
            const OpaaxString lLower = InName.ToString().ToLower();

            if (lLower == "con" || lLower == "prn" || lLower == "aux" || lLower == "nul")
            {
                return true;
            }

            return lLower.GetLength() == 4 && (lLower.StartsWith("com") || lLower.StartsWith("lpt"))
                   && Char::IsDigit(lLower.View()[3]);
        }
    }

    // =========================================================================
    // Type tag + null object (defined here so there is one of each).
    // =========================================================================
    ServiceTypeID ISaveStorage::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    ISaveStorage& ISaveStorage::Null()
    {
        static NullSaveStorage s_Null;
        return s_Null;
    }

    // =========================================================================
    // ISaveStorage
    // =========================================================================
    bool ISaveStorage::IsValidSlotName(const OpaaxStringView InSlot)
    {
        if (InSlot.IsEmpty() || InSlot.GetLength() > MAX_SLOT_NAME_LENGTH)
        {
            return false;
        }

        for (const char lChar : InSlot)
        {
            if (!Char::IsAlnum(lChar) && lChar != '_' && lChar != '-')
            {
                return false;
            }
        }

        return !IsWindowsDeviceName(InSlot);
    }

    OpaaxString ISaveStorage::GetSlotPath(const OpaaxStringView InSlot) const
    {
        const OpaaxString lDirectory = GetSaveDirectory();
        if (lDirectory.IsEmpty() || !IsValidSlotName(InSlot))
        {
            return OpaaxString();
        }

        return PathString::Join(lDirectory, InSlot.ToString() + SLOT_EXTENSION);
    }

    // =========================================================================
    // SaveStorage
    // =========================================================================
    SaveStorage::SaveStorage(const IPaths& InPaths)
        : m_SaveDirectory(InPaths.UserDir().IsEmpty() ? OpaaxString() : PathString::Join(InPaths.UserDir(), "SaveGames"))
    {
    }

    ESaveResult SaveStorage::Read(const OpaaxStringView InSlot, OpaaxString& OutText) const
    {
        const OpaaxString lPath = GetSlotPath(InSlot);
        if (lPath.IsEmpty())
        {
            return IsValidSlotName(InSlot) ? ESaveResult::Missing : ESaveResult::InvalidSlot;
        }

        if (!FileSystem::IsFile(lPath))
        {
            return ESaveResult::Missing;
        }

        // Bytes, not ReadAllText: it cannot tell a file it failed to read from an empty one.
        TDynArray<Uint8> lBytes;
        if (!FileIO::ReadAllBytes(lPath, lBytes))
        {
            OPAAX_LOG(LogSaveStorage, Error, "Cannot read save slot '{}' ('{}')", InSlot, lPath.CStr());
            return ESaveResult::IoError;
        }

        OutText = OpaaxString(reinterpret_cast<const char*>(lBytes.data()), static_cast<Uint32>(lBytes.size()));
        return ESaveResult::Ok;
    }

    ESaveResult SaveStorage::Write(const OpaaxStringView InSlot, const OpaaxString& InText)
    {
        const OpaaxString lPath = GetSlotPath(InSlot);
        if (lPath.IsEmpty())
        {
            return IsValidSlotName(InSlot) ? ESaveResult::IoError : ESaveResult::InvalidSlot;
        }

        if (!FileIO::WriteAllTextAtomic(lPath, InText))
        {
            OPAAX_LOG(LogSaveStorage, Error, "Cannot write save slot '{}' ('{}'): its last save is kept", InSlot,
                      lPath.CStr());
            return ESaveResult::IoError;
        }

        return ESaveResult::Ok;
    }

    bool SaveStorage::Exists(const OpaaxStringView InSlot) const
    {
        const OpaaxString lPath = GetSlotPath(InSlot);
        return !lPath.IsEmpty() && FileSystem::IsFile(lPath);
    }

    ESaveResult SaveStorage::Delete(const OpaaxStringView InSlot)
    {
        const OpaaxString lPath = GetSlotPath(InSlot);
        if (lPath.IsEmpty())
        {
            return IsValidSlotName(InSlot) ? ESaveResult::Missing : ESaveResult::InvalidSlot;
        }

        // What a crashed write left is no use without its slot.
        const OpaaxString lTempPath = FileIO::AtomicTempPath(lPath);
        if (FileSystem::IsFile(lTempPath))
        {
            FileSystem::Remove(lTempPath);
        }

        if (!FileSystem::IsFile(lPath))
        {
            return ESaveResult::Missing;
        }

        if (!FileSystem::Remove(lPath))
        {
            OPAAX_LOG(LogSaveStorage, Error, "Cannot delete save slot '{}' ('{}')", InSlot, lPath.CStr());
            return ESaveResult::IoError;
        }

        return ESaveResult::Ok;
    }

    TDynArray<OpaaxString> SaveStorage::ListSlots() const
    {
        TDynArray<OpaaxString> lSlots;

        // No folder yet: nothing was saved.
        TDynArray<FileSystem::Entry> lEntries;
        if (m_SaveDirectory.IsEmpty() || !FileSystem::ListDirectory(m_SaveDirectory, lEntries))
        {
            return lSlots;
        }

        const Uint32 lExtensionLength = OpaaxStringView(SLOT_EXTENSION).GetLength();
        for (const FileSystem::Entry& lEntry : lEntries)
        {
            if (lEntry.bIsDirectory || !lEntry.Name.EndsWith(SLOT_EXTENSION))
            {
                continue;
            }

            const OpaaxStringView lSlot = lEntry.Name.View().SubString(0, lEntry.Name.GetLength() - lExtensionLength);
            if (IsValidSlotName(lSlot))
            {
                lSlots.push_back(lSlot.ToString());
            }
        }

        Algo::Sort(lSlots);
        return lSlots;
    }
}
