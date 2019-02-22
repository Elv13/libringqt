# Once done this will define
#  ring_INCLUDE_DIRS - include directories
#  ring_BIN - Path to Ring binary

SET(RING_FOUND true)

# Check the environment variables override. This is useful when cross-compiling
# DRing/LibJami in a different container
IF(EXISTS $ENV{RING_BUILD_DIR})
   SET(RING_BUILD_DIR $ENV{RING_BUILD_DIR})
ENDIF()
IF(EXISTS $ENV{RING_INCLUDE_DIRS})
   SET(ring_INCLUDE_DIRS $ENV{RING_INCLUDE_DIRS})
ENDIF()
IF(EXISTS $ENV{RING_LIBRARY})
   SET(ring_BIN $ENV{RING_LIBRARY})
ENDIF()

IF(NOT ring_INCLUDE_DIRS AND EXISTS ${CMAKE_INSTALL_PREFIX}/include/dring/dring.h)
   SET(ring_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include/dring)
ELSEIF(EXISTS ${RING_INCLUDE_DIR}/dring.h)
   SET(ring_INCLUDE_DIRS ${RING_INCLUDE_DIR})
ELSEIF(EXISTS ${RING_BUILD_DIR}/dring/dring.h)
   SET(ring_INCLUDE_DIRS ${RING_BUILD_DIR}/dring)
ELSEIF(DEFINED RING_XML_INTERFACES_DIR AND (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD"))
   MESSAGE("Daemon header not found, DBus mode only")
   SET(RING_FOUND true)
ELSEIF(NOT ring_INCLUDE_DIRS OR NOT ring_BIN)
   MESSAGE(STATUS "Daemon header not found!
   Add -DRING_BUILD_DIR or -DCMAKE_INSTALL_PREFIX")
   SET(RING_FOUND false)
ENDIF()

# Try to honor the CMake settings
IF (NOT ring_BIN AND "${USE_STATIC_LIBRING}" MATCHES ON AND ${BUILD_SHARED_LIBS} MATCHES OFF)
   SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.lib")

   FIND_LIBRARY(ring_BIN NAMES ring
      PATHS ${RING_BUILD_DIR}/.libs
      PATHS ${CMAKE_INSTALL_PREFIX}/lib
      PATHS ${CMAKE_INSTALL_PREFIX}/libexec
   )
ELSEIF(NOT ring_BIN AND NOT ${BUILD_SHARED_LIBS} MATCHES ON)
   SET(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib;.so;.dll")

   FIND_LIBRARY(ring_BIN NAMES ring
      PATHS ${RING_BUILD_DIR}/.libs
      PATHS ${CMAKE_INSTALL_PREFIX}/lib
      PATHS ${CMAKE_INSTALL_PREFIX}/libexec
      PATHS ${CMAKE_INSTALL_PREFIX}/bin
   )
ENDIF()

# Try again if nothing was found
IF (NOT ring_BIN)
   SET(CMAKE_FIND_LIBRARY_SUFFIXES ".dylib;.so;.dll")

   FIND_LIBRARY(ring_BIN NAMES ring
      PATHS ${RING_BUILD_DIR}/.libs
      PATHS ${CMAKE_INSTALL_PREFIX}/lib
      PATHS ${CMAKE_INSTALL_PREFIX}/libexec
      PATHS ${CMAKE_INSTALL_PREFIX}/bin
   )
ENDIF()

IF (NOT ring_BIN)
   SET(CMAKE_FIND_LIBRARY_SUFFIXES ".a;.lib")

   FIND_LIBRARY(ring_BIN NAMES ring
      PATHS ${RING_BUILD_DIR}/.libs
      PATHS ${CMAKE_INSTALL_PREFIX}/lib
      PATHS ${CMAKE_INSTALL_PREFIX}/libexec
   )
ENDIF()

IF(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Windows")
   ADD_DEFINITIONS(-fPIC)
ENDIF()

MESSAGE(STATUS "Ring daemon header is in " ${ring_INCLUDE_DIRS})
MESSAGE(STATUS "Ring library path is " ${ring_BIN})
