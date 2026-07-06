/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#include <string>

#include "HdcpProfileImplementation.h"

#include "UtilsJsonRpc.h"

#define HDMI_HOT_PLUG_EVENT_CONNECTED    0
#define HDMI_HOT_PLUG_EVENT_DISCONNECTED 1

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 9

using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(HdcpProfileImplementation, 1, 0);
        HdcpProfileImplementation *HdcpProfileImplementation::_instance = nullptr;

        PowerManagerInterfaceRef HdcpProfileImplementation::_powerManagerPlugin;

        HdcpProfileImplementation::HdcpProfileImplementation()
        : _DSVideoPortNotification(*this)
        , _DSDisplayHotPlugNotification(*this)
        , _adminLock()
        , mShell(nullptr)
        , _service(nullptr)
        {
            LOGINFO("Create HdcpProfileImplementation Instance");
            HdcpProfileImplementation::_instance = this;
        }

        HdcpProfileImplementation::~HdcpProfileImplementation()
        {
            LOGINFO("Call HdcpProfileImplementation destructor\n");
            // COM-RPC: notifications are unregistered in OnDeviceSettingsDeactivated()
            // which is called by DeviceSettingsClientHelper::Close()
            if (_powerManagerPlugin) {
               _powerManagerPlugin.Reset();
            }
            if(_service != nullptr)
            {
               _service->Release();
            }
            HdcpProfileImplementation::_instance = nullptr;
            mShell = nullptr;
        }

        void HdcpProfileImplementation::InitializePowerManager(PluginHost::IShell *service)
        {
            _powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
                                    .withIShell(service)
                                    .withRetryIntervalMS(200)
                                    .withRetryCount(25)
                                    .createInterface();
        }

        // =========================================================================
        // DeviceSettingsClientHelper override: called when DeviceSettings activates
        // DS_IARM equivalent:
        //   device::Host::getInstance().Register(IVideoOutputPortEvents, "WPE::HdcpProfile")
        //   device::Host::getInstance().Register(IDisplayDeviceEvents, "WPE::HdcpProfile")
        // =========================================================================
        void HdcpProfileImplementation::OnDeviceSettingsActivated()
        {
            LOGINFO("HdcpProfileImplementation: OnDeviceSettingsActivated — registering DS notifications");

            // Get video port handles via config store (mirrors displaysettings pattern)
            // COM-RPC: device::VideoOutputPortConfig::getInstance().getPort("HDMI0")
            //       → LoadVideoPortConfig + BuildVideoPortEntries + GetVideoPort per entry
            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    // Load port config once into cached member store (1-arg member fn)
                    LoadVideoPortConfig(_vpConfigStore);

                    _videoPortHandles.clear();
                    std::vector<VideoPortEntry> entries;
                    if (_vpConfigStore.BuildVideoPortEntries(entries)) {
                        for (const VideoPortEntry& e : entries) {
                            int32_t handle = -1;
                            Core::hresult rc = vp->GetVideoPort(e.type, e.index, handle);
                            if (rc == Core::ERROR_NONE) {
                                _videoPortHandles[e.name] = handle;
                                LOGINFO("VideoPort '%s' → handle=%d", e.name.c_str(), handle);
                            }
                        }
                    }
                    // Register for HDCP status change events
                    // COM-RPC: device::Host::Register(IVideoOutputPortEvents) → vp->Register(INotification)
                    vp->Register(&_DSVideoPortNotification);
                    vp->Release();
                    LOGINFO("HdcpProfileImplementation: IDeviceSettingsVideoPort::INotification registered");
                }
            }

            // Register for HDMI hotplug events
            // COM-RPC: device::Host::Register(IDisplayDeviceEvents) → display->Register(IDisplayHDMIHotPlugNotification)
            {
                auto* display = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
                if (display != nullptr) {
                    display->Register(&_DSDisplayHotPlugNotification);
                    display->Release();
                    LOGINFO("HdcpProfileImplementation: IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification registered");
                }
            }
        }

        // =========================================================================
        // DeviceSettingsClientHelper override: called when DeviceSettings deactivates
        // DS_IARM equivalent:
        //   device::Host::getInstance().UnRegister(IVideoOutputPortEvents)
        //   device::Host::getInstance().UnRegister(IDisplayDeviceEvents)
        // =========================================================================
        void HdcpProfileImplementation::OnDeviceSettingsDeactivated()
        {
            LOGINFO("HdcpProfileImplementation: OnDeviceSettingsDeactivated — unregistering DS notifications");

            {
                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp != nullptr) {
                    vp->Unregister(&_DSVideoPortNotification);
                    vp->Release();
                }
            }
            {
                auto* display = AcquireSubInterface<Exchange::IDeviceSettingsDisplay>();
                if (display != nullptr) {
                    display->Unregister(&_DSDisplayHotPlugNotification);
                    display->Release();
                }
            }

            _videoPortHandles.clear();
            _vpConfigStore.Clear();
        }

        void HdcpProfileImplementation::onHdmiOutputHotPlug(int connectStatus)
        {
            if (HDMI_HOT_PLUG_EVENT_CONNECTED == connectStatus)
            {
                LOGINFO("HDMI_HOT_PLUG Status[%d]",connectStatus);
            }
            onHdcpProfileDisplayConnectionChanged();
        }

        void HdcpProfileImplementation::onHdcpProfileDisplayConnectionChanged()
        {
            HDCPStatus hdcpstatus;
            if (true == GetHDCPStatusInternal(hdcpstatus))
            {
               dispatchEvent(HDCPPROFILE_EVENT_DISPLAYCONNECTIONCHANGED, hdcpstatus);
               logHdcpStatus("onHdcpProfileDisplayConnectionChanged", hdcpstatus);
            }
            else
            {
               LOGERR("Failed to getHdcpStatus");
            }
        }

        void HdcpProfileImplementation::logHdcpStatus(const char *trigger, HDCPStatus& status)
        {
            LOGWARN("[%s]-HDCPStatus::isConnected: %s", trigger, status.isConnected ? "true" : "false");
            LOGWARN("[%s]-HDCPStatus::isHDCPEnabled: %s", trigger, status.isHDCPEnabled ? "true" : "false");
            LOGWARN("[%s]-HDCPStatus::isHDCPCompliant: %s", trigger, status.isHDCPCompliant ? "true" : "false");
            LOGWARN("[%s]-HDCPStatus::supportedHDCPVersion: %s", trigger, status.supportedHDCPVersion.c_str());
            LOGWARN("[%s]-HDCPStatus::receiverHDCPVersion: %s", trigger, status.receiverHDCPVersion.c_str());
            LOGWARN("[%s]-HDCPStatus::currentHDCPVersion: %s", trigger, status.currentHDCPVersion.c_str());
            LOGWARN("[%s]-HDCPStatus::hdcpReason: %d", trigger, status.hdcpReason);
            LOGWARN("[%s]-HDCPStatus Response: %s, %s, %s, %s, %s, %s, %d", trigger,
                    status.isConnected ? "true" : "false",
                    status.isHDCPEnabled ? "true" : "false",
                    status.isHDCPCompliant ? "true" : "false",
                    status.supportedHDCPVersion.c_str(),
                    status.receiverHDCPVersion.c_str(),
                    status.currentHDCPVersion.c_str(),
                    status.hdcpReason);
        }

        void HdcpProfileImplementation::onHdmiOutputHDCPStatusEvent(int hdcpStatus)
        {
            LOGINFO("hdcpStatus[%d]",hdcpStatus);
            onHdcpProfileDisplayConnectionChanged();
        }

        // DS_IARM equivalent: HdcpProfileImplementation::OnHDCPStatusChange(dsHdcpStatus_t)
        // Preserves the power state check from the DS_IARM implementation.
        void HdcpProfileImplementation::onHdcpStatusChangeNotification(int hdcpStatus)
        {
            uint32_t res = Core::ERROR_GENERAL;
            PowerState pwrStateCur = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
            PowerState pwrStatePrev = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;

            HdcpProfileImplementation* instance = HdcpProfileImplementation::_instance;

            ASSERT (_powerManagerPlugin);
            if (_powerManagerPlugin && instance){
                res = _powerManagerPlugin->GetPowerState(pwrStateCur, pwrStatePrev);
                if (Core::ERROR_NONE != res)
                {
                    LOGWARN("Failed to Invoke RPC method: GetPowerState");
                }
                LOGINFO("Received OnHDCPStatusChange  event data:%d  param.curState: %d \r\n", hdcpStatus, pwrStateCur);
                instance->onHdmiOutputHDCPStatusEvent(hdcpStatus);
            }
        }

        /**
         * Register a notification callback
         */
        Core::hresult HdcpProfileImplementation::Register(Exchange::IHdcpProfile::INotification *notification)
        {
            ASSERT(nullptr != notification);

            _adminLock.Lock();
            printf("HdcpProfileImplementation::Register: notification = %p", notification);
            LOGINFO("Register notification");

            // Make sure we can't register the same notification callback multiple times
            if (std::find(_hdcpProfileNotification.begin(), _hdcpProfileNotification.end(), notification) == _hdcpProfileNotification.end())
            {
                _hdcpProfileNotification.push_back(notification);
                notification->AddRef();
            }
            else
            {
                LOGERR("same notification is registered already");
            }

           _adminLock.Unlock();

            return Core::ERROR_NONE;
        }

        /**
         * Unregister a notification callback
         */
        Core::hresult HdcpProfileImplementation::Unregister(Exchange::IHdcpProfile::INotification *notification)
        {
            Core::hresult status = Core::ERROR_GENERAL;

            ASSERT(nullptr != notification);

            _adminLock.Lock();

            // we just unregister one notification once
            auto itr = std::find(_hdcpProfileNotification.begin(), _hdcpProfileNotification.end(), notification);
            if (itr != _hdcpProfileNotification.end())
            {
                (*itr)->Release();
                LOGINFO("Unregister notification");
                _hdcpProfileNotification.erase(itr);
                status = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("notification not found");
            }

            _adminLock.Unlock();

            return status;
        }

        uint32_t HdcpProfileImplementation::Configure(PluginHost::IShell* service)
        {
            uint32_t result = Core::ERROR_NONE;
            _service = service;
            _service->AddRef();
            ASSERT(service != nullptr);
            // COM-RPC: open the DeviceSettings plugin link.
            // DS_IARM equivalent:
            //   device::Host::getInstance().Register(IVideoOutputPortEvents, "WPE::HdcpProfile")
            //   device::Host::getInstance().Register(IDisplayDeviceEvents, "WPE::HdcpProfile")
            // OnDeviceSettingsActivated() fires once DeviceSettings is ready,
            // which registers the VideoPort and Display notification delegates.
            DeviceSettingsClientHelper::Open(service);
            LOGINFO("HdcpProfileImplementation: DeviceSettingsClientHelper::Open() called");
            InitializePowerManager(service);
            return result;
        }

        void HdcpProfileImplementation::dispatchEvent(Event event, const HDCPStatus &hdcpstatus)
        {
            Core::IWorkerPool::Instance().Submit(DispatchJob::Create(this, event, hdcpstatus));
        }

        void HdcpProfileImplementation::Dispatch(Event event, const HDCPStatus& hdcpstatus)
        {
            _adminLock.Lock();

            std::list<Exchange::IHdcpProfile::INotification *>::const_iterator index(_hdcpProfileNotification.begin());

            switch (event)
            {
                case HDCPPROFILE_EVENT_DISPLAYCONNECTIONCHANGED:
                {
                    while (index != _hdcpProfileNotification.end())
                    {
                        (*index)->OnDisplayConnectionChanged(hdcpstatus);
                        ++index;
                    }
                }
                break;

            default:
                LOGWARN("Event[%u] not handled", event);
                break;
            }
            _adminLock.Unlock();
        }

        bool HdcpProfileImplementation::GetHDCPStatusInternal(HDCPStatus& hdcpstatus)
        {
            bool isConnected     = false;
            bool isHDCPCompliant = false;
            bool isHDCPEnabled   = true;
            // COM-RPC: dsHDCP_STATUS_UNPOWERED = DS_HDCP_STATUS_UNPOWERED = 0
            int eHDCPEnabledStatus = static_cast<int>(Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_UNPOWERED);
            // COM-RPC: dsHDCP_VERSION_MAX = DS_HDCP_VERSION_MAX = 2
            Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion hdcpProtocol         = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_MAX;
            Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion hdcpReceiverProtocol = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_MAX;
            Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion hdcpCurrentProtocol  = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_MAX;

            try
            {
                // COM-RPC: device::VideoOutputPortConfig::getInstance().getPort("HDMI0")
                //       → use _videoPortHandles (populated in OnDeviceSettingsActivated)
                const int32_t videoHandle = getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName());
                if (videoHandle < 0) {
                    LOGERR("GetHDCPStatusInternal: video port handle not available");
                    return false;
                }

                auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                if (vp == nullptr) {
                    LOGERR("GetHDCPStatusInternal: IDeviceSettingsVideoPort not available");
                    return false;
                }

                // COM-RPC: vPort.isDisplayConnected()
                //       → IDeviceSettingsVideoPort::IsVideoPortDisplayConnected(handle, connected)
                vp->IsVideoPortDisplayConnected(videoHandle, isConnected);

                // COM-RPC: (dsHdcpProtocolVersion_t)vPort.getHDCPProtocol()
                //       → IDeviceSettingsVideoPort::GetHDCPProtocolVersionOnVideoPort(handle, hdcpVersion)
                vp->GetHDCPProtocolVersionOnVideoPort(videoHandle, hdcpProtocol);

                // COM-RPC: vPort.getHDCPStatus()
                //       → IDeviceSettingsVideoPort::GetHDCPStatusOnVideoPort(handle, hdcpStatus)
                Exchange::IDeviceSettingsVideoPort::HDCPStatus vpHdcpStatus = Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_UNPOWERED;
                vp->GetHDCPStatusOnVideoPort(videoHandle, vpHdcpStatus);
                eHDCPEnabledStatus = static_cast<int>(vpHdcpStatus);

                if(isConnected)
                {
                    // COM-RPC: isHDCPCompliant = (eHDCPEnabledStatus == dsHDCP_STATUS_AUTHENTICATED)
                    //       → DS_HDCP_STATUS_AUTHENTICATED = dsHDCP_STATUS_AUTHENTICATED = 2
                    isHDCPCompliant = (vpHdcpStatus == Exchange::IDeviceSettingsVideoPort::DS_HDCP_STATUS_AUTHENTICATED);

                    // COM-RPC: vPort.isContentProtected()
                    //       → IDeviceSettingsVideoPort::IsHDCPEnabledOnVideoPort(handle, hdcpEnabled)
                    vp->IsHDCPEnabledOnVideoPort(videoHandle, isHDCPEnabled);

                    // COM-RPC: (dsHdcpProtocolVersion_t)vPort.getHDCPReceiverProtocol()
                    //       → IDeviceSettingsVideoPort::GetHDCPReceiverProtocolVersionOnVideoPort(handle, hdcpVersion)
                    vp->GetHDCPReceiverProtocolVersionOnVideoPort(videoHandle, hdcpReceiverProtocol);

                    // COM-RPC: (dsHdcpProtocolVersion_t)vPort.getHDCPCurrentProtocol()
                    //       → IDeviceSettingsVideoPort::GetHDCPCurrentProtocolVersionOnVideoPort(handle, hdcpVersion)
                    vp->GetHDCPCurrentProtocolVersionOnVideoPort(videoHandle, hdcpCurrentProtocol);
                }
                else
                {
                    isHDCPCompliant = false;
                    isHDCPEnabled = false;
                }

                vp->Release();
            }
            catch (const std::exception& e)
            {
                LOGERR("DS exception [%s] caught\r\n", e.what());
                return false;
            }
            catch (...) {
               LOGERR("Failed to getHdcpStatus with unknown exception\n");
               return false;
           }

            hdcpstatus.isConnected = isConnected;
            hdcpstatus.isHDCPCompliant = isHDCPCompliant;
            hdcpstatus.isHDCPEnabled = isHDCPEnabled;
            hdcpstatus.hdcpReason = eHDCPEnabledStatus;

            // COM-RPC: dsHDCP_VERSION_2X = DS_HDCP_VERSION_2X = 1
            if(hdcpProtocol == Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X)
            {
                hdcpstatus.supportedHDCPVersion = "2.2";
            }
            else
            {
                hdcpstatus.supportedHDCPVersion = "1.4";
            }

            if(hdcpReceiverProtocol == Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X)
            {
                hdcpstatus.receiverHDCPVersion = "2.2";
            }
            else
            {
                hdcpstatus.receiverHDCPVersion = "1.4";
            }

            if(hdcpCurrentProtocol == Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X)
            {
                hdcpstatus.currentHDCPVersion = "2.2";
            }
            else
            {
                hdcpstatus.currentHDCPVersion = "1.4";
            }
            return true;
        }

        Core::hresult HdcpProfileImplementation::GetHDCPStatus(HDCPStatus& hdcpstatus, bool& success)
        {
            success = GetHDCPStatusInternal(hdcpstatus);
            return Core::ERROR_NONE;
        }

        Core::hresult HdcpProfileImplementation::GetSettopHDCPSupport(string& supportedHDCPVersion, bool& isHDCPSupported, bool& success)
        {
            // COM-RPC: dsHDCP_VERSION_MAX = DS_HDCP_VERSION_MAX = 2
            Exchange::IDeviceSettingsVideoPort::HDCPProtocolVersion hdcpProtocol = Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_MAX;

            try
            {
                // COM-RPC: (dsHdcpProtocolVersion_t)vPort.getHDCPProtocol()
                //       → use _videoPortHandles (populated in OnDeviceSettingsActivated)
                const int32_t videoHandle = getCachedVideoPortHandle(_vpConfigStore.GetDefaultVideoPortName());
                if (videoHandle >= 0) {
                    auto* vp = AcquireSubInterface<Exchange::IDeviceSettingsVideoPort>();
                    if (vp != nullptr) {
                        vp->GetHDCPProtocolVersionOnVideoPort(videoHandle, hdcpProtocol);
                        vp->Release();
                    }
                }
            }
            catch (const std::exception& e)
            {
                LOGWARN("DS exception caught from %s\r\n", __FUNCTION__);
            }

            if(hdcpProtocol == Exchange::IDeviceSettingsVideoPort::DS_HDCP_VERSION_2X)
            {
                supportedHDCPVersion = "2.2";
                LOGWARN("supportedHDCPVersion :2.2");
            }
            else
            {
                supportedHDCPVersion = "1.4";
                LOGWARN("supportedHDCPVersion :1.4");
            }

            isHDCPSupported = true;

            success = true;
            return Core::ERROR_NONE;
        }

    } // namespace Plugin
} // namespace WPEFramework
