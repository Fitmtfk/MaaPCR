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

if(NOT EXISTS "${MAA_FRAMEWORK_DIR}/share/cmake/MaaAgentServer/MaaAgentServer.cmake")
    message(FATAL_ERROR "MaaFramework not found at: ${MAA_FRAMEWORK_DIR}. Please check the path or pass -DMAA_FRAMEWORK_DIR=...")
endif()

include("${MAA_FRAMEWORK_DIR}/share/cmake/MaaAgentServer/MaaAgentServer.cmake")

get_target_property(_CURRENT_IMPLIB MaaAgentServer::MaaAgentServer IMPORTED_IMPLIB_RELWITHDEBINFO)
get_target_property(_CURRENT_LOCATION MaaAgentServer::MaaAgentServer IMPORTED_LOCATION_RELWITHDEBINFO)

if(_CURRENT_IMPLIB AND _CURRENT_LOCATION)
    set_target_properties(MaaAgentServer::MaaAgentServer PROPERTIES
        IMPORTED_IMPLIB_DEBUG "${_CURRENT_IMPLIB}"
        IMPORTED_IMPLIB_RELEASE "${_CURRENT_IMPLIB}"
        IMPORTED_LOCATION_DEBUG "${_CURRENT_LOCATION}"
        IMPORTED_LOCATION_RELEASE "${_CURRENT_LOCATION}"
    )
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)

find_library(MAA_OPENCV_LIB
    NAMES opencv_world4_maa
    PATHS "${MAA_FRAMEWORK_DIR}/lib" "${MAA_FRAMEWORK_DIR}/bin"
    NO_DEFAULT_PATH
)
message("MAA_OPENCV_LIB:${MAA_OPENCV_LIB}")

add_executable(route_recognition
    route_recognition.cpp
    agent_main.cpp
)

target_include_directories(route_recognition
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        "${MAA_FRAMEWORK_DIR}/include"
        ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(route_recognition
    PRIVATE
        MaaAgentServer::MaaAgentServer
        ${MAA_OPENCV_LIB}
)

if(MSVC)
    target_compile_options(route_recognition PRIVATE "/WX-")
else()
    target_compile_options(route_recognition PRIVATE -Wno-error -Wno-unused-parameter -Wno-unused-variable)
endif()

if(WIN32)
    set(PUBLISH_DIR "${CMAKE_BINARY_DIR}/dist")

    add_custom_target(publish
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PUBLISH_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:route_recognition>"
            "${PUBLISH_DIR}"
        DEPENDS route_recognition
        COMMENT "Packaging release bundle..."
    )

    # 复制 MaaFramework 自带的所有 DLL
    get_target_property(_MAA_LOCATION MaaAgentServer::MaaAgentServer IMPORTED_LOCATION_RELWITHDEBINFO)
    if(_MAA_LOCATION)
        get_filename_component(_MAA_BIN_DIR "${_MAA_LOCATION}" DIRECTORY)
        file(GLOB MAA_RUNTIME_DLLS "${_MAA_BIN_DIR}/*.dll")
        if(MAA_RUNTIME_DLLS)
            add_custom_command(TARGET publish POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${MAA_RUNTIME_DLLS}
                    "${PUBLISH_DIR}"
                COMMENT "Copying MaaFramework runtime DLLs..."
            )
        endif()
    endif()
endif()