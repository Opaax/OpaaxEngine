// TestFolderPaths.h — IPaths over one folder: every asset path is relative to it, and every other
//   directory the paths answer is that folder too.
#pragma once

#include "Application/Services/IPaths.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class TestFolderPaths final : public IPaths
    {
    public:
        explicit TestFolderPaths(const OpaaxString& InRoot) : m_Root(InRoot) {}

        OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel) const override
        {
            return PathString::Join(m_Root, InAssetRel);
        }

        OpaaxString AbsoluteToAsset(const OpaaxString& InAbsPath) const override
        {
            return PathString::RelativeTo(InAbsPath, m_Root);
        }

        void        LogPaths()      const override {}
        OpaaxString WorkspaceRoot() const override { return m_Root; }
        OpaaxString EngineRoot()    const override { return m_Root; }
        OpaaxString ProjectRoot()   const override { return m_Root; }
        OpaaxString ProjectFile()   const override { return m_Root; }
        OpaaxString AssetsDir()     const override { return m_Root; }
        OpaaxString ConfigsDir()    const override { return m_Root; }
        OpaaxString SourceDir()     const override { return m_Root; }
        OpaaxString SaveDir()       const override { return m_Root; }
        OpaaxString TempDir()       const override { return m_Root; }

        OpaaxString EngineToAbsolute(const OpaaxString& InRel)  const override { return AssetToAbsolute(InRel); }
        OpaaxString ProjectToAbsolute(const OpaaxString& InRel) const override { return AssetToAbsolute(InRel); }

    private:
        OpaaxString m_Root;
    };
}
