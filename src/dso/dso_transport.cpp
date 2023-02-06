/*
 *    Copyright (c) 2023, The OpenThread Authors.
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

#if OTBR_ENABLE_DNS_DSO

#define OTBR_LOG_TAG "DSO"

#include "dso_transport.hpp"

#include <cinttypes>
#include <csignal>
#include <memory>
#include <netdb.h>
#include <sys/types.h>

#include "mbedtls/net_sockets.h"
#include "openthread/openthread-system.h"
#include "openthread/platform/dso_transport.h"

static otbr::Dso::DsoAgent *sDsoAgent = nullptr;

extern "C" void otPlatDsoEnableListening(otInstance *aInstance, bool aEnabled)
{
    sDsoAgent->SetEnabled(aInstance, aEnabled);
}

extern "C" void otPlatDsoConnect(otPlatDsoConnection *aConnection, const otSockAddr *aPeerSockAddr)
{
    sDsoAgent->FindOrCreateConnection(aConnection)->Connect(aPeerSockAddr);
}

extern "C" void otPlatDsoSend(otPlatDsoConnection *aConnection, otMessage *aMessage)
{
    auto conn = sDsoAgent->FindConnection(aConnection);

    VerifyOrExit(conn != nullptr);
    conn->Send(aMessage);

exit:
    otMessageFree(aMessage);
}

extern "C" void otPlatDsoDisconnect(otPlatDsoConnection *aConnection, otPlatDsoDisconnectMode aMode)
{
    auto conn = sDsoAgent->FindConnection(aConnection);

    VerifyOrExit(conn != nullptr);
    conn->Disconnect(aMode);

    sDsoAgent->RemoveConnection(aConnection);

exit:
    return;
}

namespace otbr {
namespace Dso {

DsoAgent::DsoAgent(Ncp::ControllerOpenThread &aNcp)
    : mNcp(aNcp)
{
    signal(SIGPIPE, SIG_IGN);
    mbedtls_net_init(&mListeningCtx);
    sDsoAgent = this;
}

DsoAgent::DsoConnection *DsoAgent::FindConnection(otPlatDsoConnection *aConnection)
{
    DsoConnection *ret = nullptr;
    auto           it  = mMap.find(aConnection);

    if (it != mMap.end())
    {
        ret = it->second.get();
    }

    return ret;
}

DsoAgent::DsoConnection *DsoAgent::FindOrCreateConnection(otPlatDsoConnection *aConnection)
{
    auto &ret = mMap[aConnection];

    if (!ret)
    {
        ret = MakeUnique<DsoConnection>(aConnection);
    }

    return ret.get();
}

DsoAgent::DsoConnection *DsoAgent::FindOrCreateConnection(otPlatDsoConnection *aConnection, mbedtls_net_context aCtx)
{
    auto &ret = mMap[aConnection];

    if (!ret)
    {
        ret = MakeUnique<DsoConnection>(aConnection, aCtx);
    }

    return ret.get();
}

void DsoAgent::ProcessConnections(const MainloopContext &aMainloop)
{
    std::vector<DsoConnection *> connections;

    connections.reserve(mMap.size());
    for (auto &conn : mMap)
    {
        connections.push_back(conn.second.get());
    }
    for (const auto &conn : connections)
    {
        switch (conn->GetState())
        {
        case DsoConnection::State::kDisabled:
            RemoveConnection(conn->GetOtPlatDsoConnection());
            break;
        case DsoConnection::State::kConnecting:
            if (FD_ISSET(conn->GetFd(), &aMainloop.mWriteFdSet))
            {
                conn->UpdateStateBySocketState();
            }
            break;
        case DsoConnection::State::kConnected:
            if (FD_ISSET(conn->GetFd(), &aMainloop.mReadFdSet))
            {
                conn->HandleReceive();
            }
            break;
        default:
            break;
        }
    }
}

void DsoAgent::Update(MainloopContext &aMainloop)
{
    if (mListeningEnabled)
    {
        FD_SET(mListeningCtx.fd, &aMainloop.mReadFdSet);
        aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, mListeningCtx.fd);
    }

    for (const auto &pair : mMap)
    {
        switch (pair.second->GetState())
        {
        case DsoConnection::State::kDisabled:
            break;
        case DsoConnection::State::kConnecting:
            FD_SET(pair.second->GetFd(), &aMainloop.mWriteFdSet);
            aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, pair.second->GetFd());
            break;
        case DsoConnection::State::kConnected:
            FD_SET(pair.second->GetFd(), &aMainloop.mReadFdSet);
            aMainloop.mMaxFd = std::max(aMainloop.mMaxFd, pair.second->GetFd());
            break;
        default:
            break;
        }
    }
}

void DsoAgent::Process(const MainloopContext &aMainloop)
{
    if (FD_ISSET(mListeningCtx.fd, &aMainloop.mReadFdSet))
    {
        HandleIncomingConnections();
    }
    ProcessConnections(aMainloop);
}

void DsoAgent::HandleIncomingConnections()
{
    mbedtls_net_context incomingCtx;
    uint8_t             address[sizeof(sockaddr_in6)];
    size_t              len;
    int                 ret = 0;

    VerifyOrExit(mListeningEnabled);

    while (!(ret = mbedtls_net_accept(&mListeningCtx, &incomingCtx, &address, sizeof(address), &len)))
    {
        HandleIncomingConnection(incomingCtx, address, len);
    }

    if (ret != MBEDTLS_ERR_SSL_WANT_READ)
    {
        otbrLogWarning("Failed to accept incoming connection: %d", ret);
    }

exit:

    return;
}

void DsoAgent::Enable(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    assert(aInstance == mNcp.GetInstance());

    constexpr int kOne = 1;
    sockaddr_in6  sockAddr;

    VerifyOrExit(!mListeningEnabled);

    mListeningCtx.fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    VerifyOrExit(!setsockopt(mListeningCtx.fd, SOL_SOCKET, SO_BINDTODEVICE, otSysGetInfraNetifName(),
                             strlen(otSysGetInfraNetifName())));
    VerifyOrExit(!setsockopt(mListeningCtx.fd, SOL_SOCKET, SO_REUSEADDR, (const uint8_t *)&kOne, sizeof(kOne)));
    VerifyOrExit(!setsockopt(mListeningCtx.fd, SOL_SOCKET, SO_REUSEPORT, (const uint8_t *)&kOne, sizeof(kOne)));

    sockAddr.sin6_family = AF_INET6;
    sockAddr.sin6_addr   = in6addr_any;
    sockAddr.sin6_port   = htons(kListeningPort);
    VerifyOrExit(!bind(mListeningCtx.fd, (struct sockaddr *)&sockAddr, sizeof(sockAddr)));
    VerifyOrExit(!mbedtls_net_set_nonblock(&mListeningCtx), otbrLogWarning("Failed to set non-blocking: %d", errno));
    VerifyOrExit(!listen(mListeningCtx.fd, kMaxQueuedConnections));

    mListeningEnabled = true;

    otbrLogInfo("DSO socket starts listening");

exit:
    return;
}

void DsoAgent::Disable(otInstance *aInstance)
{
    OTBR_UNUSED_VARIABLE(aInstance);

    assert(aInstance == mNcp.GetInstance());

    VerifyOrExit(mListeningEnabled);

    mbedtls_net_close(&mListeningCtx);
    mbedtls_net_free(&mListeningCtx);
    mMap.clear();
    mListeningEnabled = false;

exit:
    return;
}

void DsoAgent::SetEnabled(otInstance *aInstance, bool aEnabled)
{
    if (aEnabled)
    {
        Enable(aInstance);
    }
    else
    {
        Disable(aInstance);
    }
}

void DsoAgent::RemoveConnection(otPlatDsoConnection *aConnection)
{
    mMap.erase(aConnection);
}

void DsoAgent::DsoConnection::Connect(const otSockAddr *aPeerSockAddr)
{
    int                 ret;
    char                addrBuf[OT_IP6_ADDRESS_STRING_SIZE];
    std::string         portString;
    struct sockaddr_in6 sockAddrIn6;

    VerifyOrExit(mState == State::kDisabled);

    mbedtls_net_init(&mCtx);
    mPeerSockAddr = *aPeerSockAddr;
    portString    = std::to_string(aPeerSockAddr->mPort);
    otIp6AddressToString(&aPeerSockAddr->mAddress, addrBuf, sizeof(addrBuf));

    otbrLogInfo("Connecting to %s:%s", addrBuf, portString.c_str());

    mCtx.fd = socket(AF_INET6, SOCK_STREAM, 0);
    VerifyOrExit(mCtx.fd > 0, otbrLogWarning("Failed to create a socket: %d", errno));
    VerifyOrExit(!mbedtls_net_set_nonblock(&mCtx), otbrLogWarning("Failed to set non-blocking: %d", errno));

    memset(&sockAddrIn6, 0, sizeof(sockAddrIn6));
    memcpy(sockAddrIn6.sin6_addr.s6_addr, aPeerSockAddr->mAddress.mFields.m8, sizeof(sockAddrIn6.sin6_addr.s6_addr));
    sockAddrIn6.sin6_family = AF_INET6;
    sockAddrIn6.sin6_port   = htons(aPeerSockAddr->mPort);
    ret                     = connect(mCtx.fd, reinterpret_cast<struct sockaddr *>(&sockAddrIn6), sizeof(sockAddrIn6));

    if (!ret)
    {
        otbrLogInfo("Connected %s:%s", addrBuf, portString.c_str());
        mState = State::kConnected;
        otPlatDsoHandleConnected(mConnection);
    }
    else if (errno == EAGAIN || errno == EINPROGRESS)
    {
        mState = State::kConnecting;
    }

exit:
    if (mState == State::kDisabled)
    {
        otPlatDsoHandleDisconnected(mConnection, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
    }
}

void DsoAgent::DsoConnection::Disconnect(otPlatDsoDisconnectMode aMode)
{
    struct linger l;

    VerifyOrExit(mState != State::kDisabled);

    switch (aMode)
    {
    case OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT:
        l.l_onoff  = 1;
        l.l_linger = 0;
        setsockopt(mCtx.fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
        break;
    case OT_PLAT_DSO_DISCONNECT_MODE_GRACEFULLY_CLOSE:
        break;
    default:
        otbrLogWarning("Unknown disconnection mode: %d", aMode);
        break;
    }

    mbedtls_net_close(&mCtx);
    mbedtls_net_free(&mCtx);
    mState = State::kDisabled;

exit:
    return;
}

void DsoAgent::DsoConnection::Send(otMessage *aMessage)
{
    uint16_t             length = otMessageGetLength(aMessage);
    std::vector<uint8_t> buf(length + sizeof(uint16_t));
    uint16_t             lengthInBigEndian = htons(length);
    int                  writeLen;

    VerifyOrExit(mState == State::kConnected);

    otbrLogDebug("Sending a message with length %" PRIu16, length);

    memcpy(buf.data(), &lengthInBigEndian, sizeof(lengthInBigEndian));
    VerifyOrExit(length == otMessageRead(aMessage, 0, buf.data() + sizeof(lengthInBigEndian), length),
                 otbrLogWarning("Failed to read message data"));
    writeLen = mbedtls_net_send(&mCtx, buf.data(), buf.size());
    if (writeLen < length)
    {
        otbrLogWarning("Failed to send DSO message: %d", writeLen);
    }
    HandleMbedTlsError(writeLen);

    // TODO: May need to keep sending until all the data is sent
exit:
    return;
}

void DsoAgent::DsoConnection::HandleReceive(void)
{
    int     readLen;
    uint8_t buf[kRxBufferSize];

    VerifyOrExit(mState == State::kConnected);

    while (true)
    {
        if (mNeedBytes)
        {
            readLen = mbedtls_net_recv(&mCtx, buf, std::min(sizeof(buf), mNeedBytes));

            if (readLen <= 0)
            {
                HandleMbedTlsError(readLen);
                VerifyOrExit(readLen != 0);
                VerifyOrExit(readLen != MBEDTLS_ERR_SSL_WANT_READ && readLen != MBEDTLS_ERR_SSL_WANT_WRITE);
                otbrLogWarning("Failed to receive message: %d", readLen);
                ExitNow();
            }
            SuccessOrExit(otMessageAppend(mPendingMessage, buf, readLen));
            mNeedBytes -= readLen;

            if (!mNeedBytes)
            {
                otPlatDsoHandleReceive(mConnection, mPendingMessage);
                mPendingMessage = nullptr;
            }
        }
        else
        {
            assert(mLengthBuffer.size() < sizeof(uint16_t));
            assert(mPendingMessage == nullptr);

            readLen = mbedtls_net_recv(&mCtx, buf, sizeof(uint16_t) - mLengthBuffer.size());

            if (readLen <= 0)
            {
                HandleMbedTlsError(readLen);
                VerifyOrExit(readLen != 0);
                VerifyOrExit(readLen != MBEDTLS_ERR_SSL_WANT_READ && readLen != MBEDTLS_ERR_SSL_WANT_WRITE);
                otbrLogWarning("Failed to receive message: %d", readLen);
                ExitNow();
            }
            for (int i = 0; i < readLen; ++i)
            {
                mLengthBuffer.push_back(buf[i]);
            }
            if (mLengthBuffer.size() == sizeof(uint16_t))
            {
                mNeedBytes      = mLengthBuffer[0] << 8 | mLengthBuffer[1];
                mPendingMessage = otIp6NewMessage(otPlatDsoGetInstance(mConnection), nullptr);
                mLengthBuffer.clear();
                VerifyOrExit(mPendingMessage != nullptr);
            }
        }
    }

exit:
    return;
}

void DsoAgent::DsoConnection::HandleMbedTlsError(int aError)
{
    VerifyOrExit(aError < 0);

    switch (aError)
    {
    case MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY:
        otPlatDsoHandleDisconnected(mConnection, OT_PLAT_DSO_DISCONNECT_MODE_GRACEFULLY_CLOSE);
        mState = State::kDisabled;
        break;
    case MBEDTLS_ERR_NET_CONN_RESET:
        otPlatDsoHandleDisconnected(mConnection, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
        mState = State::kDisabled;
        break;
    case MBEDTLS_ERR_SSL_WANT_READ:
    case MBEDTLS_ERR_SSL_WANT_WRITE:
        break;
    default:
        otPlatDsoHandleDisconnected(mConnection, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
        mState = State::kDisabled;
        break;
    }

exit:
    return;
}

void DsoAgent::DsoConnection::UpdateStateBySocketState(void)
{
    int       optVal;
    socklen_t optLen;

    otbrLogDebug("Updating state");

    if (!getsockopt(GetFd(), SOL_SOCKET, SO_ERROR, &optVal, &optLen))
    {
        if (!optVal)
        {
            mState = State::kConnected;
            otPlatDsoHandleConnected(mConnection);
            otbrLogDebug("Connected");
        }
        else
        {
            mState = State::kDisabled;
            otPlatDsoHandleDisconnected(mConnection, OT_PLAT_DSO_DISCONNECT_MODE_FORCIBLY_ABORT);
            otbrLogDebug("Disconnected");
        }
    }
    else
    {
        otbrLogWarning("Failed to query socket status: %d", errno);
    }
}

void DsoAgent::HandleIncomingConnection(mbedtls_net_context aCtx, uint8_t *aAddress, size_t aAddressLength)
{
    otSockAddr           sockAddr;
    otPlatDsoConnection *conn;
    in6_addr            *addrIn6    = nullptr;
    bool                 successful = false;

    VerifyOrExit(!mbedtls_net_set_nonblock(&aCtx), otbrLogWarning("Failed to set the socket as non-blocking"));

    // TODO: support IPv4
    if (aAddressLength == OT_IP6_ADDRESS_SIZE)
    {
        Ip6Address address;

        addrIn6 = reinterpret_cast<in6_addr *>(aAddress);
        memcpy(&sockAddr.mAddress.mFields.m8, addrIn6, aAddressLength);
        sockAddr.mPort = 0; // Mbed TLS doesn't provide the client's port number.

        address.CopyFrom(*addrIn6);
        otbrLogInfo("Receiving connection from %s", address.ToString().c_str());
    }
    else
    {
        otbrLogInfo("Unsupported address length: %zu", aAddressLength);
        ExitNow();
    }

    conn = otPlatDsoAccept(mNcp.GetInstance(), &sockAddr);

    VerifyOrExit(conn != nullptr, otbrLogInfo("Failed to accept connection"));

    FindOrCreateConnection(conn, aCtx);
    otPlatDsoHandleConnected(conn);
    successful = true;

exit:
    if (!successful)
    {
        mbedtls_net_close(&aCtx);
    }
}

} // namespace Dso
} // namespace otbr

#endif
