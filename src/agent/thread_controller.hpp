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
 *   This file includes definitions for Thread controller.
 */

#ifndef OTBR_AGENT_THREAD_CONTROLLER_HPP_
#define OTBR_AGENT_THREAD_CONTROLLER_HPP_

#include <chrono>
#include <functional>
#include <string>
#include <vector>

#include "agent/thread_helper.hpp"
#include "agent/thread_listener.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"

namespace otbr {

/**
 * This class implements Thread controller.
 *
 * A Thread controller exclusively owns the Thread instance (otInstance)
 * and drives the Thread stack.
 *
 */
class ThreadController : public MainloopProcessor
{
public:
    otbrError Init(const std::string &aThreadIfName, const std::string &aRadioUrl, const std::string &aBackboneIfName);

    void Deinit(void);

    void AddThreadListener(ThreadListener *aListener);

    /**
     * This method posts a task to the timer
     *
     * @param[in]   aTimePoint  The timepoint to trigger the task.
     * @param[in]   aTask       The task function.
     *
     */
    void PostTimerTask(std::chrono::steady_clock::time_point aTimePoint, const std::function<void(void)> &aTask);

    /**
     * This method registers a reset handler.
     *
     * @param[in]   aHandler  The handler function.
     *
     */
    void RegisterResetHandler(std::function<void(void)> aHandler);

    /**
     * This method updates the mainloop context.
     *
     * @param[inout]  aMainloop  A reference to the mainloop to be updated.
     *
     */
    void Update(MainloopContext &aMainloop) override;

    /**
     * This method processes mainloop events.
     *
     * @param[in]  aMainloop  A reference to the mainloop context.
     *
     */
    void Process(const MainloopContext &aMainloop) override;

private:
    static void HandleStateChanged(otChangedFlags aFlags, void *aThreadController);
    void HandleStateChanged(otChangedFlags aFlags);
#if OTBR_ENABLE_BACKBONE_ROUTER
    static void HandleBackboneRouterDomainPrefixEvent(void *                            aThreadController,
                                                      otBackboneRouterDomainPrefixEvent aEvent,
                                                      const otIp6Prefix *               aDomainPrefix);
    void        HandleBackboneRouterDomainPrefixEvent(otBackboneRouterDomainPrefixEvent aEvent,
                                                      const otIp6Prefix *               aDomainPrefix);
#if OTBR_ENABLE_DUA_ROUTING
    static void HandleBackboneRouterNdProxyEvent(void *                       aThreadController,
                                                 otBackboneRouterNdProxyEvent aEvent,
                                                 const otIp6Address *         aAddress);
    void        HandleBackboneRouterNdProxyEvent(otBackboneRouterNdProxyEvent aEvent, const otIp6Address *aAddress);
#endif
#endif

    otInstance *mInstance = nullptr;
    agent::ThreadHelper mThreadHelper;

    std::vector<ThreadListener *> mListeners;
    std::multimap<std::chrono::steady_clock::time_point, std::function<void(void)>> mTimers;
    std::vector<std::function<void(void)>>                                          mResetHandlers;
};

} // namespace otbr

#endif // OTBR_AGENT_THREAD_CONTROLLER_HPP_
