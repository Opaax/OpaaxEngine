#include "UI/Styles/UIButtonStyle.h"

namespace Opaax
{
    void UIButtonStyle::PostEditChange()
    {
        Brushes.Sanitize();
    }
}
