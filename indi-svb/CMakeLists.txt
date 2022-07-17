cmake_minimum_required(VERSION 3.0)
PROJECT(indi_svb CXX C)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake_modules/")
include(GNUInstallDirs)

find_package(CFITSIO REQUIRED)
find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(USB1 REQUIRED)
find_package(SV305 REQUIRED)

set(SVB_VERSION_MAJOR 0)
set(SVB_VERSION_MINOR 1)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_svb.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_svb.xml)

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories( ${CFITSIO_INCLUDE_DIR})

include(CMakeCommon)

if (INDI_WEBSOCKET)
    find_package(websocketpp REQUIRED)
    find_package(Boost COMPONENTS system thread)
    include_directories (${Boost_INCLUDE_DIRS})
    add_definitions(-DHAVE_WEBSOCKET)
    SET(HAVE_WEBSOCKET 1)
endif()

########### indi_svb_ccd ###########
set(indi_svb_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/svb_ccd.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svb_base.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svb_device.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/svb_temperature.cpp
   )

add_executable(indi_svb_ccd ${indi_svb_SRCS})
target_link_libraries(indi_svb_ccd ${SV305_LIBRARIES} ${INDI_LIBRARIES} ${CFITSIO_LIBRARIES} ${SVB_LIBRARIES} ${USB1_LIBRARIES} ${ZLIB_LIBRARY} ${CMAKE_THREAD_LIBS_INIT})
if (HAVE_WEBSOCKET)
    target_link_libraries(indi_svb_ccd ${Boost_LIBRARIES})
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
target_link_libraries(indi_svb_ccd rt)
#target_link_libraries(indi_svb_single_ccd rt)
endif (CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")

install(TARGETS indi_svb_ccd RUNTIME DESTINATION bin)
#install(TARGETS indi_svb_single_ccd RUNTIME DESTINATION bin)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_svb.xml DESTINATION ${INDI_DATA_DIR})
