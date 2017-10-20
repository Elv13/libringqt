
MESSAGE(STATUS "Compiling with qtwrapper")

ADD_DEFINITIONS(-DENABLE_LIBWRAP=true)

IF (${MUTE_DRING} MATCHES "ON")
   #It makes clients debugging easier
   MESSAGE(STATUS "Dring logs are disabled")
   ADD_DEFINITIONS(-DMUTE_DRING=true)
ENDIF()

SET(CMAKE_AUTOMOC TRUE)

SET(libringclient_LIB_SRCS
   ${libringclient_LIB_SRCS}
   src/qtwrapper/instancemanager.cpp
   src/qtwrapper/callmanager_wrap.h
   src/qtwrapper/configurationmanager_wrap.h
   src/qtwrapper/instancemanager_wrap.h
   src/qtwrapper/presencemanager_wrap.h
   src/qtwrapper/videomanager_wrap.h
)

QT5_WRAP_CPP(LIB_WRAPPER_HEADER_MOC ${libqtwrapper_LIB_SRCS})

IF(NOT (${ENABLE_VIDEO} MATCHES "false"))
   ADD_DEFINITIONS(-DENABLE_VIDEO=true)
   SET(ENABLE_VIDEO 1 CACHE BOOLEAN "Enable video")
ENDIF(NOT (${ENABLE_VIDEO} MATCHES "false"))

ADD_DEFINITIONS(-fPIC)

IF(${VERBOSE_IPC} MATCHES "ON")
   MESSAGE(STATUS "Adding more debug output")
   ADD_DEFINITIONS(-DVERBOSE_IPC=true)
ENDIF()

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/qtwrapper)
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/src/dbus)
