cmake_minimum_required(VERSION 3.19)
project(agent_main LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/utf-8)
endif()

# MaaFramework only provides RELWITHDEBINFO, set default for multi-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
    set(CMAKE_CONFIGURATION_TYPES "RelWithDebInfo" CACHE STRING "Configuration types" FORCE)
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

include("D:/Scoop/apps/MaaFramework/current/share/cmake/MaaAgentServer/MaaAgentServer.cmake")

# MaaFramework only provides RELWITHDEBINFO config.
# For multi-config generators (Visual Studio), we need to set IMPORTED_IMPLIB for all configs.
get_target_property(_CURRENT_IMPLIB MaaAgentServer::MaaAgentServer IMPORTED_IMPLIB_RELWITHDEBINFO)
get_target_property(_CURRENT_LOCATION MaaAgentServer::MaaAgentServer IMPORTED_LOCATION_RELWITHDEBINFO)

set_target_properties(MaaAgentServer::MaaAgentServer PROPERTIES
    IMPORTED_IMPLIB_DEBUG "${_CURRENT_IMPLIB}"
    IMPORTED_IMPLIB_RELEASE "${_CURRENT_IMPLIB}"
    IMPORTED_IMPLIB_MINSIZEREL "${_CURRENT_IMPLIB}"
    IMPORTED_LOCATION_DEBUG "${_CURRENT_LOCATION}"
    IMPORTED_LOCATION_RELEASE "${_CURRENT_LOCATION}"
    IMPORTED_LOCATION_MINSIZEREL "${_CURRENT_LOCATION}"
)

set(MaaAgentServer MaaAgentServer::MaaAgentServer)

add_executable(agent_main
    route_recognition.cpp
    agent_main.cpp
)

target_include_directories(agent_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(agent_main ${MaaAgentServer} ${OpenCV_LIBS})

if(MSVC)
    target_compile_options(agent_main PRIVATE "/WX-")
else()
    target_compile_options(agent_main PRIVATE -Wno-error -Wno-unused-parameter -Wno-unused-variable)
endif()
