/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "HdcpProfile.h"
#include "HdcpProfileImplementation.h"

#include "FactoriesImplementation.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "PowerManagerMock.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include "COMLinkMock.h"
#include "WrapsMock.h"
#include "WorkerPoolImplementation.h"

// New COMRPC-based DeviceSettings mocks
#include "DeviceSettingsMock.h"
#include "DeviceSettingsHostMock.h"
#include "DeviceSettingsVideoPortMock.h"
#include "DeviceSettingsDisplayMock.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using namespace WPEFramework;
using ::testing::NiceMock;

class HDCPProfileTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::HdcpProfile> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;

    WrapsImplMock *p_wrapsImplMock = nullptr;
    Core::ProxyType<Plugin::HdcpProfileImplementation> hdcpProfileImpl;

    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    NiceMock<FactoriesImplementation> factoriesImplementation;

    HDCPProfileTest()
        : plugin(Core::ProxyType<Plugin::HdcpProfile>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16))
    {
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                [this]() {
                    TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                    return &comLinkMock;
                }));

        // Setup DeviceSettings COMRPC mock
        ON_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [&](const uint32_t, const string&) -> void* {
                    auto* root = DeviceSettingsMock::Get();
                    root->AddRef();
                    return static_cast<Exchange::IDeviceSettings*>(root);
                }));

#ifdef USE_THUNDER_R4
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                    hdcpProfileImpl = Core::ProxyType<Plugin::HdcpProfileImplementation>::Create();
                    TEST_LOG("Pass created hdcpProfileImpl: %p ", &hdcpProfileImpl);
                    return &hdcpProfileImpl;
                }));
#else
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Return(hdcpProfileImpl));
#endif /*USE_THUNDER_R4 */

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
            plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        EXPECT_EQ(string(""), plugin->Initialize(&service));
    }

    virtual ~HDCPProfileTest() override
    {
        TEST_LOG("HdcpProfileTest Destructor");

        plugin->Deinitialize(&service);

        dispatcher->Deactivate();
        dispatcher->Release();

        workerPool->Stop();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr)
        {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }

        PluginHost::IFactories::Assign(nullptr);

        // Clean up DeviceSettings mocks
        DeviceSettingsMock::Delete();
    }
};

TEST_F(HDCPProfileTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getHDCPStatus")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSettopHDCPSupport")));
}

TEST_F(HDCPProfileTest, getHDCPStatus_isConnected_false)
{
    // Setup DeviceSettings mocks
    auto& hostMock = DeviceSettingsHostMock::Mock();
    auto& videoPortMock = DeviceSettingsVideoPortMock::Mock();

    // Mock: GetDefaultVideoPortName returns "HDMI0"
    ON_CALL(hostMock, GetDefaultVideoPortName(::testing::_))
        .WillByDefault(::testing::Invoke([](string& portName) {
            portName = "HDMI0";
            return Core::ERROR_NONE;
        }));

    // Mock: GetVideoPort returns handle 0 for HDMI0
    ON_CALL(videoPortMock, GetVideoPort(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IDeviceSettingsVideoPort::VideoPort, int32_t, int32_t& handle) {
            handle = 0;
            return Core::ERROR_NONE;
        }));

    // Mock: Display is NOT connected
    ON_CALL(videoPortMock, IsVideoPortDisplayConnected(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, bool& connected) {
            connected = false;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP status unpowered
    ON_CALL(videoPortMock, GetHDCPStatusOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPStatus& status) {
            status = Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_UNPOWERED;
            return Core::ERROR_NONE;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getHDCPStatus"), _T(""), response));
    EXPECT_THAT(response, ::testing::MatchesRegex(_T("\\{"
                                                     "\"HDCPStatus\":"
                                                     "\\{"
                                                     "\"isConnected\":false,"
                                                     "\"isHDCPCompliant\":false,"
                                                     "\"isHDCPEnabled\":false,"
                                                     "\"hdcpReason\":0,"
                                                     "\"supportedHDCPVersion\":\"[1-2]+.[1-4]\","
                                                     "\"receiverHDCPVersion\":\"[1-2]+.[1-4]\","
                                                     "\"currentHDCPVersion\":\"[1-2]+.[1-4]\""
                                                     "\\},"
                                                     "\"success\":true"
                                                     "\\}")));
}

TEST_F(HDCPProfileTest, getHDCPStatus_isConnected_true)
{
    // Setup DeviceSettings mocks
    auto& hostMock = DeviceSettingsHostMock::Mock();
    auto& videoPortMock = DeviceSettingsVideoPortMock::Mock();

    // Mock: GetDefaultVideoPortName returns "HDMI0"
    ON_CALL(hostMock, GetDefaultVideoPortName(::testing::_))
        .WillByDefault(::testing::Invoke([](string& portName) {
            portName = "HDMI0";
            return Core::ERROR_NONE;
        }));

    // Mock: GetVideoPort returns handle 0 for HDMI0
    ON_CALL(videoPortMock, GetVideoPort(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IDeviceSettingsVideoPort::VideoPort, int32_t, int32_t& handle) {
            handle = 0;
            return Core::ERROR_NONE;
        }));

    // Mock: Display IS connected
    ON_CALL(videoPortMock, IsVideoPortDisplayConnected(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, bool& connected) {
            connected = true;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP status authenticated
    ON_CALL(videoPortMock, GetHDCPStatusOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPStatus& status) {
            status = Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_AUTHENTICATED;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP is enabled
    ON_CALL(videoPortMock, IsHDCPEnabledOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP receiver protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPReceiverProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP current protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPCurrentProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getHDCPStatus"), _T(""), response));
    EXPECT_THAT(response, ::testing::MatchesRegex(_T("\\{"
                                                     "\"HDCPStatus\":"
                                                     "\\{"
                                                     "\"isConnected\":true,"
                                                     "\"isHDCPCompliant\":true,"
                                                     "\"isHDCPEnabled\":true,"
                                                     "\"hdcpReason\":2,"
                                                     "\"supportedHDCPVersion\":\"[1-2]+.[1-4]\","
                                                     "\"receiverHDCPVersion\":\"[1-2]+.[1-4]\","
                                                     "\"currentHDCPVersion\":\"[1-2]+.[1-4]\""
                                                     "\\},"
                                                     "\"success\":true"
                                                     "\\}")));
}

TEST_F(HDCPProfileTest, getSettopHDCPSupport_Hdcp_v1x)
{
    // Setup DeviceSettings mocks
    auto& hostMock = DeviceSettingsHostMock::Mock();
    auto& videoPortMock = DeviceSettingsVideoPortMock::Mock();

    // Mock: GetDefaultVideoPortName returns "HDMI0"
    ON_CALL(hostMock, GetDefaultVideoPortName(::testing::_))
        .WillByDefault(::testing::Invoke([](string& portName) {
            portName = "HDMI0";
            return Core::ERROR_NONE;
        }));

    // Mock: GetVideoPort returns handle 0 for HDMI0
    ON_CALL(videoPortMock, GetVideoPort(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IDeviceSettingsVideoPort::VideoPort, int32_t, int32_t& handle) {
            handle = 0;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP protocol version 1.x
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_1X;
            return Core::ERROR_NONE;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSettopHDCPSupport"), _T(""), response));
    EXPECT_THAT(response, ::testing::MatchesRegex(_T("\\{"
                                                     "\"supportedHDCPVersion\":\"[1-2]+.[1-4]\","
                                                     "\"isHDCPSupported\":true,"
                                                     "\"success\":true"
                                                     "\\}")));
}

TEST_F(HDCPProfileTest, getSettopHDCPSupport_Hdcp_v2x)
{
    // Setup DeviceSettings mocks
    auto& hostMock = DeviceSettingsHostMock::Mock();
    auto& videoPortMock = DeviceSettingsVideoPortMock::Mock();

    // Mock: GetDefaultVideoPortName returns "HDMI0"
    ON_CALL(hostMock, GetDefaultVideoPortName(::testing::_))
        .WillByDefault(::testing::Invoke([](string& portName) {
            portName = "HDMI0";
            return Core::ERROR_NONE;
        }));

    // Mock: GetVideoPort returns handle 0 for HDMI0
    ON_CALL(videoPortMock, GetVideoPort(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IDeviceSettingsVideoPort::VideoPort, int32_t, int32_t& handle) {
            handle = 0;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSettopHDCPSupport"), _T(""), response));
    EXPECT_THAT(response, ::testing::MatchesRegex(_T("\\{"
                                                     "\"supportedHDCPVersion\":\"[1-2]+.[1-4]\","
                                                     "\"isHDCPSupported\":true,"
                                                     "\"success\":true"
                                                     "\\}")));
}

// Event tests
class HDCPProfileEventTest : public HDCPProfileTest {
protected:
    HDCPProfileEventTest() : HDCPProfileTest() {}
    virtual ~HDCPProfileEventTest() override {}
};

TEST_F(HDCPProfileEventTest, onDisplayConnectionChanged)
{
    Core::Event onDisplayConnectionChanged(false, true);

    // Setup DeviceSettings mocks
    auto& hostMock = DeviceSettingsHostMock::Mock();
    auto& videoPortMock = DeviceSettingsVideoPortMock::Mock();

    // Mock: GetDefaultVideoPortName returns "HDMI0"
    ON_CALL(hostMock, GetDefaultVideoPortName(::testing::_))
        .WillByDefault(::testing::Invoke([](string& portName) {
            portName = "HDMI0";
            return Core::ERROR_NONE;
        }));

    // Mock: GetVideoPort returns handle 0 for HDMI0
    ON_CALL(videoPortMock, GetVideoPort(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IDeviceSettingsVideoPort::VideoPort, int32_t, int32_t& handle) {
            handle = 0;
            return Core::ERROR_NONE;
        }));

    // Mock: Display IS connected
    ON_CALL(videoPortMock, IsVideoPortDisplayConnected(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, bool& connected) {
            connected = true;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP status authenticated
    ON_CALL(videoPortMock, GetHDCPStatusOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPStatus& status) {
            status = Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_AUTHENTICATED;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP is enabled
    ON_CALL(videoPortMock, IsHDCPEnabledOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP receiver protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPReceiverProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP current protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPCurrentProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));

                EXPECT_THAT(text, ::testing::MatchesRegex(_T("\\{"
                "\"jsonrpc\":\"2.0\","
                "\"method\":\"client.events.onDisplayConnectionChanged\","
                "\"params\":"
                "\\{\"HDCPStatus\":"
                "\\{"
                "\"isConnected\":true,"
                "\"isHDCPCompliant\":true,"
                "\"isHDCPEnabled\":true,"
                "\"hdcpReason\":2,"
                "\"supportedHDCPVersion\":\"2.2\","
                "\"receiverHDCPVersion\":\"2.2\","
                "\"currentHDCPVersion\":\"2.2\""
                "\\}"
                "\\}"
                "\\}")));

                onDisplayConnectionChanged.SetEvent();

                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onDisplayConnectionChanged"), _T("client.events"), message);

    // Trigger the event using the public test method
    Plugin::HdcpProfileImplementation::_instance->OnDisplayHDMIHotPlug(0); // 0 = CONNECTED

    EXPECT_EQ(Core::ERROR_NONE, onDisplayConnectionChanged.Lock());

    EVENT_UNSUBSCRIBE(0, _T("onDisplayConnectionChanged"), _T("client.events"), message);
}

TEST_F(HDCPProfileEventTest, onHdmiOutputHDCPStatusEvent)
{
    Core::Event onDisplayConnectionChanged(false, true);

    // Setup DeviceSettings mocks
    auto& hostMock = DeviceSettingsHostMock::Mock();
    auto& videoPortMock = DeviceSettingsVideoPortMock::Mock();

    // Mock: GetDefaultVideoPortName returns "HDMI0"
    ON_CALL(hostMock, GetDefaultVideoPortName(::testing::_))
        .WillByDefault(::testing::Invoke([](string& portName) {
            portName = "HDMI0";
            return Core::ERROR_NONE;
        }));

    // Mock: GetVideoPort returns handle 0 for HDMI0
    ON_CALL(videoPortMock, GetVideoPort(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IDeviceSettingsVideoPort::VideoPort, int32_t, int32_t& handle) {
            handle = 0;
            return Core::ERROR_NONE;
        }));

    // Mock: Display IS connected
    ON_CALL(videoPortMock, IsVideoPortDisplayConnected(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, bool& connected) {
            connected = true;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP status authenticated
    ON_CALL(videoPortMock, GetHDCPStatusOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPStatus& status) {
            status = Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_AUTHENTICATED;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP is enabled
    ON_CALL(videoPortMock, IsHDCPEnabledOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, bool& enabled) {
            enabled = true;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP receiver protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPReceiverProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    // Mock: HDCP current protocol version 2.x
    ON_CALL(videoPortMock, GetHDCPCurrentProtocolVersionOnVideoPort(0, ::testing::_))
        .WillByDefault(::testing::Invoke([](int32_t, Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion& version) {
            version = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X;
            return Core::ERROR_NONE;
        }));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));

                EXPECT_THAT(text, ::testing::MatchesRegex(_T("\\{"
                "\"jsonrpc\":\"2.0\","
                "\"method\":\"client.events.onDisplayConnectionChanged\","
                "\"params\":"
                "\\{\"HDCPStatus\":"
                "\\{"
                "\"isConnected\":true,"
                "\"isHDCPCompliant\":true,"
                "\"isHDCPEnabled\":true,"
                "\"hdcpReason\":2,"
                "\"supportedHDCPVersion\":\"2.2\","
                "\"receiverHDCPVersion\":\"2.2\","
                "\"currentHDCPVersion\":\"2.2\""
                "\\}"
                "\\}"
                "\\}")));

                onDisplayConnectionChanged.SetEvent();

                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onDisplayConnectionChanged"), _T("client.events"), message);

    // Trigger the event using the public test method
    Plugin::HdcpProfileImplementation::_instance->OnHDCPStatusChange(2); // 2 = AUTHENTICATED

    EXPECT_EQ(Core::ERROR_NONE, onDisplayConnectionChanged.Lock());

    EVENT_UNSUBSCRIBE(0, _T("onDisplayConnectionChanged"), _T("client.events"), message);
}
