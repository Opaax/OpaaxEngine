#include "IConfig.h"

#include "Core/String/OpaaxPathString.h"

namespace Opaax
{
	OpaaxStringID IConfig::GetName() const
	{
		return OpaaxStringID(PathString::Stem(FileName()).ToString());
	}
}
