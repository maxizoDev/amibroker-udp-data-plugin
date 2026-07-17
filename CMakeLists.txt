cmake_minimum_required(VERSION 3.20)
project(AmibrokerUDPData VERSION 1.0.0 LANGUAGES CXX)

# 64-bit MSVC + MFC are required for AmiBroker plug-ins. The build is
# expected to run on Windows with the "C++ MFC for latest build tools
# (x64)" component installed via the Visual Studio Installer.
if(NOT MSVC)
    message(WARNING "AmibrokerUDPData targets MSVC; non-MSVC builds will not work.")
endif()

# Use shared MFC DLL (matches the Data_Template / QT / ODBC samples).
# This must be set before MFC headers are pulled in.
set(CMAKE_MFC_FLAG 2)              # 2 = use MFC in a shared DLL
add_definitions(-D_AFXDLL)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# AmiBroker plug-ins are loaded via LoadLibrary; we want a true
# .dll module without the import-library cruft.
add_library(AmibrokerUDPData MODULE
    src/StdAfx.cpp
    src/App.cpp
    src/ConfigDlg.cpp
    src/Settings.cpp
    src/Logger.cpp
    src/TickBuffer.cpp
    src/UdpReceiver.cpp
    src/SymbolTable.cpp
    src/Backfill.cpp
    src/Plugin.cpp
    src/Resource.rc
)

target_include_directories(AmibrokerUDPData PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(AmibrokerUDPData PRIVATE
    _USRDLL
    _AFXDLL
    _CRT_SECURE_NO_WARNINGS
    WINVER=0x0601
    _WIN32_WINNT=0x0601
    _MBCS
)

target_compile_options(AmibrokerUDPData PRIVATE
    /W4
    /permissive-
    /Zc:forScope-       # matches QT sample's flag
    /UUNICODE           # undefine UNICODE so LPCTSTR stays char* (ANSI/MBCS build)
    /U_UNICODE
)

target_link_libraries(AmibrokerUDPData PRIVATE
    ws2_32
    winhttp
)

set_target_properties(AmibrokerUDPData PROPERTIES
    OUTPUT_NAME "AmibrokerUDPData"
    SUFFIX      ".dll"
    PREFIX      ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Force x64. AmiBroker 64-bit only loads x64 plug-ins; x86 builds are not
# supported (see CLAUDE.md "Boundaries").
if(CMAKE_SIZEOF_VOID_P AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "AmibrokerUDPData must be built as x64.")
endif()

message(STATUS "AmibrokerUDPData configured (x64 MFC DLL, output: bin/AmibrokerUDPData.dll)")
