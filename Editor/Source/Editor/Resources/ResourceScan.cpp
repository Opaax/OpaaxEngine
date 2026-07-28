#include "Editor/Resources/ResourceScan.h"

#include "Application/Services/Platforms/IFileSystem.h"

#include <algorithm>
#include <cstring>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        // Everything after the LAST dot. A leading dot means a dotfile (".gitkeep"), which has no
        // extension — the same rule std::filesystem::path::extension applies.
        OpaaxString ExtensionOf(const OpaaxString& InFileName)
        {
            const Uint32 lLength = InFileName.GetLength();

            for (Uint32 i = lLength; i > 1; --i)
            {
                if (InFileName[i - 1] == '.')
                {
                    return InFileName.SubString(i - 1, lLength - (i - 1));
                }
            }

            return {};
        }

        bool NameLess(const OpaaxString& InLeft, const OpaaxString& InRight)
        {
            return std::strcmp(InLeft.CStr(), InRight.CStr()) < 0;
        }

        void ScanFolder(const IFileSystem& InFileSystem, const OpaaxString& InAbsPath,
                        const OpaaxString& InRelPath, ResourceFolder& OutFolder, ResourceRoot& InOutRoot)
        {
            TDynArray<IFileSystem::Entry> lEntries;
            if (!InFileSystem.ListDirectory(InAbsPath, lEntries))
            {
                return;
            }

            for (const IFileSystem::Entry& lEntry : lEntries)
            {
                const OpaaxString lChildRel = InRelPath.IsEmpty()
                                            ? lEntry.Name
                                            : (InRelPath + "/" + lEntry.Name);

                if (lEntry.bIsDirectory)
                {
                    ResourceFolder lChild;
                    lChild.Name    = lEntry.Name;
                    lChild.RelPath = lChildRel;

                    ++InOutRoot.FolderCount;
                    ScanFolder(InFileSystem, lEntry.AbsPath, lChildRel, lChild, InOutRoot);

                    OutFolder.Folders.push_back(Move(lChild));
                }
                else
                {
                    OutFolder.Files.push_back(ResourceFile{
                        lEntry.Name,
                        lChildRel,
                        lEntry.AbsPath,
                        NormalizeExtension(ExtensionOf(lEntry.Name)) });

                    ++InOutRoot.FileCount;
                }
            }

            // Sorted here, once per scan, so every view renders the same order for free.
            std::sort(OutFolder.Folders.begin(), OutFolder.Folders.end(),
                [](const ResourceFolder& InLeft, const ResourceFolder& InRight)
                { return NameLess(InLeft.Name, InRight.Name); });

            std::sort(OutFolder.Files.begin(), OutFolder.Files.end(),
                [](const ResourceFile& InLeft, const ResourceFile& InRight)
                { return NameLess(InLeft.Name, InRight.Name); });
        }
    }

    OpaaxStringID NormalizeExtension(const OpaaxString& InExtension)
    {
        if (InExtension.IsEmpty())
        {
            return {};
        }

        const OpaaxString lLower = InExtension.ToLower();

        // A registrant may write "wave" or ".wave"; the scanner always produces the dotted form, so the
        // dot is added here rather than trusted from either side.
        return OpaaxStringID(lLower[0] == '.' ? lLower : (OpaaxString(".") + lLower));
    }

    bool ScanRoot(const IFileSystem& InFileSystem, ResourceRoot& InOutRoot)
    {
        InOutRoot.Tree         = ResourceFolder{};
        InOutRoot.Tree.Name    = InOutRoot.Label.ToString();
        InOutRoot.FileCount    = 0;
        InOutRoot.FolderCount  = 0;
        InOutRoot.bExists      = InFileSystem.IsPathExist(InOutRoot.AbsPath);

        if (!InOutRoot.bExists)
        {
            return false;
        }

        ScanFolder(InFileSystem, InOutRoot.AbsPath, OpaaxString(), InOutRoot.Tree, InOutRoot);
        return true;
    }
}
