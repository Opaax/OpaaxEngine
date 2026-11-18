#include "UI/Sounds/UISoundSet.h"

#include "Core/Maths/Maths.h"

namespace Opaax
{
    void UISoundSet::PostEditChange()
    {
        Sounds.Volume = Maths::Max(Sounds.Volume, 0.f);
    }
}
