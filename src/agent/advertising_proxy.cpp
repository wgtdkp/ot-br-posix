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
 *   The file implements the Advertising Proxy.
 */

#include "agent/advertising_proxy.hpp"

#include <string>

#include <assert.h>

#include "common/logging.hpp"

namespace otbr {

static otError OtbrErrorToOtError(otbrError aError)
{
    otError error;

    switch (aError)
    {
    case OTBR_ERROR_NONE:
        error = OT_ERROR_NONE;
        break;

    case OTBR_ERROR_NOT_FOUND:
        error = OT_ERROR_NOT_FOUND;
        break;

    case OTBR_ERROR_PARSE:
        error = OT_ERROR_PARSE;
        break;

    case OTBR_ERROR_NOT_IMPLEMENTED:
        error = OT_ERROR_NOT_IMPLEMENTED;
        break;

    case OTBR_ERROR_INVALID_ARGS:
        error = OT_ERROR_INVALID_ARGS;
        break;

    case OTBR_ERROR_DUPLICATED:
        error = OT_ERROR_DUPLICATED;
        break;

    default:
        error = OT_ERROR_FAILED;
        break;
    }

    return error;
}

static otbrError SplitFullServiceName(const std::string &aFullName,
                                      std::string &      aInstanceName,
                                      std::string &      aType,
                                      std::string &      aDomain)
{
    otbrError error = OTBR_ERROR_INVALID_ARGS;
    size_t    dotPos[3];

    dotPos[0] = aFullName.find_first_of('.');
    VerifyOrExit(dotPos[0] != std::string::npos);

    dotPos[1] = aFullName.find_first_of('.', dotPos[0] + 1);
    VerifyOrExit(dotPos[1] != std::string::npos);

    dotPos[2] = aFullName.find_first_of('.', dotPos[1] + 1);
    VerifyOrExit(dotPos[2] != std::string::npos);

    error         = OTBR_ERROR_NONE;
    aInstanceName = aFullName.substr(0, dotPos[0]);
    aType         = aFullName.substr(dotPos[0] + 1, dotPos[2] - dotPos[0] - 1);
    aDomain       = aFullName.substr(dotPos[2] + 1, aFullName.size() - dotPos[2] - 1);

exit:
    return error;
}

static otbrError SplitFullHostName(const std::string &aFullName, std::string &aInstanceName, std::string &aDomain)
{
    otbrError error = OTBR_ERROR_INVALID_ARGS;
    size_t    dotPos;

    dotPos = aFullName.find_first_of('.');
    VerifyOrExit(dotPos != std::string::npos);

    error         = OTBR_ERROR_NONE;
    aInstanceName = aFullName.substr(0, dotPos);
    aDomain       = aFullName.substr(dotPos + 1, aFullName.size() - dotPos - 1);

exit:
    return error;
}

AdvertisingProxy::AdvertisingProxy(Mdns::Publisher *aPublisher)
    : mInstance(nullptr)
    , mPublisher(aPublisher)
{
}

otbrError AdvertisingProxy::Start(otInstance *aInstance)
{
    mInstance = aInstance;

    otSrpServerSetAdvertisingHandler(mInstance, AdvertisingHandler, this);

    mPublisher->SetPublishServiceHandler(PublishServiceHandler, this);
    mPublisher->SetPublishHostHandler(PublishHostHandler, this);

    return OTBR_ERROR_NONE;
}

void AdvertisingProxy::Stop()
{
    mPublisher->SetPublishServiceHandler(nullptr, nullptr);
    mPublisher->SetPublishHostHandler(nullptr, nullptr);

    // Stop receiving SRP server events.
    if (mInstance != nullptr)
    {
        otSrpServerSetAdvertisingHandler(mInstance, nullptr, nullptr);
    }
}

void AdvertisingProxy::AdvertisingHandler(const otSrpServerHost *aHost, uint32_t aTimeout, void *aContext)
{
    static_cast<AdvertisingProxy *>(aContext)->AdvertisingHandler(aHost, aTimeout);
}

void AdvertisingProxy::AdvertisingHandler(const otSrpServerHost *aHost, uint32_t aTimeout)
{
    (void)aTimeout;

    otbrError           error = OTBR_ERROR_NONE;
    OutstandingUpdate   update{aHost, 0};
    std::string         hostName;
    std::string         hostDomain;
    const otIp6Address *hostAddress;
    uint8_t             hostAddressNum;
    bool                publishHost;

    otbrLog(OTBR_LOG_INFO, "advertising SRP service updates %p", aHost);

    hostAddress = otSrpServerHostGetAddresses(aHost, &hostAddressNum);
    publishHost = hostAddressNum > 0;

    update.mCount += publishHost;
    for (auto service = otSrpServerHostGetServices(aHost); service != nullptr; service = service->mNext)
    {
        update.mCount += !service->mIsDeleted;
    }
    mOutstandingUpdates.emplace_back(update);

    SuccessOrExit(error = SplitFullHostName(otSrpServerHostGetFullName(aHost), hostName, hostDomain));

    if (publishHost)
    {
        // TODO: select a preferred address.
        SuccessOrExit(error =
                          mPublisher->PublishHost(hostName.c_str(), hostAddress[0].mFields.m8, sizeof(hostAddress[0])));
    }
    else
    {
        SuccessOrExit(error = mPublisher->UnpublishHost(hostName.c_str()));
    }

    for (const otSrpServerService *service = otSrpServerHostGetServices(aHost); service != nullptr;
         service                           = service->mNext)
    {
        std::string serviceName;
        std::string serviceType;
        std::string serviceDomain;

        SuccessOrExit(error = SplitFullServiceName(service->mFullName, serviceName, serviceType, serviceDomain));

        if (publishHost && !service->mIsDeleted)
        {
            Mdns::Publisher::TxtList txtList;

            // TODO: TXT records.

            SuccessOrExit(error = mPublisher->PublishService(hostName.c_str(), service->mPort, serviceName.c_str(),
                                                             serviceType.c_str(), txtList));
        }
        else
        {
            SuccessOrExit(error = mPublisher->UnpublishService(serviceName.c_str(), serviceType.c_str()));
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_INFO, "failed to advertise SRP service updates %p", aHost);

        mOutstandingUpdates.pop_back();
        otSrpServerHandleAdvertisingResult(mInstance, aHost, OtbrErrorToOtError(error));
    }

    return;
}

void AdvertisingProxy::PublishServiceHandler(const char *aName, const char *aType, otbrError aError, void *aContext)
{
    static_cast<AdvertisingProxy *>(aContext)->PublishServiceHandler(aName, aType, aError);
}

void AdvertisingProxy::PublishServiceHandler(const char *aName, const char *aType, otbrError aError)
{
    otbrError   error    = OTBR_ERROR_NONE;
    std::string normType = aType;

    if (normType.back() == '.')
    {
        normType.pop_back();
    }

    for (auto update = mOutstandingUpdates.begin(); update != mOutstandingUpdates.end(); ++update)
    {
        for (auto service = otSrpServerHostGetServices(update->mHost); service != nullptr; service = service->mNext)
        {
            std::string instanceName;
            std::string type;
            std::string domain;

            SuccessOrExit(error = SplitFullServiceName(service->mFullName, instanceName, type, domain));
            if (aName == instanceName && normType == type)
            {
                if (aError != OTBR_ERROR_NONE || update->mCount == 1)
                {
                    otSrpServerHandleAdvertisingResult(mInstance, update->mHost, OtbrErrorToOtError(aError));
                    mOutstandingUpdates.erase(update);
                }
                else
                {
                    --update->mCount;
                }
                ExitNow();
            }
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "failed to handle publication result of service %s", aName);
    }
}

void AdvertisingProxy::PublishHostHandler(const char *aName, otbrError aError, void *aContext)
{
    static_cast<AdvertisingProxy *>(aContext)->PublishHostHandler(aName, aError);
}

void AdvertisingProxy::PublishHostHandler(const char *aName, otbrError aError)
{
    otbrError error = OTBR_ERROR_NONE;

    for (auto update = mOutstandingUpdates.begin(); update != mOutstandingUpdates.end(); ++update)
    {
        std::string instanceName;
        std::string domain;

        SuccessOrExit(error = SplitFullHostName(otSrpServerHostGetFullName(update->mHost), instanceName, domain));
        if (aName == instanceName)
        {
            if (aError != OTBR_ERROR_NONE || update->mCount == 1)
            {
                // TODO: convert otbrError to otError.
                otSrpServerHandleAdvertisingResult(mInstance, update->mHost, static_cast<otError>(aError));
                mOutstandingUpdates.erase(update);
            }
            else
            {
                --update->mCount;
            }
            ExitNow();
        }
    }

exit:
    if (error != OTBR_ERROR_NONE)
    {
        otbrLog(OTBR_LOG_WARNING, "failed to handle publication result of host %s", aName);
    }
}

} // namespace otbr
