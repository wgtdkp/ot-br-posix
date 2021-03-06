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
 *   This file implements Thread controller.
 *
 */

#include "agent/thread_controller.hpp"

#include <algorithm>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <openthread/backbone_router_ftd.h>
#include <openthread/cli.h>
#include <openthread/dataset.h>
#include <openthread/logging.h>
#include <openthread/srp_server.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/platform/logging.h>
#include <openthread/platform/misc.h>
#include <openthread/platform/radio.h>
#include <openthread/platform/settings.h>

#include "common/code_utils.hpp"
#include "common/logging.hpp"
#include "common/types.hpp"

#if OTBR_ENABLE_LEGACY
#include <ot-legacy-pairing-ext.h>
#endif

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;

namespace otbr {

otbrError ThreadController::Init(const std::string &aThreadIfName, const std::string &aRadioUrl, const std::string &aBackboneIfName)
{
    otbrError  error = OTBR_ERROR_NONE;
    otLogLevel level = OT_LOG_LEVEL_NONE;
    otPlatformConfig config;

    memset(&config, 0, sizeof(config));

    config.mInterfaceName         = aThreadIfName.c_str();
    config.mBackboneInterfaceName = aBackboneIfName.c_str();
    config.mRadioUrl              = aRadioUrl.c_str();
    config.mSpeedUpFactor         = 1;

    switch (otbrLogGetLevel())
    {
    case OTBR_LOG_EMERG:
    case OTBR_LOG_ALERT:
    case OTBR_LOG_CRIT:
        level = OT_LOG_LEVEL_CRIT;
        break;
    case OTBR_LOG_ERR:
    case OTBR_LOG_WARNING:
        level = OT_LOG_LEVEL_WARN;
        break;
    case OTBR_LOG_NOTICE:
        level = OT_LOG_LEVEL_NOTE;
        break;
    case OTBR_LOG_INFO:
        level = OT_LOG_LEVEL_INFO;
        break;
    case OTBR_LOG_DEBUG:
        level = OT_LOG_LEVEL_DEBG;
        break;
    default:
        ExitNow(error = OTBR_ERROR_OPENTHREAD);
        break;
    }
    VerifyOrExit(otLoggingSetLevel(level) == OT_ERROR_NONE, error = OTBR_ERROR_OPENTHREAD);

    mInstance = otSysInit(&config);
    otCliUartInit(mInstance);
#if OTBR_ENABLE_LEGACY
    otLegacyInit();
#endif

    {
        otError result = otSetStateChangedCallback(mInstance, &ThreadController::HandleStateChanged, this);

        agent::ThreadHelper::LogOpenThreadResult("Set state callback", result);
        VerifyOrExit(result == OT_ERROR_NONE, error = OTBR_ERROR_OPENTHREAD);
    }

#if OTBR_ENABLE_BACKBONE_ROUTER
    otBackboneRouterSetDomainPrefixCallback(mInstance, &ThreadController::HandleBackboneRouterDomainPrefixEvent,
                                            this);
#if OTBR_ENABLE_DUA_ROUTING
    otBackboneRouterSetNdProxyCallback(mInstance, &ThreadController::HandleBackboneRouterNdProxyEvent, this);
#endif
#endif

#if OTBR_ENABLE_SRP_ADVERTISING_PROXY
    otSrpServerSetEnabled(mInstance, /* aEnabled */ true);
#endif

    mThreadHelper.Init(mInstance, this);

exit:
    return error;
}

void ThreadController::Deinit(void)
{
    if (mInstance != nullptr)
    {
        otInstanceFinalize(mInstance);
    }
    otSysDeinit();
}

void ThreadController::AddThreadListener(ThreadListener *aListener)
{
    auto existingListener = std::find_if(mListeners.begin(), mListeners.end(), [=](ThreadListener *aExistingListener) {
        return aExistingListener == aListener;
    });

    OTBR_UNUSED_VARIABLE(existingListener);
    assert(existingListener == mListeners.end());

    mListeners.push_back(aListener);
}

void ThreadController::PostTimerTask(std::chrono::steady_clock::time_point aTimePoint, const std::function<void(void)> &aTask)
{
    mTimers.insert({aTimePoint, aTask});
}

void ThreadController::RegisterResetHandler(std::function<void(void)> aHandler)
{
    mResetHandlers.emplace_back(std::move(aHandler));
}

static struct timeval ToTimeVal(const microseconds &aTime)
{
    constexpr int  kUsPerSecond = 1000000;
    struct timeval val;

    val.tv_sec  = aTime.count() / kUsPerSecond;
    val.tv_usec = aTime.count() % kUsPerSecond;

    return val;
};

void ThreadController::Update(MainloopContext &aMainloop)
{
    microseconds timeout = microseconds(aMainloop.mTimeout.tv_usec) + seconds(aMainloop.mTimeout.tv_sec);
    auto         now     = steady_clock::now();

    if (otTaskletsArePending(mInstance))
    {
        timeout = microseconds::zero();
    }
    else if (!mTimers.empty())
    {
        if (mTimers.begin()->first < now)
        {
            timeout = microseconds::zero();
        }
        else
        {
            timeout = std::min(timeout, duration_cast<microseconds>(mTimers.begin()->first - now));
        }
    }

    aMainloop.mTimeout = ToTimeVal(timeout);

    otSysMainloopUpdate(mInstance, &aMainloop);
}

void ThreadController::Process(const MainloopContext &aMainloop)
{
    auto now = steady_clock::now();

    otTaskletsProcess(mInstance);

    otSysMainloopProcess(mInstance, &aMainloop);

    while (!mTimers.empty() && mTimers.begin()->first <= now)
    {
        mTimers.begin()->second();
        mTimers.erase(mTimers.begin());
    }

    if (getenv("OTBR_NO_AUTO_ATTACH") == nullptr && mThreadHelper.TryResumeNetwork() == OT_ERROR_NONE)
    {
        setenv("OTBR_NO_AUTO_ATTACH", "1", 0);
    }
}

void ThreadController::HandleStateChanged(otChangedFlags aFlags, void *aThreadController)
{
    static_cast<ThreadController *>(aThreadController)->HandleStateChanged(aFlags);
}

void ThreadController::HandleStateChanged(otChangedFlags aFlags)
{
    for (auto &listener : mListeners)
    {
        listener->OnThreadStateChanged(mInstance, aFlags);
    }
}

#if OTBR_ENABLE_BACKBONE_ROUTER
void ThreadController::HandleBackboneRouterDomainPrefixEvent(void *                            aThreadController,
                                                    otBackboneRouterDomainPrefixEvent aEvent,
                                                    const otIp6Prefix *               aDomainPrefix)
{
    static_cast<ThreadController *>(aThreadController)->HandleBackboneRouterDomainPrefixEvent(aEvent, aDomainPrefix);
}

void ThreadController::HandleBackboneRouterDomainPrefixEvent(otBackboneRouterDomainPrefixEvent aEvent,
                                                    const otIp6Prefix *               aDomainPrefix)
{
    for (auto &listener : mListeners)
    {
        listener->OnBackboneRouterDomainPrefixChanged(mInstance, aEvent, aDomainPrefix);
    }
}

#if OTBR_ENABLE_DUA_ROUTING
void ThreadController::HandleBackboneRouterNdProxyEvent(void *                       aThreadController,
                                                otBackboneRouterNdProxyEvent aEvent,
                                                const otIp6Address *         aAddress)
{
    static_cast<ThreadController *>(aThreadController)->HandleBackboneRouterNdProxyEvent(aEvent, aAddress);
}

void ThreadController::HandleBackboneRouterNdProxyEvent(otBackboneRouterNdProxyEvent aEvent, const otIp6Address *aAddress)
{
    for (auto &listener : mListeners)
    {
        listener->OnBackboneRouterNdProxyChanged(mInstance, aEvent, aAddress);
    }
}
#endif
#endif // OTBR_ENABLE_BACKBONE_ROUTER

/*
 * Provide, if required an "otPlatLog()" function
 */
extern "C" void otPlatLog(otLogLevel aLogLevel, otLogRegion aLogRegion, const char *aFormat, ...)
{
    OT_UNUSED_VARIABLE(aLogRegion);

    int otbrLogLevel;

    switch (aLogLevel)
    {
    case OT_LOG_LEVEL_NONE:
        otbrLogLevel = OTBR_LOG_EMERG;
        break;
    case OT_LOG_LEVEL_CRIT:
        otbrLogLevel = OTBR_LOG_CRIT;
        break;
    case OT_LOG_LEVEL_WARN:
        otbrLogLevel = OTBR_LOG_WARNING;
        break;
    case OT_LOG_LEVEL_NOTE:
        otbrLogLevel = OTBR_LOG_NOTICE;
        break;
    case OT_LOG_LEVEL_INFO:
        otbrLogLevel = OTBR_LOG_INFO;
        break;
    case OT_LOG_LEVEL_DEBG:
        otbrLogLevel = OTBR_LOG_DEBUG;
        break;
    default:
        otbrLogLevel = OTBR_LOG_DEBUG;
        break;
    }

    va_list ap;
    va_start(ap, aFormat);
    otbrLogv(otbrLogLevel, aFormat, ap);
    va_end(ap);
}

} // namespce otbr
