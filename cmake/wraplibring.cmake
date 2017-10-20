
MESSAGE(STATUS "Compiling with qtwrapper")

ADD_DEFINITIONS(-DENABLE_LIBWRAP=true)

# OS X
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
   SET(CMAKE_MACOSX_RPATH ON)
   SET(CMAKE_SKIP_BUILD_RPATH FALSE)
   SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
   SET(CMAKE_INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}")
   SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

IF (${MUTE_DRING} MATCHES "ON")
   #It makes clients debugging easier
   MESSAGE(STATUS "Dring logs are disabled")
   ADD_DEFINITIONS(-DMUTE_DRING=true)
ENDIF()

SET(libringclient_LIB_SRCS
   ${libringclient_LIB_SRCS}
   src/qtwrapper/instancemanager.cpp
   src/qtwrapper/callmanager_wrap.h
   src/qtwrapper/configurationmanager_wrap.h
   src/qtwrapper/instancemanager_wrap.h
   src/qtwrapper/presencemanager_wrap.h
)

QT5_WRAP_CPP(LIB_WRAPPER_HEADER_MOC ${libqtwrapper_LIB_SRCS})

IF(ENABLE_VIDEO)
   SET(libringclient_LIB_SRCS
      ${libringclient_LIB_SRCS}
      src/qtwrapper/videomanager_wrap.h
      src/qtwrapper/videomanager_wrap.cpp
   )
ENDIF()

ADD_DEFINITIONS(-fPIC)

IF(${VERBOSE_IPC} MATCHES "ON")
   MESSAGE(STATUS "Adding more debug output")
   ADD_DEFINITIONS(-DVERBOSE_IPC=true)
ENDIF()

# Allow building with undefined symbols when only the daemon headers are provided
# It speeds up our CI builds
IF(${ring_BIN} MATCHES "ring_BIN-NOTFOUND" AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    SET_TARGET_PROPERTIES( ringclient PROPERTIES LINK_FLAGS "-undefined dynamic_lookup" )
ENDIF()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/qtwrapper)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/dbus)
