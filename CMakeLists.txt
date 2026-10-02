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
set(CMAKE_PROJECT_NAME WokwiSystem)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

option(WOKWI_SIMULATION "Use the STM32F103 Wokwi FreeRTOS port" ON)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Shared with the HAL and FreeRTOS object libraries, not just main.c.
if(WOKWI_SIMULATION)
    target_compile_definitions(stm32cubemx INTERFACE WOKWI_ENABLED=1)

    # Wokwi's STM32F103 model executes SVC as a normal instruction and does
    # not complete the standard Cortex-M exception-return task startup.  Keep
    # the vendor ARM_CM3 port untouched and replace only the port layer in the
    # simulator build.
    set(WOKWI_FREERTOS_PORT_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3_WOKWI")
    get_target_property(FREERTOS_TARGET_SOURCES FreeRTOS SOURCES)
    list(FILTER FREERTOS_TARGET_SOURCES EXCLUDE REGEX
        "portable/GCC/ARM_CM3/port\\.c$")
    set_property(TARGET FreeRTOS PROPERTY SOURCES "${FREERTOS_TARGET_SOURCES}")
    target_sources(FreeRTOS PRIVATE "${WOKWI_FREERTOS_PORT_DIR}/port.c")
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE Core/Src/wokwi_i2c.c Core/Src/wokwi_spi.c)
    # GPIO loops run many times per byte. Retain debug symbols, but avoid the
    # call/stack overhead of -O0 on these two small simulator transports.
    set_source_files_properties(Core/Src/wokwi_i2c.c Core/Src/wokwi_spi.c
        PROPERTIES COMPILE_OPTIONS "-O2")
    target_include_directories(stm32cubemx BEFORE INTERFACE
        "${WOKWI_FREERTOS_PORT_DIR}")
endif()

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/bmp280.c
    Core/Src/ssd1306.c
    Core/Src/app_tasks.c
    Core/Src/pressure_processing.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
