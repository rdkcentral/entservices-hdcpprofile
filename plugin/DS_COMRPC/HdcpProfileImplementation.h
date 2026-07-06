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

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/IHdcpProfile.h>
#include <interfaces/IPowerManager.h>
#include <interfaces/IConfiguration.h>

#include <com/com.h>
#include <core/core.h>
#include <mutex>
#include <vector>

#include "PowerManagerInterface.h"
#include "DeviceSettingsInterface.h"

namespace WPEFramework
{
    namespace Plugin
    {

        class HdcpProfileImplementation : public Exchange::IHdcpProfile,
                                          public Exchange::IConfiguration,
                                          public DeviceSettingsClientHelper
        {
        public:
            // We do not allow this plugin to be copied !!
            HdcpProfileImplementation();
            ~HdcpProfileImplementation() override;

            static HdcpProfileImplementation *instance(HdcpProfileImplementation *HdcpProfileImpl = nullptr);

            // We do not allow this plugin to be copied !!
            HdcpProfileImplementation(const HdcpProfileImplementation &) = delete;
            HdcpProfileImplementation &operator=(const HdcpProfileImplementation &) = delete;

            BEGIN_INTERFACE_MAP(HdcpProfileImplementation)
            INTERFACE_ENTRY(Exchange::IHdcpProfile)
            INTERFACE_ENTRY(Exchange::IConfiguration)
            END_INTERFACE_MAP

        public:
            enum Event
            {
                HDCPPROFILE_EVENT_DISPLAYCONNECTIONCHANGED
            };

            class EXTERNAL DispatchJob : public Core::IDispatch
            {
            protected:
                DispatchJob(HdcpProfileImplementation *HdcpProfileImplementation, Event event, HDCPStatus &params)
                    : _hdcpProfileImplementation(HdcpProfileImplementation), _event(event), _params(params)
                {
                    if (_hdcpProfileImplementation != nullptr)
                    {
                        _hdcpProfileImplementation->AddRef();
                    }
                }

            public:
                DispatchJob() = delete;
                DispatchJob(const DispatchJob &) = delete;
                DispatchJob &operator=(const DispatchJob &) = delete;
                ~DispatchJob()
                {
                    if (_hdcpProfileImplementation != nullptr)
                    {
                        _hdcpProfileImplementation->Release();
                    }
                }

            public:
                static Core::ProxyType<Core::IDispatch> Create(HdcpProfileImplementation *hdcpProfileImplementation, Event event, HDCPStatus params)
                {
#ifndef USE_THUNDER_R4
                    return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<DispatchJob>::Create(hdcpProfileImplementation, event, params)));
#else
                    return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<DispatchJob>::Create(hdcpProfileImplementation, event, params)));
#endif
                }
                virtual void Dispatch()
                {
                    _hdcpProfileImplementation->Dispatch(_event, _params);
                }

            private:
                HdcpProfileImplementation *_hdcpProfileImplementation;
                const Event _event;
                HDCPStatus _params;
            };

        public:
            template <typename T>
            T* baseInterface()
            {
                static_assert(std::is_base_of<T, HdcpProfileImplementation>(), "base type mismatch");
                return static_cast<T*>(this);
            }

            // =========================================================================
            // DeviceSettingsClientHelper overrides
            // Called when entservices-devicesettings plugin activates/deactivates.
            // DS_IARM equivalent: device::Host::Register/UnRegister(IVideoOutputPortEvents, IDisplayDeviceEvents)
            // =========================================================================
            void OnDeviceSettingsActivated() override;
            void OnDeviceSettingsDeactivated() override;

            Core::hresult Register(Exchange::IHdcpProfile::INotification *notification) override;
            Core::hresult Unregister(Exchange::IHdcpProfile::INotification *notification) override;

            Core::hresult GetHDCPStatus(HDCPStatus& hdcpstatus, bool& success) override;
            Core::hresult GetSettopHDCPSupport(string& supportedHDCPVersion, bool& isHDCPSupported, bool& success) override;
            bool GetHDCPStatusInternal(HDCPStatus& hdcpstatus);
            void InitializePowerManager(PluginHost::IShell *service);
            void onHdmiOutputHotPlug(int connectStatus);
            void onHdmiOutputHDCPStatusEvent(int hdcpStatus);
            void onHdcpStatusChangeNotification(int hdcpStatus);  // mirrors DS_IARM OnHDCPStatusChange power-state check
            void logHdcpStatus(const char *trigger, HDCPStatus& status);
            void onHdcpProfileDisplayConnectionChanged();
            static PowerManagerInterfaceRef _powerManagerPlugin;
            uint32_t Configure(PluginHost::IShell* service) override;

        private:
            // =========================================================================
            // COM-RPC notification delegate: IDeviceSettingsVideoPort::INotification
            // DS_IARM equivalent: device::Host::IVideoOutputPortEvents::OnHDCPStatusChange
            // =========================================================================
            class DSVideoPortNotification
                : public Exchange::IDeviceSettingsVideoPort::INotification {
            public:
                explicit DSVideoPortNotification(HdcpProfileImplementation& parent) : _parent(parent) {}
                DSVideoPortNotification(const DSVideoPortNotification&) = delete;
                DSVideoPortNotification& operator=(const DSVideoPortNotification&) = delete;

                void OnHDCPStatusChange(const Exchange::IDeviceSettingsVideoPort::HDCPStatus hdcpStatus) override {
                    // COM-RPC: maps to DS_IARM OnHDCPStatusChange(dsHdcpStatus_t)
                    // Preserves the power state check via onHdcpStatusChangeNotification()
                    _parent.onHdcpStatusChangeNotification(static_cast<int>(hdcpStatus));
                }
                void OnResolutionPostChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange&) override {}
                void OnResolutionPreChange(const Exchange::IDeviceSettingsVideoPort::ResolutionChange&) override {}
                void OnVideoFormatUpdate(const Exchange::IDeviceSettingsVideoPort::HDRStandard) override {}

                BEGIN_INTERFACE_MAP(DSVideoPortNotification)
                    INTERFACE_ENTRY(Exchange::IDeviceSettingsVideoPort::INotification)
                END_INTERFACE_MAP
            private:
                HdcpProfileImplementation& _parent;
            };

            // =========================================================================
            // COM-RPC notification delegate: IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification
            // DS_IARM equivalent: device::Host::IDisplayDeviceEvents::OnDisplayHDMIHotPlug
            // =========================================================================
            class DSDisplayHotPlugNotification
                : public Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification {
            public:
                explicit DSDisplayHotPlugNotification(HdcpProfileImplementation& parent) : _parent(parent) {}
                DSDisplayHotPlugNotification(const DSDisplayHotPlugNotification&) = delete;
                DSDisplayHotPlugNotification& operator=(const DSDisplayHotPlugNotification&) = delete;

                void OnDisplayHDMIHotPlug(const Exchange::IDeviceSettingsDisplay::DisplayEvent displayEvent) override {
                    // COM-RPC: DS_DISPLAY_EVENT_CONNECTED=0, DS_DISPLAY_EVENT_DISCONNECTED=1
                    // matches DS_IARM: dsDISPLAY_EVENT_CONNECTED=0, dsDISPLAY_EVENT_DISCONNECTED=1
                    _parent.onHdmiOutputHotPlug(static_cast<int>(displayEvent));
                }

                BEGIN_INTERFACE_MAP(DSDisplayHotPlugNotification)
                    INTERFACE_ENTRY(Exchange::IDeviceSettingsDisplay::IDisplayHDMIHotPlugNotification)
                END_INTERFACE_MAP
            private:
                HdcpProfileImplementation& _parent;
            };

            // Notification delegate instances
            Core::Sink<DSVideoPortNotification>    _DSVideoPortNotification;
            Core::Sink<DSDisplayHotPlugNotification> _DSDisplayHotPlugNotification;

            // Cached HDMI0 video port handle (set in OnDeviceSettingsActivated)
            int32_t m_videoPortHandle { -1 };

            mutable Core::CriticalSection _adminLock;
            PluginHost::IShell *mShell;
            std::list<Exchange::IHdcpProfile::INotification *> _hdcpProfileNotification;
            PluginHost::IShell* _service;
            void dispatchEvent(Event, const HDCPStatus &params);
            void Dispatch(Event event, const HDCPStatus &params);

        public:
            static HdcpProfileImplementation *_instance;
        };

    } // namespace Plugin
} // namespace WPEFramework
