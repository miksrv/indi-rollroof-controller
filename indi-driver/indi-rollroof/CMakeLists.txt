cmake_minimum_required(VERSION 2.4.7)
PROJECT(indi_rollroof CXX C)

set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(FIRMATA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/")
set (VERSION_MAJOR 0)
set (VERSION_MINOR 2)
 
find_package(INDI REQUIRED)
find_package(Nova REQUIRED)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})
include_directories(${FIRMATA_INCLUDE_DIR})


################# libfirmata ############################
set (firmata_VERSION_MAJOR 1)
set (firmata_VERSION_MINOR 0)
set (firmata_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/firmata.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/libfirmata/src/arduino.cpp
)
add_library(firmata ${firmata_SRCS})

################ Roll Off ################
set(rollroof_SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/rollroof.cpp
   )

add_executable(indi_rollroof ${rollroof_SRCS})
target_link_libraries(indi_rollroof ${INDI_DRIVER_LIBRARIES} firmata)
install(TARGETS indi_rollroof RUNTIME DESTINATION bin )
install(FILES indi_rollroof.xml DESTINATION ${INDI_DATA_DIR})

