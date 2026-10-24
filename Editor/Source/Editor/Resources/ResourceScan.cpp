#include "Editor/Resources/ResourceScan.h"

#include "Editor/Resources/AssetFileClass.hpp"
#include "Platform/IFileSystem.h"
#include "Core/String/OpaaxPathString.h"                  // Extension
#include "Resources/ResourceFormat.h"   // NormalizeExtension

#include "Core/Algo/Algo.h"
#include "Core/String/CString.h"

using namespace Opaax;

namespace Opaax::Editor
{
    namespace
    {
        bool NameLess(const OpaaxString& InLeft, const OpaaxString& InRight)
        {
            return CString::Compare(InLeft.CStr(), InRight.CStr()) < 0;
        }

        void ScanFolder(const IFileSystem& InFileSystem, const OpaaxString& InAbsPath,
                        const OpaaxString& InRelPath, ResourceFolder& OutFolder, ResourceRoot& InOutRoot,
                        const ObjectClassRegistry* InClasses)
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
                    ScanFolder(InFileSystem, lEntry.AbsPath, lChildRel, lChild, InOutRoot, InClasses);

                    OutFolder.Folders.emplace_back(Move(lChild));
                }
                else
                {
                    ResourceFile& lFile = OutFolder.Files.emplace_back(
                        lEntry.Name,
                        lChildRel,
                        lEntry.AbsPath,
                        NormalizeExtension(PathString::Extension(lEntry.Name)));

                    lFile.AssetClass = FindAssetClass(lFile.AbsPath, InClasses);

                    ++InOutRoot.FileCount;
                }
            }

            // Sorted once per scan, so every view shows the same order.
            Algo::Sort(OutFolder.Folders,
                [](const ResourceFolder& InLeft, const ResourceFolder& InRight)
                { return NameLess(InLeft.Name, InRight.Name); });

            Algo::Sort(OutFolder.Files,
                [](const ResourceFile& InLeft, const ResourceFile& InRight)
                { return NameLess(InLeft.Name, InRight.Name); });
        }
    }

    bool ScanRoot(const IFileSystem& InFileSystem, ResourceRoot& InOutRoot, const ObjectClassRegistry* InClasses)
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

        ScanFolder(InFileSystem, InOutRoot.AbsPath, OpaaxString(), InOutRoot.Tree, InOutRoot, InClasses);
        return true;
    }
}
