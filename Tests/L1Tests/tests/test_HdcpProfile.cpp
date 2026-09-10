/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#include "HdcpProfile.h"

#include "FactoriesImplementation.h"
#include "HostMock.h"
#include "ManagerMock.h"
#include "ServiceMock.h"
#include "dsDisplay.h"
#include "ThunderPortability.h"
#include "PowerManagerMock.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstdio>
#include "COMLinkMock.h"
#include "WrapsMock.h"
#include "WorkerPoolImplementation.h"
#include "HdcpProfileImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
using namespace WPEFramework;

using ::testing::NiceMock;

class DeviceSettingsMock : public Exchange::IDeviceSettings {
public:
    MOCK_METHOD(Core::hresult, Configure, (PluginHost::IShell* service), (override));
    MOCK_METHOD(Core::hresult, GetDeviceSettingConfigs, (DeviceSettingConfigs& configs), (override));
    MOCK_METHOD(uint32_t, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceId), (override));
};

class DeviceSettingsVideoPortMock : public Exchange::IDeviceSettingsVideoPort {
public:
    MOCK_METHOD(Core::hresult, Register, (const string clientName, INotification* notification), (override));
    MOCK_METHOD(Core::hresult, Unregister, (INotification* notification), (override));
    MOCK_METHOD(Core::hresult, GetVideoPort, (const VideoPort videoPort, const int32_t index, int32_t& handle), (override));
    MOCK_METHOD(Core::hresult, GetVideoPortResolutionConfig, (VideoPort videoPortType, IVideoPortResolutionIterator*& videoPortResolutions), (const, override));
    MOCK_METHOD(Core::hresult, IsVideoPortEnabled, (const int32_t handle, bool& enabled), (override));
    MOCK_METHOD(Core::hresult, IsVideoPortDisplayConnected, (const int32_t handle, bool& connected), (override));
    MOCK_METHOD(Core::hresult, IsVideoPortDisplaySurround, (const int32_t handle, bool& surround), (override));
    MOCK_METHOD(Core::hresult, GetVideoPortDisplaySurroundMode, (const int32_t handle, VideoPortSurroundMode& surroundMode), (override));
    MOCK_METHOD(Core::hresult, EnableVideoPort, (const int32_t handle, const bool enable), (override));
    MOCK_METHOD(Core::hresult, GetVideoPortResolution, (const int32_t handle, VideoPortResolution& videoPortResolution), (override));
    MOCK_METHOD(Core::hresult, SetVideoPortResolution, (const int32_t handle, const VideoPortResolution& videoPortResolution, const bool persist, const bool forceCompatibility), (override));
    MOCK_METHOD(Core::hresult, EnableHDCPOnVideoPort, (const int32_t handle, const bool hdcpEnable, const uint8_t hdcpKey[], const uint16_t hdcpKeySize), (override));
    MOCK_METHOD(Core::hresult, IsHDCPEnabledOnVideoPort, (const int32_t handle, bool& hdcpEnabled), (override));
    MOCK_METHOD(Core::hresult, GetHDCPStatusOnVideoPort, (const int32_t handle, HDCPStatus& hdcpStatus), (override));
    MOCK_METHOD(Core::hresult, GetHDCPProtocolVersionOnVideoPort, (const int32_t handle, HDCPProtocolVersion& hdcpVersion), (override));
    MOCK_METHOD(Core::hresult, GetHDCPReceiverProtocolVersionOnVideoPort, (const int32_t handle, HDCPProtocolVersion& hdcpVersion), (override));
    MOCK_METHOD(Core::hresult, GetHDCPCurrentProtocolVersionOnVideoPort, (const int32_t handle, HDCPProtocolVersion& hdcpVersion), (override));
    MOCK_METHOD(Core::hresult, IsVideoPortActive, (const int32_t handle, bool& active), (override));
    MOCK_METHOD(Core::hresult, GetTVHDRCapabilities, (const int32_t handle, int32_t& capabilities), (override));
    MOCK_METHOD(Core::hresult, GetTVSupportedResolutions, (const int32_t handle, int32_t& resolutions), (override));
    MOCK_METHOD(Core::hresult, SetForceDisable4K, (const int32_t handle, const bool disable), (override));
    MOCK_METHOD(Core::hresult, GetForceDisable4K, (const int32_t handle, bool& disabled), (override));
    MOCK_METHOD(Core::hresult, IsVideoPortOutputHDR, (const int32_t handle, bool& isHDR), (override));
    MOCK_METHOD(Core::hresult, ResetVideoPortOutputToSDR, (), (override));
    MOCK_METHOD(Core::hresult, GetHDMIPreference, (const int32_t handle, HDCPProtocolVersion& hdcpVersion), (override));
    MOCK_METHOD(Core::hresult, SetHDMIPreference, (const int32_t handle, const HDCPProtocolVersion hdcpVersion), (override));
    MOCK_METHOD(Core::hresult, GetVideoEOTF, (const int32_t handle, HDRStandard& hdrStandard), (override));
    MOCK_METHOD(Core::hresult, GetMatrixCoefficients, (const int32_t handle, DisplayMatrixCoefficients& matrixCoefficients), (override));
    MOCK_METHOD(Core::hresult, GetColorDepth, (const int32_t handle, uint32_t& colorDepth), (override));
    MOCK_METHOD(Core::hresult, GetColorSpace, (const int32_t handle, DisplayColorSpace& colorSpace), (override));
    MOCK_METHOD(Core::hresult, GetQuantizationRange, (const int32_t handle, DisplayQuantizationRange& quantizationRange), (override));
    MOCK_METHOD(Core::hresult, GetCurrentOutputSettings, (const int32_t handle, DSOutputSettings& outputSettings), (override));
    MOCK_METHOD(Core::hresult, SetBackgroundColor, (const int32_t handle, const VideoBackgroundColor backgroundColor), (override));
    MOCK_METHOD(Core::hresult, SetForceHDRMode, (const int32_t handle, const HDRStandard hdrMode), (override));
    MOCK_METHOD(Core::hresult, GetColorDepthCapabilities, (const int32_t handle, uint32_t& colorDepthCapabilities), (override));
    MOCK_METHOD(Core::hresult, GetPreferredColorDepth, (const int32_t handle, DisplayColorDepth& colorDepth, const bool persist), (override));
    MOCK_METHOD(Core::hresult, SetPreferredColorDepth, (const int32_t handle, const DisplayColorDepth colorDepth, const bool persist), (override));
    MOCK_METHOD(uint32_t, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceId), (override));
};

class HDCPProfileTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::HdcpProfile> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;

    HostImplMock             *p_hostImplMock = nullptr ;
    WrapsImplMock *p_wrapsImplMock = nullptr;
    Core::ProxyType<Plugin::HdcpProfileImplementation> hdcpProfileImpl;

    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    NiceMock<DeviceSettingsMock> deviceSettingsMock;
    NiceMock<DeviceSettingsVideoPortMock> videoPortMock;
    Core::Event deviceSettingsReady;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    NiceMock<FactoriesImplementation> factoriesImplementation;

    HDCPProfileTest()
        : plugin(Core::ProxyType<Plugin::HdcpProfile>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , deviceSettingsReady(false, true)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(2, Core::Thread::DefaultStackSize(), 16))
    {
        p_hostImplMock  = new NiceMock <HostImplMock>;
        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        printf("Pass created wrapsImplMock: %p ", p_wrapsImplMock);
        device::Host::setImpl(p_hostImplMock);
        Wraps::setImpl(p_wrapsImplMock);

        ON_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [this](const uint32_t interfaceId, const string& callsign) -> void* {
                    if ((interfaceId == Exchange::IDeviceSettings::ID) && (callsign == "org.rdk.DeviceSettings")) {
                        deviceSettingsMock.AddRef();
                        return static_cast<Exchange::IDeviceSettings*>(&deviceSettingsMock);
                    }
                    return nullptr;
                }));
        ON_CALL(deviceSettingsMock, QueryInterface(::testing::_))
            .WillByDefault(::testing::Invoke(
                [this](const uint32_t interfaceId) -> void* {
                    if (interfaceId == Exchange::IDeviceSettingsVideoPort::ID) {
                        videoPortMock.AddRef();
                        return static_cast<Exchange::IDeviceSettingsVideoPort*>(&videoPortMock);
                    }
                    return nullptr;
                }));
        ON_CALL(deviceSettingsMock, GetDeviceSettingConfigs(::testing::_))
            .WillByDefault(::testing::Invoke(
                [](Exchange::IDeviceSettings::DeviceSettingConfigs& configs) {
                    Exchange::IDeviceSettings::VideoPortTypeConfig typeConfig{};
                    typeConfig.typeId = Exchange::IDeviceSettingsVideoPort::DS_VIDEO_PORT_TYPE_HDMI;
                    typeConfig.name = "HDMI";
                    configs.videoPortTypes.push_back(typeConfig);

                    Exchange::IDeviceSettings::VideoPortPortConfig portConfig{};
                    portConfig.videoPortType = Exchange::IDeviceSettingsVideoPort::DS_VIDEO_PORT_TYPE_HDMI;
                    portConfig.videoPortIndex = 0;
                    portConfig.defaultResolution = "1080p";
                    configs.videoPorts.push_back(portConfig);
                    return Core::ERROR_NONE;
                }));
        ON_CALL(videoPortMock, GetVideoPort(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::DoAll(::testing::SetArgReferee<2>(1), ::testing::Return(Core::ERROR_NONE)));
        ON_CALL(videoPortMock, Register(::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [this](const string, Exchange::IDeviceSettingsVideoPort::INotification*) {
                    deviceSettingsReady.SetEvent();
                    return Core::ERROR_NONE;
                }));
        ON_CALL(videoPortMock, Unregister(::testing::_))
            .WillByDefault(::testing::Return(Core::ERROR_NONE));

        ON_CALL(service, COMLink())
        .WillByDefault(::testing::Invoke(
              [this]() {
                    TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                    return &comLinkMock;
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
        EXPECT_EQ(Core::ERROR_NONE, deviceSettingsReady.Lock(1000));

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
        device::Host::setImpl(nullptr);
        if (p_hostImplMock != nullptr)
        {
            delete p_hostImplMock;
            p_hostImplMock = nullptr;
        }
        
    }
};

class HDCPProfileDsTest : public HDCPProfileTest {
};

class HDCPProfileEventTest : public HDCPProfileDsTest {
};

class HDCPProfileEventIarmTest : public HDCPProfileEventTest {
public:
    device::Host::IDisplayDeviceEvents* _displayDeviceEvents = nullptr;
    device::Host::IVideoOutputPortEvents* _videoOutputPortEvents = nullptr;

protected:
    ManagerImplMock   *p_managerImplMock = nullptr ;

    HDCPProfileEventIarmTest()
        : HDCPProfileEventTest()
    {
        p_managerImplMock  = new NiceMock <ManagerImplMock>;
        device::Manager::setImpl(p_managerImplMock);

        EXPECT_CALL(*p_managerImplMock, Initialize())
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Return());

        // Deinitialize the instance created by parent class first
        plugin->Deinitialize(&service);

        // Small delay to ensure worker threads complete any pending jobs
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        EXPECT_EQ(string(""), plugin->Initialize(&service));
    }

    virtual ~HDCPProfileEventIarmTest() override
    {
        // Small delay to allow worker threads to complete pending dispatched events
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        plugin->Deinitialize(&service);
        device::Manager::setImpl(nullptr);
        if (p_managerImplMock != nullptr)
        {
            delete p_managerImplMock;
            p_managerImplMock = nullptr;
        }
    }
};

TEST_F(HDCPProfileTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getHDCPStatus")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getSettopHDCPSupport")));
}

TEST_F(HDCPProfileDsTest, getHDCPStatus_isConnected_false)
{
    ON_CALL(videoPortMock, IsVideoPortDisplayConnected(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(false), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPStatusOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_UNPOWERED), ::testing::Return(Core::ERROR_NONE)));

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

TEST_F(HDCPProfileDsTest, getHDCPStatus_isConnected_true)
{
    ON_CALL(videoPortMock, IsVideoPortDisplayConnected(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(true), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPStatusOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_AUTHENTICATED), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, IsHDCPEnabledOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(true), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPReceiverProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPCurrentProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));

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

TEST_F(HDCPProfileDsTest, getSettopHDCPSupport_Hdcp_v1x)
{
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_1X), ::testing::Return(Core::ERROR_NONE)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSettopHDCPSupport"), _T(""), response));
    EXPECT_THAT(response, ::testing::MatchesRegex(_T("\\{"
                                                     "\"supportedHDCPVersion\":\"[1-2]+.[1-4]\","
                                                     "\"isHDCPSupported\":true,"
                                                     "\"success\":true"
                                                     "\\}")));
}

TEST_F(HDCPProfileDsTest, getSettopHDCPSupport_Hdcp_v2x)
{
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getSettopHDCPSupport"), _T(""), response));
    EXPECT_THAT(response, ::testing::MatchesRegex(_T("\\{"
                                                     "\"supportedHDCPVersion\":\"[1-2]+.[1-4]\","
                                                     "\"isHDCPSupported\":true,"
                                                     "\"success\":true"
                                                     "\\}")));
}


TEST_F(HDCPProfileEventIarmTest, onDisplayConnectionChanged)
{
    Core::Event onDisplayConnectionChanged(false, true);

    ON_CALL(videoPortMock, IsVideoPortDisplayConnected(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(true), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPStatusOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_AUTHENTICATED), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, IsHDCPEnabledOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(true), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPReceiverProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPCurrentProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));

                //EXPECT_EQ(text, string(_T("")));
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

    Plugin::HdcpProfileImplementation::_instance->onHdmiOutputHotPlug(dsDISPLAY_EVENT_CONNECTED);
    
    EXPECT_EQ(Core::ERROR_NONE, onDisplayConnectionChanged.Lock());

    EVENT_UNSUBSCRIBE(0, _T("onDisplayConnectionChanged"), _T("client.events"), message);
}

TEST_F(HDCPProfileEventIarmTest, onHdmiOutputHDCPStatusEvent)
{
    Core::Event onDisplayConnectionChanged(false, true);

    ON_CALL(videoPortMock, IsVideoPortDisplayConnected(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(true), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPStatusOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_AUTHENTICATED), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, IsHDCPEnabledOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(true), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPReceiverProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));
    ON_CALL(videoPortMock, GetHDCPCurrentProtocolVersionOnVideoPort(::testing::_, ::testing::_))
        .WillByDefault(::testing::DoAll(::testing::SetArgReferee<1>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X), ::testing::Return(Core::ERROR_NONE)));

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

    Plugin::HdcpProfileImplementation::_instance->onHdcpStatusChangeNotification(dsHDCP_STATUS_AUTHENTICATED);

    EXPECT_EQ(Core::ERROR_NONE, onDisplayConnectionChanged.Lock());

    EVENT_UNSUBSCRIBE(0, _T("onDisplayConnectionChanged"), _T("client.events"), message);
}
