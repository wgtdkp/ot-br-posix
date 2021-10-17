/*
 *    Copyright (c) 2018, The OpenThread Authors.
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
 *   This file includes definition for mDNS publisher.
 */

#ifndef OTBR_AGENT_MDNS_MDNSSD_HPP_
#define OTBR_AGENT_MDNS_MDNSSD_HPP_

#include <array>
#include <map>
#include <utility>
#include <vector>

#include <assert.h>
#include <dns_sd.h>

#include "common/code_utils.hpp"
#include "common/mainloop.hpp"
#include "common/types.hpp"
#include "mdns/mdns.hpp"

namespace otbr {

namespace Mdns {

/** This class implements mDNS publisher with mDNSResponder. */
class PublisherMDnsSd : public MainloopProcessor, public Publisher
{
public:
    PublisherMDnsSd(StateChangedCallback aStateChangedCallback);
    ~PublisherMDnsSd(void) override;

    otbrError Start(void) override;
    bool      IsStarted(void) const override;
    void      Stop(void) override;
    void      PublishService(const std::string &    aHostName,
                             uint16_t               aPort,
                             const std::string &    aName,
                             const std::string &    aType,
                             const SubTypeList &    aSubTypeList,
                             const TxtList &        aTxtList,
                             PublishServiceCallback aCallback) override;
    void      UnpublishService(const std::string &aName, const std::string &aType, ResultCallback aCallback) override;
    void      PublishHost(const std::string &         aName,
                          const std::vector<uint8_t> &aAddress,
                          PublishHostCallback         aCallback) override;
    void      UnpublishHost(const std::string &aName, ResultCallback aCallback) override;
    void      SubscribeService(const std::string &aType,
                               const std::string &aInstanceName,
                               ResultCallback     aCallback) override;
    void      UnsubscribeService(const std::string &aType,
                                 const std::string &aInstanceName,
                                 ResultCallback     aCallback) override;
    void      SubscribeHost(const std::string &aHostName, ResultCallback aCallback) override;
    void      UnsubscribeHost(const std::string &aHostName, ResultCallback aCallback) override;

    void Update(MainloopContext &aMainloop) override;
    void Process(const MainloopContext &aMainloop) override;

private:
    /*
    enum
    {
        //kMaxSizeOfTxtRecord   = 256,
        //kMaxSizeOfServiceName = kDNSServiceMaxServiceName,
        //kMaxSizeOfHost        = 128,
        //kMaxSizeOfDomain      = kDNSServiceMaxDomainName,
        //kMaxSizeOfServiceType = 69,
    };
    */

    struct Service
    {
        std::string   mName;
        std::string   mType;
        std::string   mRegType; ///< Service type with optional subtypes separated by commas.
        DNSServiceRef mService;
        uint16_t      mPort = 0;
    };

    struct Host
    {
        std::string          mName;
        std::vector<uint8_t> mAddress;
        DNSRecordRef         mRecord;
    };

    struct PublishServiceCallbackContext
    {
        PublisherMDnsSd &      mPublisher;
        PublishServiceCallback mCallback;
    };

    struct PublishHostCallbackContext
    {
        PublisherMDnsSd &   mPublisher;
        PublishHostCallback mCallback;
    };

    struct Subscription
    {
        PublisherMDnsSd *mMDnsSd;
        DNSServiceRef    mServiceRef;

        explicit Subscription(PublisherMDnsSd &aMDnsSd)
            : mMDnsSd(&aMDnsSd)
            , mServiceRef(nullptr)
        {
        }

        void Release(void);
        void DeallocateServiceRef(void);
    };

    struct ServiceSubscription : public Subscription
    {
        explicit ServiceSubscription(PublisherMDnsSd &aMDnsSd, std::string aType, std::string aInstanceName)
            : Subscription(aMDnsSd)
            , mType(std::move(aType))
            , mInstanceName(std::move(aInstanceName))
        {
        }

        void Browse(void);
        void Resolve(uint32_t aInterfaceIndex, const char *aInstanceName, const char *aType, const char *aDomain);
        void GetAddrInfo(uint32_t aInterfaceIndex);

        static void HandleBrowseResult(DNSServiceRef       aServiceRef,
                                       DNSServiceFlags     aFlags,
                                       uint32_t            aInterfaceIndex,
                                       DNSServiceErrorType aErrorCode,
                                       const char *        aInstanceName,
                                       const char *        aType,
                                       const char *        aDomain,
                                       void *              aContext);
        void        HandleBrowseResult(DNSServiceRef       aServiceRef,
                                       DNSServiceFlags     aFlags,
                                       uint32_t            aInterfaceIndex,
                                       DNSServiceErrorType aErrorCode,
                                       const char *        aInstanceName,
                                       const char *        aType,
                                       const char *        aDomain);
        static void HandleResolveResult(DNSServiceRef        aServiceRef,
                                        DNSServiceFlags      aFlags,
                                        uint32_t             aInterfaceIndex,
                                        DNSServiceErrorType  aErrorCode,
                                        const char *         aFullName,
                                        const char *         aHostTarget,
                                        uint16_t             aPort, // In network byte order.
                                        uint16_t             aTxtLen,
                                        const unsigned char *aTxtRecord,
                                        void *               aContext);
        void        HandleResolveResult(DNSServiceRef        aServiceRef,
                                        DNSServiceFlags      aFlags,
                                        uint32_t             aInterfaceIndex,
                                        DNSServiceErrorType  aErrorCode,
                                        const char *         aFullName,
                                        const char *         aHostTarget,
                                        uint16_t             aPort, // In network byte order.
                                        uint16_t             aTxtLen,
                                        const unsigned char *aTxtRecord);
        static void HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                            DNSServiceFlags        aFlags,
                                            uint32_t               aInterfaceIndex,
                                            DNSServiceErrorType    aErrorCode,
                                            const char *           aHostName,
                                            const struct sockaddr *aAddress,
                                            uint32_t               aTtl,
                                            void *                 aContext);
        void        HandleGetAddrInfoResult(DNSServiceRef          aServiceRef,
                                            DNSServiceFlags        aFlags,
                                            uint32_t               aInterfaceIndex,
                                            DNSServiceErrorType    aErrorCode,
                                            const char *           aHostName,
                                            const struct sockaddr *aAddress,
                                            uint32_t               aTtl);

        std::string            mType;
        std::string            mInstanceName;
        DiscoveredInstanceInfo mInstanceInfo;
    };

    struct HostSubscription : public Subscription
    {
        explicit HostSubscription(PublisherMDnsSd &aMDnsSd, std::string aHostName)
            : Subscription(aMDnsSd)
            , mHostName(std::move(aHostName))
        {
        }

        void        Resolve(void);
        static void HandleResolveResult(DNSServiceRef          aServiceRef,
                                        DNSServiceFlags        aFlags,
                                        uint32_t               aInterfaceIndex,
                                        DNSServiceErrorType    aErrorCode,
                                        const char *           aHostName,
                                        const struct sockaddr *aAddress,
                                        uint32_t               aTtl,
                                        void *                 aContext);
        void        HandleResolveResult(DNSServiceRef          aServiceRef,
                                        DNSServiceFlags        aFlags,
                                        uint32_t               aInterfaceIndex,
                                        DNSServiceErrorType    aErrorCode,
                                        const char *           aHostName,
                                        const struct sockaddr *aAddress,
                                        uint32_t               aTtl);

        std::string        mHostName;
        DiscoveredHostInfo mHostInfo;
    };

    using ServiceList             = std::vector<Service>;
    using HostList                = std::vector<Host>;
    using ServiceIterator         = ServiceList::iterator;
    using HostIterator            = HostList::iterator;
    using ServiceSubscriptionList = std::vector<ServiceSubscription>;
    using HostSubscriptionList    = std::vector<HostSubscription>;

    void        DiscardService(const std::string &aName, const std::string &aType, DNSServiceRef aServiceRef = nullptr);
    void        RecordService(const std::string &aName,
                              const std::string &aType,
                              const std::string &aRegType,
                              uint16_t           aPort,
                              DNSServiceRef      aServiceRef);
    static bool IsServiceOutdated(const Service &service, const std::string &aNewRegType, int aNewPort);

    otbrError DiscardHost(const std::string &aName, bool aSendGoodbye = true);
    void      RecordHost(const std::string &aName, const std::vector<uint8_t> aAddress, DNSRecordRef aRecordRef);

    static void HandleServiceRegisterResult(DNSServiceRef         aService,
                                            const DNSServiceFlags aFlags,
                                            DNSServiceErrorType   aError,
                                            const char *          aName,
                                            const char *          aType,
                                            const char *          aDomain,
                                            void *                aContext);
    void        HandleServiceRegisterResult(DNSServiceRef         aService,
                                            const DNSServiceFlags aFlags,
                                            DNSServiceErrorType   aError,
                                            const char *          aName,
                                            const char *          aType,
                                            const char *          aDomain,
                                            PublishServiceCallback aCallback);
    static void HandleRegisterHostResult(DNSServiceRef       aHostsConnection,
                                         DNSRecordRef        aHostRecord,
                                         DNSServiceFlags     aFlags,
                                         DNSServiceErrorType aErrorCode,
                                         void *              aContext);
    void        HandleRegisterHostResult(DNSServiceRef       aHostsConnection,
                                         DNSRecordRef        aHostRecord,
                                         DNSServiceFlags     aFlags,
                                         DNSServiceErrorType aErrorCode);

    std::string        MakeFullName(const std::string &aName);
    static std::string MakeRegType(const std::string &aType, const SubTypeList &aSubTypeList);

    ServiceIterator FindPublishedService(const std::string &aName, const std::string &aType);
    ServiceIterator FindPublishedService(const DNSServiceRef &aServiceRef);
    HostIterator    FindPublishedHost(const DNSRecordRef &aRecordRef);
    HostIterator    FindPublishedHost(const std::string &aHostName);

    void        OnServiceResolved(ServiceSubscription &aService);
    static void OnServiceResolveFailed(const ServiceSubscription &aService, DNSServiceErrorType aErrorCode);
    void        OnHostResolved(HostSubscription &aHost);
    void        OnHostResolveFailed(const HostSubscription &aHost, DNSServiceErrorType aErrorCode);

    StateChangedCallback mStateChangedCallback;

    ServiceList   mServices;
    HostList      mHosts;
    DNSServiceRef mHostsRef;
    bool          mIsEnabled = false;

    ServiceSubscriptionList mSubscribedServices;
    HostSubscriptionList    mSubscribedHosts;
};

/**
 * @}
 */

} // namespace Mdns

} // namespace otbr

#endif // OTBR_AGENT_MDNS_MDNSSD_HPP_
