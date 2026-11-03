cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME Application)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# ============================================================================
# wakaama / LwM2M
# ============================================================================

set(WAKAAMA_MODE_CLIENT ON CACHE BOOL "" FORCE)
set(WAKAAMA_MODE_SERVER OFF CACHE BOOL "" FORCE)
set(WAKAAMA_MODE_BOOTSTRAP_SERVER OFF CACHE BOOL "" FORCE)

set(WAKAAMA_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)

set(WAKAAMA_CLIENT_INITIATED_BOOTSTRAP OFF CACHE BOOL "" FORCE)
set(WAKAAMA_CLIENT_LWM2M_V_1_0 OFF CACHE BOOL "" FORCE)

set(WAKAAMA_DATA_TLV ON CACHE BOOL "" FORCE)
set(WAKAAMA_DATA_JSON OFF CACHE BOOL "" FORCE)
set(WAKAAMA_DATA_SENML_JSON OFF CACHE BOOL "" FORCE)
set(WAKAAMA_DATA_SENML_CBOR OFF CACHE BOOL "" FORCE)
set(WAKAAMA_DATA_OLD_CONTENT_FORMAT OFF CACHE BOOL "" FORCE)

set(WAKAAMA_LOG_LEVEL LOG_DISABLED CACHE STRING "" FORCE)

set(WAKAAMA_TRANSPORT NONE CACHE STRING "" FORCE)
set(WAKAAMA_PLATFORM NONE CACHE STRING "" FORCE)

set(WAKAAMA_CLI OFF CACHE BOOL "" FORCE)
set(WAKAAMA_UNIT_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/wakaama
    ${CMAKE_CURRENT_BINARY_DIR}/wakaama
    EXCLUDE_FROM_ALL
)

# ============================================================================

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/EVSE_APP/*.c"
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${SRC_FILES}
    # wakaama
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/wakaama/examples/client/common/object_security.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/wakaama/examples/client/common/object_server.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/wakaama/examples/lightclient/object_device.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    EVSE_APP/
    EVSE_APP/AP/
    EVSE_APP/BSP/
    EVSE_APP/DRIVER/
    EVSE_APP/HW/
    EVSE_APP/COMMON/
    EVSE_APP/TASK/
    EVSE_APP/TASK/OTA/
    EVSE_APP/TASK/WIFI/

    EVSE_APP/MIDDLEWARE/
    EVSE_APP/MIDDLEWARE/OTA/
    EVSE_APP/MIDDLEWARE/OTA/TRANSPORT/
    EVSE_APP/MIDDLEWARE/LWM2M/
    EVSE_APP/MIDDLEWARE/LWM2M/OBJECTS/
    EVSE_APP/MIDDLEWARE/OTA_SERVICE/
    
    # package
    EVSE_APP/PACKAGE/

    # wakaama
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/wakaama/examples/client/common
    ${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty/wakaama/coap/er-coap-13
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    USE_HAL_DRIVER
    STM32F429xx
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    
    # Add user defined libraries
    wakaama_static
)

# ELF 파일을 HEX와 BIN으로 변환
add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD

    COMMAND ${CMAKE_OBJCOPY}
            -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${CMAKE_PROJECT_NAME}>.hex

    COMMAND ${CMAKE_OBJCOPY}
            -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/$<TARGET_FILE_BASE_NAME:${CMAKE_PROJECT_NAME}>.bin

    COMMENT "Generating HEX and BIN files"
)