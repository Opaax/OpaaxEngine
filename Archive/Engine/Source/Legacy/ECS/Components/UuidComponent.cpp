#include "UuidComponent.h"

#include <limits>
#include <random>
#include <string>

namespace Opaax::ECS
{
    Uint64 GenerateUuid()
    {
        // Thread-local generator — avoids contention, seeded once per thread.
        static thread_local std::mt19937_64 sEngine{ std::random_device{}() };
        static thread_local std::uniform_int_distribution<Uint64> sDist{ 1, std::numeric_limits<Uint64>::max() };
        return sDist(sEngine);
    }

    json UuidComponent::Serialize() const
    {
        // Stringify to dodge JSON integer precision (some parsers cap at 53 bits).
        return { { "uuid", std::to_string(Id) } };
    }

    void UuidComponent::DeserializeImplementation(const json& Json)
    {
        if (!Json.contains("uuid")) { Id = 0; return; }

        const auto& lEntry = Json["uuid"];
        if (lEntry.is_string())
        {
            try { Id = std::stoull(lEntry.get<std::string>()); }
            catch (...) { Id = 0; }
        }
        else if (lEntry.is_number_unsigned())
        {
            Id = lEntry.get<Uint64>();
        }
        else
        {
            Id = 0;
        }
    }
}
