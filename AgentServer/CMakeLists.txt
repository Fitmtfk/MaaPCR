cmake_minimum_required(VERSION 3.28)

project(MaaPCRAgentServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

if(NOT DEFINED MAA_FRAMEWORK_DIR)
    set(MAA_FRAMEWORK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps")
endif()

list(APPEND CMAKE_PREFIX_PATH "${MAA_FRAMEWORK_DIR}")

if(NOT DEFINED MAA_Deps_DIR)
    set(MAA_Deps_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../deps/MaaDeps")
endif()

include("${MAA_Deps_DIR}/maadeps.cmake")

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(nlohmann_json REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(Boost REQUIRED CONFIG COMPONENTS system regex)
find_package(MaaAgentServer REQUIRED)

add_executable(MaaPCRAgentServer
    logger.cpp
    route_recognition.cpp
    agent_main.cpp
)

target_link_libraries(MaaPCRAgentServer
    MaaAgentServer::MaaAgentServer
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
)

set_target_properties(MaaPCRAgentServer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../agent"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../agent"
)

if(MSVC)
    target_compile_options(MaaPCRAgentServer PRIVATE "/WX-")
else()
    target_compile_options(MaaPCRAgentServer PRIVATE -Wno-error -Wno-unused-parameter -Wno-unused-variable)
endif()