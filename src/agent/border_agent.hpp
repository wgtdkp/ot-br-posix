/*
 *    Copyright (c) 2017, The OpenThread Authors.
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
 *   This file includes definition for Thread border agent.
 */

#ifndef BORDER_AGENT_HPP_
#define BORDER_AGENT_HPP_

#include <stdint.h>

#include "mdns.hpp"
#include "ncp.hpp"

namespace ot {

namespace BorderRouter {

/**
 * @addtogroup border-router-border-agent
 *
 * @brief
 *   This module includes definition for Thread border agent
 *
 * @{
 */

/**
 * This class implements Thread border agent functionality.
 *
 */
class BorderAgent
{
public:
    /**
     * The constructor to initialize the Thread border agent.
     *
     * @param[in]   aNcp            A pointer to the NCP controller.
     *
     */
    BorderAgent(Ncp::Controller *aNcp);

    ~BorderAgent(void);

    /**
     * This method initialize border agent service.
     *
     */
    void Init(void);

    /**
     * This method updates the fd_set and timeout for mainloop.
     *
     * @param[inout]  aReadFdSet   A reference to read file descriptors.
     * @param[inout]  aWriteFdSet  A reference to write file descriptors.
     * @param[inout]  aErrorFdSet  A reference to error file descriptors.
     * @param[inout]  aMaxFd       A reference to the max file descriptor.
     * @param[inout]  aTimeout     A reference to timeout.
     *
     */
    void UpdateFdSet(fd_set &aReadFdSet, fd_set &aWriteFdSet, fd_set &aErrorFdSet, int &aMaxFd, timeval &aTimeout);

    /**
     * This method performs border agent processing.
     *
     * @param[in]   aReadFdSet   A reference to read file descriptors.
     * @param[in]   aWriteFdSet  A reference to write file descriptors.
     * @param[in]   aErrorFdSet  A reference to error file descriptors.
     *
     */
    void Process(const fd_set &aReadFdSet, const fd_set &aWriteFdSet, const fd_set &aErrorFdSet);

private:
    /**
     * This struct defines the State bitmap.
     *
     */
    struct State
    {
        uint32_t mConnectionMode : 3;
        uint32_t mThreadIfStatus : 2;
        uint32_t mAvailability : 2;
        uint32_t mBbrIsActive : 1;
        uint32_t mBbrIsPrimary : 1;
    };

    /**
     * This method starts border agent service.
     *
     * @retval  OTBR_ERROR_NONE     Successfully started border agent.
     * @retval  OTBR_ERROR_ERRNO    Failed to start border agent.
     *
     */
    otbrError Start(void);

    /**
     * This method stops border agent service.
     *
     */
    void Stop(void);

#if OTBR_ENABLE_NCP_WPANTUND
    static void SendToCommissioner(void *aContext, int aEvent, va_list aArguments);
#endif

    static void HandleMdnsState(void *aContext, Mdns::State aState)
    {
        static_cast<BorderAgent *>(aContext)->HandleMdnsState(aState);
    }
    void HandleMdnsState(Mdns::State aState);
    void PublishService(void);
    void StartPublishService(void);
    void StopPublishService(void);

    void SetNetworkName(const char *aNetworkName);
    void SetExtPanId(const uint8_t *aExtPanId);
    void HandleThreadState(bool aStarted);
    void HandlePSKc(const uint8_t *aPSKc);

    static void HandlePSKc(void *aContext, int aEvent, va_list aArguments);
    static void HandleThreadState(void *aContext, int aEvent, va_list aArguments);
    static void HandleNetworkName(void *aContext, int aEvent, va_list aArguments);
    static void HandleExtPanId(void *aContext, int aEvent, va_list aArguments);
    static void EncodeState(char *aBuf, size_t aMaxLength, const State &aState);

    Mdns::Publisher *mPublisher;
    Ncp::Controller *mNcp;

#if OTBR_ENABLE_NCP_WPANTUND
    int mSocket;
#endif
    uint8_t mExtPanId[kSizeExtPanId];
    char    mNetworkName[kSizeNetworkName + 1];
    State   mState;
    bool    mThreadStarted;
    bool    mPSKcInitialized;
};

/**
 * @}
 */

} // namespace BorderRouter

} // namespace ot

#endif // BORDER_AGENT_HPP_
