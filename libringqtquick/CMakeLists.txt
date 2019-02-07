cmake_minimum_required(VERSION 3.1)

project(ringqtquick)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC TRUE)
SET(GENERIC_LIB_VERSION "1.0.0")
SET(QT_MIN_VERSION  "5.9.0" )

option(BUILD_SHARED_LIBS "Build the shared library (instead of static)" OFF)

# Shared is current NOT supported. The application will load but will NOT work
set(BUILD_SHARED_LIBS OFF)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED Core Quick)

if (ringqt_REQUIRE_DBUS)
   find_package(Qt5DBus ${QT_MIN_VERSION} REQUIRED)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
# include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../src) #FIXME

set(libringqtquick_CONFIG_PATH "${CMAKE_CURRENT_BINARY_DIR}/LibRingQtQuickConfig.cmake")

configure_package_config_file(
   "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibRingQtQuickConfig.cmake.in" ${libringqtquick_CONFIG_PATH}
   INSTALL_DESTINATION ${LIB_INSTALL_DIR}/libringqt/cmake
   PATH_VARS CMAKE_INSTALL_FULL_INCLUDEDIR
)

set(libringqtquick_LIB_SRCS
    src/accountfields.cpp
    src/accountbuilder.cpp
    src/messagebuilder.cpp
    src/contactbuilder.cpp
    src/sharedmodellocker.cpp
    src/ringaccountbuilder.cpp
    src/timelineiterator.cpp
    src/plugin.cpp
)

set(libringqtquick_LIB_HEADERS
    src/accountfields.h
    src/accountbuilder.h
    src/contactbuilder.h
    src/messagebuilder.h
    src/ringaccountbuilder.h
)

# if(NOT BUILD_SHARED_LIBS)
    add_definitions(-DQT_PLUGIN)
    add_definitions(-DQT_STATICPLUGIN=1)
# endif(NOT BUILD_SHARED_LIBS)

set(AUTOMOC_MOC_OPTIONS -Muri=net.lvindustries.ringqtquick)

add_library(ringqtquick STATIC ${libringqtquick_LIB_SRCS} )

target_include_directories( ringqtquick PRIVATE
    src/
    ../src/
    ${ringqt_INCLUDE_DIR}
)

target_link_libraries( ringqtquick
    ringqt
    Qt5::Core
    Qt5::Quick
)

set_target_properties(ringqtquick PROPERTIES
    PUBLIC_HEADER
    "${libringqtquick_LIB_HEADERS}"
)

set_target_properties( ringqtquick
   PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_VERSION}
)

#if("${DISABLE_EXPORT}" MATCHES "OFF")

    export(TARGETS ringqtquick FILE "${PROJECT_BINARY_DIR}/cmake/LibRingQtQuickTargets.cmake" )

    install(TARGETS ringqtquick
        EXPORT LibRingQtQuickTargets
        LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}" COMPONENT ringqtquick
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}/libringqt
        COMPONENT Devel
    )

    install(EXPORT LibRingQtQuickTargets
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/LibRingQtQuick
        COMPONENT Devel
    )

#endif()
