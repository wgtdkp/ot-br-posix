/*
 *    Copyright (c) 2021, The OpenThread Authors.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *    3. Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *    POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * This file define the Task Runner that executes tasks on the mainloop.
 */

#ifndef OTBR_COMMON_TASK_RUNNER_HPP_
#define OTBR_COMMON_TASK_RUNNER_HPP_

#include <openthread-br/config.h>

#include <functional>
#include <mutex>
#include <queue>

#include "common/mainloop.h"

namespace otbr {

/**
 * This class implements the Task Runner that executes
 * tasks on the mainloop.
 *
 */
class TaskRunner
{
public:
    /**
     * This type represents the generic executable task.
     *
     */
    using Task = std::function<void(void)>;

    /**
     * This constructor initializes the Task Runner instance.
     *
     */
    TaskRunner(void);

    /**
     * This destructor destroys the Task Runner instance.
     *
     */
    ~TaskRunner(void);

    /**
     * This method posts a task to the task runner.
     *
     * Tasks are executed sequentially and follow the First-Come-First-Serve rule.
     * It is safe to call this method in different threads concurrently.
     *
     * @param[in]  aTask  The task to be executed.
     *
     */
    void Post(const Task &aTask);

    /**
     * This method updates the file descriptor sets and timeout for mainloop.
     *
     * @param[inout]  aMainloop  A reference to OpenThread mainloop context.
     *
     */
    void UpdateFdSet(otSysMainloopContext &aMainloop);

    /**
     * This method performs processing.
     *
     * @param[in]  aMainloop  A reference to OpenThread mainloop context.
     *
     */
    void Process(const otSysMainloopContext &aMainloop);

private:
    enum
    {
        kRead  = 0,
        kWrite = 1,
    };

    void PushTask(const Task &aTask);
    void PopTasks(void);

    // The event fds which is used to wakeup the mainloop
    // when there are pending tasks in the task queue.
    int mEventFd[2];

    std::queue<Task> mTaskQueue;

    // The mutex which protects the `mTaskQueue` from being
    // simultaneously accessed by multiple threads.
    std::mutex mTaskQueueMutex;
};

} // namespace otbr

#endif // OTBR_COMMON_TASK_RUNNER_HPP_
