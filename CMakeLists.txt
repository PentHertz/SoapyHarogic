cmake_minimum_required(VERSION 3.1)
project(SoapyHarogic CXX)

# Test if we can find SoapySDR and HTRA
find_package(SoapySDR "0.4.0" NO_MODULE REQUIRED)
if(NOT SoapySDR_FOUND)
    message(FATAL_ERROR "SoapySDR development files not found")
endif()

# Manual search for HTRA library
find_path(HTRA_INCLUDE_DIR
    NAMES HTRA_API.h htra_api.h
    HINTS /opt/htraapi/inc
    PATHS /usr/local/include /usr/include
)

find_library(HTRA_LIBRARY
    NAMES htraapi libhtraapi
    HINTS /opt/htraapi/lib/x86_64 /opt/htraapi/lib
    PATHS /usr/local/lib /usr/lib
)

if(HTRA_INCLUDE_DIR AND HTRA_LIBRARY)
    message(STATUS "Found HTRA include: ${HTRA_INCLUDE_DIR}")
    message(STATUS "Found HTRA library: ${HTRA_LIBRARY}")
else()
    message(FATAL_ERROR "HTRA not found")
endif()

# Check for source files
set(SOURCES "")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/HarogicDevice.cpp)
    list(APPEND SOURCES HarogicDevice.cpp)
    message(STATUS "Found HarogicDevice.cpp")
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/SoapyHarogic.cpp)
    list(APPEND SOURCES SoapyHarogic.cpp)
    message(STATUS "Found SoapyHarogic.cpp")
endif()

if(NOT SOURCES)
	message(FATAL_ERROR "No source files found! Expected HarogicDevice.cpp or SoapyHarogic.cpp")
endif()

# Create the module
SOAPY_SDR_MODULE_UTIL(
	TARGET HarogicSupport
    SOURCES ${SOURCES}
    LIBRARIES ${HTRA_LIBRARY}
)

target_include_directories(HarogicSupport PRIVATE ${HTRA_INCLUDE_DIR})

message(STATUS "Configuration successful!")
message(STATUS "Sources: ${SOURCES}")
message(STATUS "Target: HarogicSupport")
