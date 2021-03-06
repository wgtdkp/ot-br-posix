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
 *   This file includes definitions for Thread events listener.
 */

#ifndef OTBR_AGENT_THREAD_LISTENER_HPP_
#define OTBR_AGENT_THREAD_LISTENER_HPP_

#if OTBR_ENABLE_BACKBONE_ROUTER
#include <openthread/backbone_router_ftd.h>
#endif
#include <openthread/instance.h>

#include "common/code_utils.hpp"

namespace otbr {

/**
 * This class implements Thread listener.
 */
class ThreadListener
{
public:
    virtual ~ThreadListener(void) = default;

    virtual void OnThreadStateChanged(otInstance *aInstance, otChangedFlags aFlags)
    {
        OTBR_UNUSED_VARIABLE(aInstance);
        OTBR_UNUSED_VARIABLE(aFlags);
    }

#if OTBR_ENABLE_BACKBONE_ROUTER

    virtual void OnBackboneRouterDomainPrefixChanged(otInstance *aInstance,
                                                         otBackboneRouterDomainPrefixEvent aEvent,
                                                         const otIp6Prefix *aDomainPrefix)
    {
        OTBR_UNUSED_VARIABLE(aInstance);
        OTBR_UNUSED_VARIABLE(aEvent);
        OTBR_UNUSED_VARIABLE(aDomainPrefix);
    }

    virtual void OnBackboneRouterNdProxyChanged(otInstance *aInstance, otBackboneRouterNdProxyEvent aEvent,
                                                const otIp6Address *         aDua)
    {
        OTBR_UNUSED_VARIABLE(aInstance);
        OTBR_UNUSED_VARIABLE(aEvent);
        OTBR_UNUSED_VARIABLE(aDua);
    }

#endif // OTBR_ENABLE_BACKBONE_ROUTER
};

} // namespace otbr

#endif // OTBR_AGENT_THREAD_LISTENER_HPP_
