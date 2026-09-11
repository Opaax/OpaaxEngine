#include "Editor/Resources/ResourceScan.h"

#include "Platform/IFileSystem.h"
#include "Core/String/OpaaxPathString.h"                  // Extension — the ONE extension-of-a-path rule
#include "Engine/Subsystems/Resources/ResourceFormat.h"   // NormalizeExtension — the ONE comparability rule

#include <algorithm>
#include <cstring>

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
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

                    OutFolder.Folders.emplace_back(Move(lChild));
                }
                else
                {
                    OutFolder.Files.emplace_back(
                        lEntry.Name,
                        lChildRel,
                        lEntry.AbsPath,
                        NormalizeExtension(PathString::Extension(lEntry.Name)));

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
