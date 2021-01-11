/*
 *    Copyright (c) 2020, The OpenThread Authors.
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
 *   This file includes definition for Advertising Proxy.
 */

#ifndef OTBR_ADVERTISING_PROXY_HPP_
#define OTBR_ADVERTISING_PROXY_HPP_

#if OTBR_ENABLE_ADVERTISING_PROXY

#include <stdint.h>

#include <openthread/instance.h>
#include <openthread/srp_server.h>

#include "mdns/mdns.hpp"

namespace otbr {

/**
 * This class implements the Advertising Proxy.
 *
 */
class AdvertisingProxy
{
public:
    explicit AdvertisingProxy(Mdns::Publisher *aPublisher);

    otbrError Start(otInstance *aInstance);

    void Stop();

private:
    struct OutstandingUpdate
    {
        const otSrpServerHost *mHost;  // The host represents a collection of updates.
        uint32_t               mCount; // The number of outstanding updates.
    };

    static void AdvertisingHandler(const otSrpServerHost *aHost, uint32_t aTimeout, void *aContext);

    void AdvertisingHandler(const otSrpServerHost *aHost, uint32_t aTimeout);

    static void PublishServiceHandler(const char *aName, const char *aType, otbrError aError, void *aContext);
    void        PublishServiceHandler(const char *aName, const char *aType, otbrError aError);
    static void PublishHostHandler(const char *aName, otbrError aError, void *aContext);
    void        PublishHostHandler(const char *aName, otbrError aError);

    // A pointer to the OpenThread instance, has no ownership.
    otInstance *mInstance;

    // A pointer to the mDNS publisher, has no ownership.
    Mdns::Publisher *mPublisher;

    // A vector that tracks outstanding updates.
    std::vector<OutstandingUpdate> mOutstandingUpdates;
};

} // namespace otbr

#endif // OTBR_ENABLE_ADVERTISING_PROXY

#endif // OTBR_ADVERTISING_PROXY_HPP_
