#include "UI/Styles/UIButtonStyle.h"

namespace Opaax
{
    void UIButtonStyle::PostEditChange()
    {
        Brushes.Sanitize();
    }

    const UIButtonStyle* ResolveButtonStyle(IUIAssetProvider* const InAssets, const TAssetRef<UIButtonStyle>& InStyle)
    {
        if (InStyle.IsEmpty() || InAssets == nullptr)
        {
            return nullptr;
        }
        return Cast<UIButtonStyle>(InAssets->ResolveAsset(InStyle.Path.CStr()));
    }
}
