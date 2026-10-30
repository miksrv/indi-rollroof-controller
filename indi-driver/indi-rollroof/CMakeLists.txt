cmake_minimum_required(VERSION 3.18)
PROJECT(indi_rollroof CXX C)

# INDI looks for drivers and their XML descriptions under /usr
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FIRMATA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/")
# The driver version: the single source for the driver XML and DRIVER_INFO
set (VERSION_MAJOR 2)
set (VERSION_MINOR 0)
set (VERSION_PATCH 1)

# INDI 2.x: headers in <prefix>/include/libindi, driver API in libindidriver
find_path(INDI_INCLUDE_DIR indidome.h PATH_SUFFIXES libindi REQUIRED)
find_library(INDI_DRIVER_LIBRARY indidriver REQUIRED)

# INDI 1.x headers compile with confusing errors, fail early with a clear message instead
file(STRINGS "${INDI_INCLUDE_DIR}/indiversion.h" INDI_VERSION_MAJOR_LINE REGEX "#define INDI_VERSION_MAJOR")
string(REGEX MATCH "[0-9]+" INDI_VERSION_MAJOR "${INDI_VERSION_MAJOR_LINE}")
if(INDI_VERSION_MAJOR LESS 2)
    message(FATAL_ERROR "INDI 2.x is required, found INDI ${INDI_VERSION_MAJOR}.x headers in ${INDI_INCLUDE_DIR}")
endif()
set(INDI_DATA_DIR "${CMAKE_INSTALL_PREFIX}/share/indi")

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${FIRMATA_INCLUDE_DIR})


################# libfirmata ############################
set (firmata_VERSION_MAJOR 1)
set (firmata_VERSION_MINOR 0)
set (firmata_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/firmata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/arduino.cpp
)
add_library(firmata STATIC ${firmata_SRCS})

################ Roll Off ################
set(rollroof_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/rollroof.cpp
   )

add_executable(indi_rollroof ${rollroof_SRCS})
target_link_libraries(indi_rollroof ${INDI_DRIVER_LIBRARY} firmata)
target_compile_definitions(indi_rollroof PRIVATE
        ROLLROOF_VERSION_MAJOR=${VERSION_MAJOR}
        ROLLROOF_VERSION_MINOR=${VERSION_MINOR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_rollroof.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_rollroof.xml)

install(TARGETS indi_rollroof RUNTIME DESTINATION bin )
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_rollroof.xml DESTINATION ${INDI_DATA_DIR})
