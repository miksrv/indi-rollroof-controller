cmake_minimum_required(VERSION 3.18)
PROJECT(indi_rollroof CXX C)

# INDI looks for drivers and their XML descriptions under /usr
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FIRMATA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/")
set (VERSION_MAJOR 2)
set (VERSION_MINOR 0)

# INDI 2.x: headers in <prefix>/include/libindi, driver API in libindidriver
find_path(INDI_INCLUDE_DIR indidome.h PATH_SUFFIXES libindi REQUIRED)
find_library(INDI_DRIVER_LIBRARY indidriver REQUIRED)
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
install(TARGETS indi_rollroof RUNTIME DESTINATION bin )
install(FILES indi_rollroof.xml DESTINATION ${INDI_DATA_DIR})
