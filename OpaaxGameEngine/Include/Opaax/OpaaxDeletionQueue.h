#pragma once
#include <deque>
#include <ranges>

#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    struct OPAAX_API OpaaxDeletionQueue
    {
        std::deque<OPSTDFunc<void()>> Deletors;

        void PushFunction(std::function<void()>&& Function);

        void Flush()
        {
            // reverse iterate the deletion queue to execute all the functions
            for (auto& Deletor : std::ranges::reverse_view(Deletors))
            {
                Deletor(); //call functors
            }

            Deletors.clear();
        }
    };

    inline void OpaaxDeletionQueue::PushFunction(std::function<void()>&& Function)
    {
        Deletors.push_back(Function);
    }
}
