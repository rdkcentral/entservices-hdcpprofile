# HdcpProfile Plugin Architecture

## Overview

The HdcpProfile plugin is a WPEFramework (Thunder) service plugin that provides HDCP (High-bandwidth Digital Content Protection) status monitoring and management capabilities for RDK devices. The plugin integrates with the Device Settings (DS) HAL to retrieve HDCP protocol information and monitor display connection changes.

## System Architecture

### Component Hierarchy

```
┌─────────────────────────────────────────────────────┐
│           WPEFramework Plugin Host                  │
│              (Thunder R4.x)                         │
└──────────────────┬──────────────────────────────────┘
                   │
         ┌─────────▼──────────┐
         │  HdcpProfile Plugin │
         │   (JSON-RPC API)    │
         └─────────┬───────────┘
                   │
    ┌──────────────┼──────────────┐
    │              │              │
┌───▼───┐   ┌──────▼─────┐  ┌────▼────────┐
│ Plugin│   │Implementation│ │ Power Mgr  │
│ Proxy │   │   Service    │ │ Interface  │
└───┬───┘   └──────┬───────┘ └────┬────────┘
    │              │              │
    └──────────────┼──────────────┘
                   │
         ┌─────────▼──────────┐
         │  Device Settings   │
         │    HAL Layer       │
         │  (Video Output)    │
         └────────────────────┘
```

## Core Components

### 1. HdcpProfile (Plugin Proxy)

**Location:** `plugin/HdcpProfile.cpp`

The plugin proxy manages the lifecycle and communication bridge between the WPEFramework and the implementation service. Key responsibilities:

- **Initialization:** Creates and configures the out-of-process implementation service
- **Registration:** Registers JSON-RPC interfaces for external communication
- **Notification Management:** Bridges notifications between implementation and clients
- **Lifecycle Management:** Handles plugin activation, deactivation, and cleanup

**Key Methods:**
- `Initialize()`: Sets up plugin, creates implementation instance, registers JSON-RPC interface
- `Deinitialize()`: Cleans up resources and terminates implementation process
- `Deactivated()`: Handles out-of-process implementation crashes/terminations

### 2. HdcpProfileImplementation (Service Implementation)

**Location:** `plugin/HdcpProfileImplementation.cpp`

The implementation service provides core HDCP functionality. Key responsibilities:

- **HDCP Status Retrieval:** Queries Device Settings HAL for current HDCP state
- **Event Handling:** Receives and processes HDMI hotplug and HDCP status change events
- **Notification Dispatch:** Notifies registered clients of display connection changes
- **Power State Awareness:** Integrates with PowerManager to handle power state transitions

**Key Methods:**
- `GetHDCPStatus()`: Returns current HDCP protocol version, compliance status, and connection state
- `GetSettopHDCPSupport()`: Returns maximum supported HDCP version of the device
- `OnDisplayHDMIHotPlug()`: Handles HDMI cable connection/disconnection events
- `OnHDCPStatusChange()`: Handles HDCP authentication status changes

### 3. Device Settings Integration

The plugin interfaces with the Device Settings (DS) HAL through:

- **VideoOutputPort API:** Accesses HDCP protocol versions and authentication status
- **VideoOutputPortConfig:** Retrieves default video port configuration
- **Host Events:** Registers for display device and video output port events

**DS Event Handlers:**
- `IVideoOutputPortEvents`: Monitors HDCP status changes
- `IDisplayDeviceEvents`: Monitors HDMI hotplug events

## Data Flow

### HDCP Status Query Flow

```
Client Request
     │
     ▼
JSON-RPC Interface
     │
     ▼
HdcpProfile Plugin
     │
     ▼
Implementation Service
     │
     ▼
Device Settings HAL
     │
     ▼
VideoOutputPort
     │
     ▼
HDCP Hardware/Driver
```

### Event Notification Flow

```
HDMI Hardware Event
     │
     ▼
Device Settings HAL
     │
     ▼
DS Event Callback
     │
     ▼
Implementation Service
     │
     ▼
Event Dispatcher (WorkerPool)
     │
     ▼
Registered Notification Callbacks
     │
     ▼
Client Applications
```

## Key Interfaces

### Exchange Interfaces

**IHdcpProfile** (`interfaces/IHdcpProfile.h`):
- Primary interface exposed to clients
- Methods: `GetHDCPStatus()`, `GetSettopHDCPSupport()`
- Notification interface for display connection changes

**IConfiguration** (`interfaces/IConfiguration.h`):
- Plugin configuration interface
- Method: `Configure()` - Initializes DS HAL integration

### JSON-RPC API

Automatically generated from interface definitions:
- `JHdcpProfile`: JSON-RPC proxy implementation
- `JsonData_HdcpProfile`: JSON data structures for RPC

## Threading Model

- **Main Thread:** Plugin initialization and RPC request handling
- **Worker Pool:** Asynchronous event notification dispatch
- **DS Event Threads:** Device Settings HAL event callbacks
- **Thread Safety:** Mutex-protected notification list (`_adminLock`)

## Dependencies

### Build Dependencies
- **WPEFramework:** Core Thunder framework (R4.x)
- **Device Settings (DS):** HAL library for HDCP/display operations
- **CEC (optional):** HDMI-CEC protocol support

### Runtime Dependencies
- **PowerManager Plugin:** Power state monitoring
- **Device Settings Service:** HDMI/HDCP hardware abstraction

## Configuration

The plugin supports configuration through:
- **Static Configuration:** CMake build options (`PLUGIN_HDCPPROFILE_AUTOSTART`, `PLUGIN_HDCPPROFILE_STARTUPORDER`)
- **Runtime Configuration:** WPEFramework plugin configuration JSON

## Error Handling

- **Exception Safety:** All DS HAL calls wrapped in try-catch blocks
- **Return Status:** Uses WPEFramework Core::hresult error codes
- **Logging:** Comprehensive logging via `UtilsLogging.h` macros (LOGINFO, LOGWARN, LOGERR)
- **Resource Cleanup:** RAII patterns and explicit cleanup in Deinitialize()

## Design Patterns

1. **Proxy Pattern:** Plugin acts as proxy to out-of-process implementation
2. **Observer Pattern:** Notification system for display connection events
3. **Singleton Pattern:** HdcpProfileImplementation maintains static instance pointer
4. **Job Pattern:** Asynchronous event dispatch via WorkerPool jobs

## Security Considerations

- **Process Isolation:** Implementation runs in separate process for stability
- **Access Control:** Relies on WPEFramework's security token mechanism
- **HDCP Compliance:** Enforces HDCP content protection requirements

---

*Document Version: 1.0*  
*Last Updated: February 2026*
