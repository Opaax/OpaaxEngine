#include "UI/UICanvasAsset.h"

#include "Core/IO/FileIO.h"
#include "Core/Log/Logger.h"
#include "Object/ObjectClassRegistry.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UICanvasAsset);

    UICanvasAsset::UICanvasAsset()  = default;
    UICanvasAsset::~UICanvasAsset() = default;

    bool UICanvasAsset::LoadSource(const char* InAbsPath)
    {
        m_Text = FileIO::ReadAllText(OpaaxString(InAbsPath));

        if (m_Text.IsEmpty())
        {
            OPAAX_LOG(LogUICanvasAsset, Error, "Canvas '{}' is missing, empty or unreadable", InAbsPath);
            return false;
        }

        return true;
    }

    Uint64 UICanvasAsset::GetResourceSize() const
    {
        return sizeof(UICanvasAsset) + m_Text.GetLength();
    }

    TStrongObjectPtr<UIWidget> UICanvasAsset::BuildTree(const ObjectClassRegistry& InClasses, float& OutReferenceHeight) const
    {
        UICanvasFile::UICanvasDoc lDoc;

        if (!UICanvasFile::Deserialize(m_Text, InClasses, lDoc))
        {
            return nullptr;   // UICanvasFile logged why
        }

        OutReferenceHeight = lDoc.ReferenceHeight;
        return Move(lDoc.Root);
    }
}
