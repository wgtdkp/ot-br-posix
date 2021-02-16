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
 * This file implements the Task Runner that executes tasks on the mainloop.
 */

#include "common/task_runner.hpp"

#include <algorithm>

#include <fcntl.h>
#include <unistd.h>

#include "common/code_utils.hpp"

namespace otbr {

TaskRunner::TaskRunner(void)
{
    int flags;

    // There are no chances that we can handle failure of
    // creating a pipe, simply die.
    VerifyOrDie(pipe(mEventFd) != -1, strerror(errno));

    flags = fcntl(mEventFd[kRead], F_GETFL, 0);
    VerifyOrDie(fcntl(mEventFd[kRead], F_SETFL, flags | O_NONBLOCK) != -1, strerror(errno));
    flags = fcntl(mEventFd[kWrite], F_GETFL, 0);
    VerifyOrDie(fcntl(mEventFd[kWrite], F_SETFL, flags | O_NONBLOCK) != -1, strerror(errno));
}

TaskRunner::~TaskRunner(void)
{
    if (mEventFd[kRead] != -1)
    {
        close(mEventFd[kRead]);
        mEventFd[kRead] = -1;
    }
    if (mEventFd[kWrite] != -1)
    {
        close(mEventFd[kWrite]);
        mEventFd[kWrite] = -1;
    }
}

void TaskRunner::Post(const Task &aTask)
{
    PushTask(aTask);
}

void TaskRunner::UpdateFdSet(otSysMainloopContext &aMainloop)
{
    FD_SET(mEventFd[kRead], &aMainloop.mReadFdSet);
    aMainloop.mMaxFd = std::max(mEventFd[kRead], aMainloop.mMaxFd);
}

void TaskRunner::Process(const otSysMainloopContext &aMainloop)
{
    if (FD_ISSET(mEventFd[kRead], &aMainloop.mReadFdSet))
    {
        uint8_t n;

        // Read any data in the pipe.
        while (read(mEventFd[kRead], &n, sizeof(n)) == sizeof(n))
        {
        }

        PopTasks();
    }
}

void TaskRunner::PushTask(const Task &aTask)
{
    int                         rval;
    const uint8_t               kOne = 1;
    std::lock_guard<std::mutex> _(mTaskQueueMutex);

    mTaskQueue.emplace(aTask);
    rval = write(mEventFd[kWrite], &kOne, sizeof(kOne));

    if (rval != sizeof(kOne))
    {
        if (rval == -1)
        {
            otbrLog(OTBR_LOG_ERR, "failed to write event fd %d: %s", mEventFd, strerror(errno));
        }
        else
        {
            otbrLog(OTBR_LOG_ERR, "partially write event fd %d: %s", mEventFd, strerror(errno));
        }
    }
}

void TaskRunner::PopTasks(void)
{
    std::lock_guard<std::mutex> _(mTaskQueueMutex);

    while (!mTaskQueue.empty())
    {
        mTaskQueue.front()();
        mTaskQueue.pop();
    }
}

} // namespace otbr
