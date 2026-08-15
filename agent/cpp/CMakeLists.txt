cmake_minimum_required(VERSION 3.19)
project(route_recognition LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

if(NOT DEFINED MAA_FRAMEWORK_DIR)
    set(MAA_FRAMEWORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../deps/MaaFramework")
endif()

include("${MAA_FRAMEWORK_DIR}/share/cmake/MaaAgentServer/MaaAgentServer.cmake")

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

add_executable(route_recognition
    route_recognition.cpp
    agent_main.cpp
)

target_include_directories(route_recognition PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(route_recognition MaaAgentServer::MaaAgentServer)

if(MSVC)
    target_compile_options(route_recognition PRIVATE "/WX-")
else()
    target_compile_options(route_recognition PRIVATE -Wno-error -Wno-unused-parameter -Wno-unused-variable)
endif()
