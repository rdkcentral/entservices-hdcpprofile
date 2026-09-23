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
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <cstring>
#include <interfaces/IHdcpProfile.h>

// HdcpProfile now talks to the real org.rdk.DeviceSettings plugin over COM-RPC.
// HAL mocks stand in for libds-hal so this test can control DeviceSettings behavior
// Note: HAL mocks (DsVideoPort, DsDisplay, DsHost) are automatically provided by L2TestsMock

#define JSON_TIMEOUT (1000)
#define COM_TIMEOUT (100)
#define EVNT_TIMEOUT (5000)
#define TEST_LOG(x, ...)                                                                                                                         \
    fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);
#define HDCPPROFILE_CALLSIGN _T("org.rdk.HdcpProfile.1")
#define HDCPPROFILE_L2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::IHdcpProfile;

typedef enum : uint32_t {
    HdcpProfile_OnDisplayConnectionChanged = 0x00000001,
    HdcpProfile_StateInvalid = 0x00000000
} HdcpProfileL2test_async_events_t;

/**
 * @brief Internal test mock class
 *
 * Note that this is for internal test use only and doesn't mock any actual
 * concrete interface.
 */
class AsyncHandlerMock_HdcpProfile {
public:
    AsyncHandlerMock_HdcpProfile() {
    }
    MOCK_METHOD(void, onDisplayConnectionChanged, (const IHdcpProfile::HDCPStatus& hdcpStatus));
};

/* Notification Handler Class for COM-RPC*/
class HdcpProfileNotificationHandler : public Exchange::IHdcpProfile::INotification {
private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

    /** @brief Last received HDCP status */
    IHdcpProfile::HDCPStatus m_lastHdcpStatus;

    BEGIN_INTERFACE_MAP(Notification)
    INTERFACE_ENTRY(Exchange::IHdcpProfile::INotification)
    END_INTERFACE_MAP

public:
    HdcpProfileNotificationHandler() : m_event_signalled(HdcpProfile_StateInvalid) {}
    ~HdcpProfileNotificationHandler() {}

    void onDisplayConnectionChanged(const IHdcpProfile::HDCPStatus& hdcpStatus) override {
        TEST_LOG("onDisplayConnectionChanged event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        m_lastHdcpStatus = hdcpStatus;
        TEST_LOG("  isConnected: %d", hdcpStatus.isConnected);
        TEST_LOG("  isHDCPCompliant: %d", hdcpStatus.isHDCPCompliant);
        TEST_LOG("  isHDCPEnabled: %d", hdcpStatus.isHDCPEnabled);
        TEST_LOG("  hdcpReason: %d", hdcpStatus.hdcpReason);
        TEST_LOG("  supportedHDCPVersion: %s", hdcpStatus.supportedHDCPVersion.c_str());
        TEST_LOG("  receiverHDCPVersion: %s", hdcpStatus.receiverHDCPVersion.c_str());
        TEST_LOG("  currentHDCPVersion: %s", hdcpStatus.currentHDCPVersion.c_str());

        /* Notify the requester thread. */
        m_event_signalled |= HdcpProfile_OnDisplayConnectionChanged;
        m_condition_variable.notify_one();
    }

    uint32_t WaitForRequestStatus(uint32_t timeout_ms, HdcpProfileL2test_async_events_t expected_status) {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        uint32_t signalled = HdcpProfile_StateInvalid;

        while (!(expected_status & m_event_signalled)) {
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                TEST_LOG("Timeout waiting for request status event");
                break;
            }
        }
        signalled = m_event_signalled;
        return signalled;
    }

    void ResetEvent() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled = HdcpProfile_StateInvalid;
    }

    IHdcpProfile::HDCPStatus GetLastHdcpStatus() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_lastHdcpStatus;
    }
};

/* HdcpProfile L2 test class declaration */
class HdcpProfile_L2test : public L2TestMocks {
protected:
    Core::JSONRPC::Message message;
    string response;

    virtual ~HdcpProfile_L2test() override;

public:
    HdcpProfile_L2test();
    
    // Captured from HAL callbacks - used to simulate HAL events
    dsHdcpStatusCallback_t m_dsHdcpStatusCallback = nullptr;
    dsHdmiHotPlugEventCallback_t m_dsHdmiHotPlugCallback = nullptr;
    
    // HAL Mocks are now provided by L2TestsMock parent class:
    // - p_dsVideoPortHalMock
    // - p_dsDisplayHalMock
    // - p_dsHostHalMock
    // - p_telemetryApiImplMock
    
    uint32_t CreateHdcpProfileInterfaceObjectUsingComRPCConnection();

    /**
     * @brief waits for various status change on asynchronous calls
     */
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, HdcpProfileL2test_async_events_t expected_status);

private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

protected:
    /** @brief Pointer to the IShell interface */
    PluginHost::IShell *m_controller_HdcpProfile;

    /** @brief Pointer to the IHdcpProfile interface */
    Exchange::IHdcpProfile *m_HdcpProfileplugin;

    Core::Sink<HdcpProfileNotificationHandler> notify;
};

/**
 * @brief Constructor for HdcpProfile L2 test class
 */
HdcpProfile_L2test::HdcpProfile_L2test()
    : L2TestMocks() {
    uint32_t status = Core::ERROR_GENERAL;
    m_event_signalled = HdcpProfile_StateInvalid;

    // TelemetryApi mock is already registered by L2TestMocks (p_telemetryApiImplMock);
    // calling TelemetryApi::setImpl() again here would fail the (nullptr == impl) guard.
    ON_CALL(*p_telemetryApiImplMock, t2_init(::testing::_)).WillByDefault(::testing::Return());
    ON_CALL(*p_telemetryApiImplMock, t2_uninit()).WillByDefault(::testing::Return());
    ON_CALL(*p_telemetryApiImplMock, t2_event_s(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(T2ERROR_SUCCESS));
    ON_CALL(*p_telemetryApiImplMock, t2_event_d(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(T2ERROR_SUCCESS));
    ON_CALL(*p_telemetryApiImplMock, t2_event_f(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(T2ERROR_SUCCESS));
    TEST_LOG("TelemetryApi mock initialized");

    // Configure HdcpProfile-specific HAL mock behaviors
    // Note: Common Init/Term behaviors are already set up by L2TestsMock
    // HdcpProfile plugin uses IDeviceSettingsVideoPort and IDeviceSettingsDisplay interfaces
    // So we need VideoPort, Display, and Host HAL mocks
    
    // 1. Host HAL Mock - Default video port name
    ON_CALL(*p_dsHostHalMock, dsGetDefaultVideoPortName(::testing::_))
        .WillByDefault(::testing::Invoke(
            [](dsVideoPortType_t* portType) {
                if (portType) { *portType = dsVIDEOPORT_TYPE_HDMI; }
                return dsERR_NONE;
            }));
    TEST_LOG("DsHostApi HdcpProfile-specific behaviors configured");
    
    // 2. VideoPort HAL Mock - HDCP functionality
    ON_CALL(*p_dsVideoPortHalMock, dsGetVideoPort(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](dsVideoPortType_t, int, intptr_t* handle) {
                if (handle) { *handle = 1; }
                return dsERR_NONE;
            }));
    
    ON_CALL(*p_dsVideoPortHalMock, dsIsDisplayConnected(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](intptr_t, bool* connected) {
                if (connected) { *connected = true; }
                return dsERR_NONE;
            }));
    
    ON_CALL(*p_dsVideoPortHalMock, dsGetHDCPStatus(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](intptr_t, dsHdcpStatus_t* status) {
                if (status) { *status = dsHDCP_STATUS_AUTHENTICATED; }
                return dsERR_NONE;
            }));
    
    ON_CALL(*p_dsVideoPortHalMock, dsGetHDCPProtocol(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](intptr_t, dsHdcpProtocolVersion_t* version) {
                if (version) { *version = dsHDCP_VERSION_2X; }
                return dsERR_NONE;
            }));
    
    ON_CALL(*p_dsVideoPortHalMock, dsGetHDCPReceiverProtocol(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](intptr_t, dsHdcpProtocolVersion_t* version) {
                if (version) { *version = dsHDCP_VERSION_2X; }
                return dsERR_NONE;
            }));
    
    ON_CALL(*p_dsVideoPortHalMock, dsGetHDCPCurrentProtocol(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](intptr_t, dsHdcpProtocolVersion_t* version) {
                if (version) { *version = dsHDCP_VERSION_2X; }
                return dsERR_NONE;
            }));
    
    ON_CALL(*p_dsVideoPortHalMock, dsIsHDCPEnabled(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](intptr_t, bool* enabled) {
                if (enabled) { *enabled = true; }
                return dsERR_NONE;
            }));
    
    // Register callback to capture HDCP status change events
    ON_CALL(*p_dsVideoPortHalMock, dsRegisterHdcpStatusCallback(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](intptr_t, dsHdcpStatusCallback_t cbFunc) {
                m_dsHdcpStatusCallback = cbFunc;
                TEST_LOG("Captured dsHdcpStatusCallback");
                return dsERR_NONE;
            }));
    
    TEST_LOG("DsVideoPortApi HdcpProfile-specific behaviors configured");
    
    // 3. Display HAL Mock - HDMI hotplug functionality
    ON_CALL(*p_dsDisplayHalMock, dsGetDisplay(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](dsVideoPortType_t, int, intptr_t* handle) {
                if (handle) { *handle = 1; }
                return dsERR_NONE;
            }));
    
    // Register callback to capture HDMI hotplug events
    ON_CALL(*p_dsDisplayHalMock, dsRegisterHdmiHotPlugEventCallback(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](intptr_t, dsHdmiHotPlugEventCallback_t cbFunc) {
                m_dsHdmiHotPlugCallback = cbFunc;
                TEST_LOG("Captured dsHdmiHotPlugEventCallback");
                return dsERR_NONE;
            }));
    
    TEST_LOG("DsDisplayApi HdcpProfile-specific behaviors configured");
    
    // Note: Audio, FPD, and HdmiIn HAL mocks are set up by L2TestsMock
    // HdcpProfile uses IDeviceSettingsVideoPort and IDeviceSettingsDisplay interfaces
    // But DeviceSettings plugin initializes ALL sub-implementations, so L2TestsMock provides
    // robust default mocks for all HAL APIs to prevent crashes during DeviceSettings activation
    
    TEST_LOG("HdcpProfile HAL mock setup complete - VideoPort, Display, and Host configured");

    // Mock PowerManager HAL for DeviceSettings dependency
    // Note: PowerManager activation is optional - DeviceSettings can work without it
    // Set up mocks but allow them to not be called
    EXPECT_CALL(*p_powerManagerHalMock, PLAT_DS_INIT())
        .Times(::testing::AtMost(1))
        .WillRepeatedly(::testing::Return(DEEPSLEEPMGR_SUCCESS));

    EXPECT_CALL(*p_powerManagerHalMock, PLAT_INIT())
        .Times(::testing::AtMost(1))
        .WillRepeatedly(::testing::Return(PWRMGR_SUCCESS));

    EXPECT_CALL(*p_powerManagerHalMock, PLAT_API_SetWakeupSrc(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Return(PWRMGR_SUCCESS));

    EXPECT_CALL(*p_powerManagerHalMock, PLAT_API_GetPowerState(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [](PWRMgr_PowerState_t* powerState) {
                *powerState = PWRMGR_POWERSTATE_ON;
                return PWRMGR_SUCCESS;
            }));

    TEST_LOG("PowerManager HAL mock setup complete");

    // Activate DeviceSettings plugin first (required dependency for HdcpProfile)
    TEST_LOG("Activating DeviceSettings plugin...");
    status = ActivateService("org.rdk.DeviceSettings");
    if (status != Core::ERROR_NONE) {
        TEST_LOG("Failed to activate DeviceSettings: %d (%s)", status, Core::ErrorToString(status));
    } else {
        TEST_LOG("DeviceSettings activated successfully");
        // Give DeviceSettings time to initialize
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Activate HdcpProfile plugin with retry mechanism
    TEST_LOG("Activating HdcpProfile plugin...");
    int retry_count = 0;
    const int max_retries = 10;
    status = Core::ERROR_GENERAL;
    
    while (status != Core::ERROR_NONE && retry_count < max_retries) {
        status = ActivateService("org.rdk.HdcpProfile");
        if (status != Core::ERROR_NONE) {
            TEST_LOG("ActivateService attempt %d/%d returned: %d (%s)", 
                     retry_count + 1, max_retries, status, Core::ErrorToString(status));
            retry_count++;
            if (retry_count < max_retries) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        } else {
            TEST_LOG("ActivateService succeeded on attempt %d", retry_count + 1);
        }
    }
    
    if (status != Core::ERROR_NONE) {
        TEST_LOG("Failed to activate HdcpProfile after %d attempts", max_retries);
    }
}

/**
 * @brief Destructor for HdcpProfile L2 test class
 */
HdcpProfile_L2test::~HdcpProfile_L2test() {
    TEST_LOG("HdcpProfile_L2test Destructor");
    
    // Deactivate plugins in reverse order
    DeactivateService("org.rdk.HdcpProfile");
    DeactivateService("org.rdk.DeviceSettings");
}

/**
 * @brief Create HdcpProfile interface object using COM-RPC connection
 */
uint32_t HdcpProfile_L2test::CreateHdcpProfileInterfaceObjectUsingComRPCConnection() {
    string token;
    // Get the Controller for HdcpProfile plugin
    auto interface = m_controller->QueryInterfaceByCallsign<PluginHost::IShell>(HDCPPROFILE_CALLSIGN);
    if (interface == nullptr) {
        TEST_LOG("Failed to get Controller for HdcpProfile plugin");
        return Core::ERROR_UNAVAILABLE;
    }

    m_controller_HdcpProfile = interface;
    /* Activate the plugin */
    auto result = m_controller_HdcpProfile->Activate(PluginHost::IShell::REQUESTED);
    if (result != Core::ERROR_NONE) {
        TEST_LOG("Failed to activate HdcpProfile plugin: %d", result);
        return result;
    }

    /* Get the HdcpProfile interface */
    m_HdcpProfileplugin = m_controller_HdcpProfile->QueryInterface<Exchange::IHdcpProfile>();
    if (m_HdcpProfileplugin == nullptr) {
        TEST_LOG("Failed to get IHdcpProfile interface");
        return Core::ERROR_UNAVAILABLE;
    }

    TEST_LOG("Successfully created HdcpProfile COM-RPC interface");
    return Core::ERROR_NONE;
}

/**
 * @brief Wait for request status
 */
uint32_t HdcpProfile_L2test::WaitForRequestStatus(uint32_t timeout_ms, HdcpProfileL2test_async_events_t expected_status) {
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = HdcpProfile_StateInvalid;

    while (!(expected_status & m_event_signalled)) {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
            TEST_LOG("Timeout waiting for request status event");
            break;
        }
    }
    signalled = m_event_signalled;
    return signalled;
}

/**
 * @brief Test GetSettopHDCPSupport via COM-RPC
 */
TEST_F(HdcpProfile_L2test, GetSettopHDCPSupport_COMRPC)
{
    TEST_LOG("Testing GetSettopHDCPSupport via COM-RPC");
    
    if (CreateHdcpProfileInterfaceObjectUsingComRPCConnection() != Core::ERROR_NONE) {
        FAIL() << "Failed to create HdcpProfile COM-RPC interface";
    }
    
    ASSERT_NE(m_controller_HdcpProfile, nullptr);
    ASSERT_NE(m_HdcpProfileplugin, nullptr);
    
    string supportedHDCPVersion;
    bool isHDCPSupported = false;
    bool success = false;
    
    uint32_t result = m_HdcpProfileplugin->GetSettopHDCPSupport(supportedHDCPVersion, isHDCPSupported, success);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    EXPECT_TRUE(isHDCPSupported);
    EXPECT_FALSE(supportedHDCPVersion.empty());
    
    TEST_LOG("Settop HDCP Support:");
    TEST_LOG("  isHDCPSupported: %d", isHDCPSupported);
    TEST_LOG("  supportedHDCPVersion: %s", supportedHDCPVersion.c_str());
    
    m_HdcpProfileplugin->Release();
    m_controller_HdcpProfile->Release();
}

/**
 * @brief Test GetHDCPStatus via COM-RPC
 */
TEST_F(HdcpProfile_L2test, GetHDCPStatus_COMRPC)
{
    TEST_LOG("Testing GetHDCPStatus via COM-RPC");
    
    if (CreateHdcpProfileInterfaceObjectUsingComRPCConnection() != Core::ERROR_NONE) {
        FAIL() << "Failed to create HdcpProfile COM-RPC interface";
    }
    
    ASSERT_NE(m_controller_HdcpProfile, nullptr);
    ASSERT_NE(m_HdcpProfileplugin, nullptr);
    
    IHdcpProfile::HDCPStatus hdcpStatus;
    bool success = false;
    
    uint32_t result = m_HdcpProfileplugin->GetHDCPStatus(hdcpStatus, success);
    
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    EXPECT_TRUE(hdcpStatus.isConnected);
    EXPECT_FALSE(hdcpStatus.supportedHDCPVersion.empty());
    
    TEST_LOG("HDCP Status:");
    TEST_LOG("  isConnected: %d", hdcpStatus.isConnected);
    TEST_LOG("  isHDCPCompliant: %d", hdcpStatus.isHDCPCompliant);
    TEST_LOG("  isHDCPEnabled: %d", hdcpStatus.isHDCPEnabled);
    TEST_LOG("  hdcpReason: %d", hdcpStatus.hdcpReason);
    TEST_LOG("  supportedHDCPVersion: %s", hdcpStatus.supportedHDCPVersion.c_str());
    TEST_LOG("  receiverHDCPVersion: %s", hdcpStatus.receiverHDCPVersion.c_str());
    TEST_LOG("  currentHDCPVersion: %s", hdcpStatus.currentHDCPVersion.c_str());
    
    m_HdcpProfileplugin->Release();
    m_controller_HdcpProfile->Release();
}

/**
 * @brief Test Register and Unregister via COM-RPC
 */
TEST_F(HdcpProfile_L2test, RegisterUnregister_COMRPC)
{
    TEST_LOG("Testing Register and Unregister via COM-RPC");
    
    if (CreateHdcpProfileInterfaceObjectUsingComRPCConnection() != Core::ERROR_NONE) {
        FAIL() << "Failed to create HdcpProfile COM-RPC interface";
    }
    
    ASSERT_NE(m_controller_HdcpProfile, nullptr);
    ASSERT_NE(m_HdcpProfileplugin, nullptr);
    
    // Register for notifications
    uint32_t result = m_HdcpProfileplugin->Register(&notify);
    EXPECT_EQ(result, Core::ERROR_NONE);
    TEST_LOG("Successfully registered for notifications");
    
    // Unregister from notifications
    result = m_HdcpProfileplugin->Unregister(&notify);
    EXPECT_EQ(result, Core::ERROR_NONE);
    TEST_LOG("Successfully unregistered from notifications");
    
    m_HdcpProfileplugin->Release();
    m_controller_HdcpProfile->Release();
}

/**
 * @brief Test onDisplayConnectionChanged notification via HDMI hotplug event
 */
TEST_F(HdcpProfile_L2test, OnDisplayConnectionChanged_HdmiHotplug_COMRPC)
{
    TEST_LOG("Testing onDisplayConnectionChanged notification via HDMI hotplug");
    
    if (CreateHdcpProfileInterfaceObjectUsingComRPCConnection() != Core::ERROR_NONE) {
        FAIL() << "Failed to create HdcpProfile COM-RPC interface";
    }
    
    ASSERT_NE(m_controller_HdcpProfile, nullptr);
    ASSERT_NE(m_HdcpProfileplugin, nullptr);
    
    // Register for notifications
    uint32_t result = m_HdcpProfileplugin->Register(&notify);
    EXPECT_EQ(result, Core::ERROR_NONE);
    TEST_LOG("Successfully registered for notifications");
    
    // Reset event flag
    notify.ResetEvent();
    
    // Trigger HDMI hotplug event via HAL callback
    if (m_dsHdmiHotPlugCallback != nullptr) {
        TEST_LOG("Triggering HDMI hotplug event (CONNECTED)");
        m_dsHdmiHotPlugCallback(dsHDMI_HOTPLUG_CONNECTED, nullptr);
        
        // Wait for notification
        uint32_t eventStatus = notify.WaitForRequestStatus(EVNT_TIMEOUT, HdcpProfile_OnDisplayConnectionChanged);
        
        EXPECT_NE(eventStatus, HdcpProfile_StateInvalid);
        if (eventStatus != HdcpProfile_StateInvalid) {
            TEST_LOG("onDisplayConnectionChanged notification received successfully");
            
            IHdcpProfile::HDCPStatus receivedStatus = notify.GetLastHdcpStatus();
            EXPECT_TRUE(receivedStatus.isConnected);
            EXPECT_FALSE(receivedStatus.supportedHDCPVersion.empty());
        } else {
            TEST_LOG("Timeout waiting for onDisplayConnectionChanged notification");
        }
    } else {
        TEST_LOG("WARNING: dsHdmiHotPlugCallback not captured, skipping event trigger");
    }
    
    // Unregister
    result = m_HdcpProfileplugin->Unregister(&notify);
    EXPECT_EQ(result, Core::ERROR_NONE);
    
    m_HdcpProfileplugin->Release();
    m_controller_HdcpProfile->Release();
}

/**
 * @brief Test onDisplayConnectionChanged notification via HDCP status change event
 */
TEST_F(HdcpProfile_L2test, OnDisplayConnectionChanged_HdcpStatusChange_COMRPC)
{
    TEST_LOG("Testing onDisplayConnectionChanged notification via HDCP status change");
    
    if (CreateHdcpProfileInterfaceObjectUsingComRPCConnection() != Core::ERROR_NONE) {
        FAIL() << "Failed to create HdcpProfile COM-RPC interface";
    }
    
    ASSERT_NE(m_controller_HdcpProfile, nullptr);
    ASSERT_NE(m_HdcpProfileplugin, nullptr);
    
    // Register for notifications
    uint32_t result = m_HdcpProfileplugin->Register(&notify);
    EXPECT_EQ(result, Core::ERROR_NONE);
    TEST_LOG("Successfully registered for notifications");
    
    // Reset event flag
    notify.ResetEvent();
    
    // Trigger HDCP status change event via HAL callback
    if (m_dsHdcpStatusCallback != nullptr) {
        TEST_LOG("Triggering HDCP status change event (AUTHENTICATED)");
        m_dsHdcpStatusCallback(1, dsHDCP_STATUS_AUTHENTICATED, nullptr);
        
        // Wait for notification
        uint32_t eventStatus = notify.WaitForRequestStatus(EVNT_TIMEOUT, HdcpProfile_OnDisplayConnectionChanged);
        
        EXPECT_NE(eventStatus, HdcpProfile_StateInvalid);
        if (eventStatus != HdcpProfile_StateInvalid) {
            TEST_LOG("onDisplayConnectionChanged notification received successfully");
            
            IHdcpProfile::HDCPStatus receivedStatus = notify.GetLastHdcpStatus();
            EXPECT_TRUE(receivedStatus.isConnected);
            EXPECT_TRUE(receivedStatus.isHDCPCompliant);
            EXPECT_FALSE(receivedStatus.supportedHDCPVersion.empty());
        } else {
            TEST_LOG("Timeout waiting for onDisplayConnectionChanged notification");
        }
    } else {
        TEST_LOG("WARNING: dsHdcpStatusCallback not captured, skipping event trigger");
    }
    
    // Unregister
    result = m_HdcpProfileplugin->Unregister(&notify);
    EXPECT_EQ(result, Core::ERROR_NONE);
    
    m_HdcpProfileplugin->Release();
    m_controller_HdcpProfile->Release();
}
