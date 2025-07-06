cmake_minimum_required(VERSION 3.8) # Bumping version for modern CMake features
project(SoapyHarogic CXX)

# Add the local 'cmake' directory to the search path for modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Find SoapySDR using its own config file
find_package(SoapySDR "0.8" REQUIRED)
message(STATUS "Found SoapySDR: ${SoapySDR_VERSION} - Includes: ${SoapySDR_INCLUDE_DIRS}, Libs: ${SoapySDR_LIBRARIES}")

# Find the Harogic HTRA library using our custom Find module
find_package(LibHTRA REQUIRED)
message(STATUS "Found LibHTRA: ${LibHTRA_VERSION} - Includes: ${LibHTRA_INCLUDE_DIRS}, Libs: ${LibHTRA_LIBRARIES}")

# Define the source files for the module
# This is cleaner than checking for file existence
set(SOURCES
    HarogicDevice.cpp
    # SoapyHarogic.cpp # Add this back if you create the file
)

# Use the SoapySDR utility to create the loadable module
# This handles all the library type, naming, and installation details
SOAPY_SDR_MODULE_UTIL(
    TARGET HarogicSupport
    SOURCES ${SOURCES}
    LIBRARIES ${LibHTRA_LIBRARIES}
)

# Add the necessary include directories to the target
# Using INTERFACE from find_package targets is the modern way, but this is also correct.
target_include_directories(HarogicSupport PRIVATE
    ${SoapySDR_INCLUDE_DIRS}
    ${LibHTRA_INCLUDE_DIRS}
)

# Link the libraries
# This is often handled by SOAPY_SDR_MODULE_UTIL, but being explicit is safe.
target_link_libraries(HarogicSupport PRIVATE
    ${SoapySDR_LIBRARIES}
    ${LibHTRA_LIBRARIES}
)

message(STATUS "Configuration successful! Target 'HarogicSupport' will be built.")
message(STATUS "Final install path will be: ${SoapySDR_MODULE_INSTALL_DIR}")
