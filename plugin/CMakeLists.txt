# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(PLUGIN_NAME HdcpProfile)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_HDCPPROFILE_AUTOSTART "false" CACHE STRING "Automatically start HdcpProfile plugin")
set(PLUGIN_HDCPPROFILE_STARTUPORDER "" CACHE STRING "To configure startup order of HdcpProfile plugin")

# Build selection: DS_COMRPC (COM-RPC via entservices-devicesettings) or DS_IARM (libds/IARM)
option(DS_COMRPC "Build HdcpProfile using the DeviceSettings COM-RPC plugin instead of libds/IARM" OFF)
if (DS_COMRPC)
    set(DS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/DS_COMRPC)
    message(STATUS "HdcpProfile: building with DS_COMRPC (COM-RPC)")
else()
    set(DS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/DS_IARM)
    message(STATUS "HdcpProfile: building with DS_IARM (libds)")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Helpers REQUIRED)
if (USE_THUNDER_R4)
    find_package(${NAMESPACE}COM REQUIRED)
else ()
    find_package(${NAMESPACE}Protocols REQUIRED)
endif (USE_THUNDER_R4)

# --- Outer plugin (HdcpProfile) ---
add_library(${MODULE_NAME} SHARED
        HdcpProfile.cpp
        Module.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

target_link_libraries(${MODULE_NAME}
        PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}::${NAMESPACE}Helpers)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

# --- Implementation library (DS_IARM or DS_COMRPC) ---
add_library(${PLUGIN_IMPLEMENTATION} SHARED
        ${DS_SOURCE_DIR}/HdcpProfileImplementation.cpp
        Module.cpp)

target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE
        ${DS_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR})

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_link_libraries(${PLUGIN_IMPLEMENTATION}
        PRIVATE
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins)

if (USE_THUNDER_R4)
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}COM::${NAMESPACE}COM)
else ()
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Protocols::${NAMESPACE}Protocols)
endif (USE_THUNDER_R4)

if (DS_COMRPC)
    # COM-RPC: link against WPEFrameworkHelpers (provides DeviceSettingsClientHelper)
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PUBLIC
            ${NAMESPACE}Plugins::${NAMESPACE}Plugins
            ${NAMESPACE}::${NAMESPACE}Helpers)
else()
    # DS_IARM: link against libds
    find_package(DS)
    find_package(CEC)
    target_include_directories(${PLUGIN_IMPLEMENTATION} PRIVATE ${DS_INCLUDE_DIRS})
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PUBLIC
            ${NAMESPACE}Plugins::${NAMESPACE}Plugins
            ${DS_LIBRARIES}
            ${NAMESPACE}::${NAMESPACE}Helpers)
endif()

if (RDK_SERVICE_L2_TEST)
    message ("L2 test Enabled")
    find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
    if (TESTMOCKLIB_LIBRARIES)
        message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
        target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${TESTMOCKLIB_LIBRARIES})
    else (TESTMOCKLIB_LIBRARIES)
        message ("Require ${TESTMOCKLIB_LIBRARIES} library")
    endif (TESTMOCKLIB_LIBRARIES)
endif()

set_source_files_properties(HdcpProfile.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

install(TARGETS ${PLUGIN_IMPLEMENTATION}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
