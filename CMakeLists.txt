cmake_minimum_required(VERSION 3.16)
project(AmibrokerUDPDataPlugin VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force 64-bit build
set(CMAKE_GENERATOR_PLATFORM x64)

# Add the DLL library
add_library(AmibrokerUDPData SHARED
    src/Plugin.cpp
    src/UDPReceiver.cpp
    src/TickBuffer.cpp
)

# Set output directory
set_target_properties(AmibrokerUDPData PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Include directories
target_include_directories(AmibrokerUDPData PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Windows-specific settings
if(MSVC)
    set_target_properties(AmibrokerUDPData PROPERTIES
        SUFFIX ".dll"
        PREFIX ""
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    
    # Set subsystem for Windows
    set_target_properties(AmibrokerUDPData PROPERTIES
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
    
    # Optimization and runtime library
target_compile_options(AmibrokerUDPData PRIVATE
        /O2
        /W4
        /fp:precise
    )
endif()

# Link Windows socket library
target_link_libraries(AmibrokerUDPData PRIVATE ws2_32.lib)

# Set output name
set_target_properties(AmibrokerUDPData PROPERTIES
    OUTPUT_NAME "AmibrokerUDPData"
)

# Print build info
message(STATUS "AmiBroker UDP Data Plugin Configuration")
message(STATUS "========================================")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Platform: ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "Output Directory: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")