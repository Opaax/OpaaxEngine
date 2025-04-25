#pragma once
#include <deque>
#include <ranges>

#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    /**
     * @struct OpaaxDeletionQueue
     * OpaaxDeletionQueue is a utility class for managing and executing deferred cleanup tasks.
     *
     * This class maintains a queue of callable objects (`Deletors`) that can be executed
     * at a later point, typically during a cleanup or shutdown phase.
     */
    struct OPAAX_API OpaaxDeletionQueue
    {
        std::deque<OPSTDFunc<void()>> Deletors;

        /**
         * Pushes a callable object onto the deletion queue for deferred execution.
         *
         * This method allows the user to add cleanup tasks, represented as callable objects,
         * to the queue. These tasks will be executed during the cleanup or shutdown phase.
         *
         * @param Function A callable object (e.g., lambda or function) to be moved into the deletion queue.
         */
        void PushFunction(std::function<void()>&& Function);
        /**
         * Executes and clears all callable objects in the deletion queue.
         *
         * This method iterates through the deletion queue in reverse order, executing
         * each callable object, and then clears the queue. It is typically invoked
         * during a cleanup or shutdown phase to ensure all deferred tasks are completed.
         */
        void Flush();
    };

    inline void OpaaxDeletionQueue::PushFunction(std::function<void()>&& Function)
    {
        Deletors.push_back(Function);
    }

    inline void OpaaxDeletionQueue::Flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto& Deletor : std::ranges::reverse_view(Deletors))
        {
            Deletor(); //call functors
        }

        Deletors.clear();
    }
}
