#include "World/Serialization/HardReferences.h"

#include "World/Components/ComponentRegistry.h"
#include "World/Serialization/MapData.h"

namespace Opaax::HardReferences
{
    TDynArray<HardReference> Collect(const MapData& InData, const ComponentRegistry& InRegistry)
    {
        TDynArray<HardReference> lFound;

        const auto lAlreadyFound = [&lFound](const Uint32 InTypeId, const OpaaxString& InPath)
        {
            for (const HardReference& lRef : lFound)
            {
                if (lRef.TypeId == InTypeId && lRef.Path == InPath) { return true; }
            }
            return false;
        };

        for (const EntityData& lEntity : InData.Entities)
        {
            for (const ComponentData& lComponent : lEntity.Components)
            {
                const IComponentEntry* const lEntry = InRegistry.FindByName(lComponent.TypeName);
                if (lEntry == nullptr) { continue; }   // a type this build does not know — MP3's tolerance

                for (const HardRefField& lField : lEntry->GetHardRefFields())
                {
                    // The json bridge writes a TResourcePath as its bare string (ResourcePathJson).
                    const auto lIt = lComponent.Payload.find(lField.Name.CStr());
                    if (lIt == lComponent.Payload.end() || !lIt->is_string()) { continue; }

                    const OpaaxString lPath(lIt->get<std::string>().c_str());
                    if (lPath.IsEmpty() || lAlreadyFound(lField.TypeId, lPath)) { continue; }

                    lFound.emplace_back(lField.TypeId, lPath);
                }
            }
        }

        return lFound;
    }
}
