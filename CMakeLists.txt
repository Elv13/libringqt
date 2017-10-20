CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

IF(POLICY CMP0053)
   CMAKE_POLICY(SET CMP0053 NEW)
ENDIF(POLICY CMP0053)

IF(POLICY CMP0022)
   CMAKE_POLICY(SET CMP0022 NEW)
ENDIF(POLICY CMP0022)

IF(POLICY CMP0043)
   CMAKE_POLICY(SET CMP0043 NEW)
ENDIF(POLICY CMP0043)

IF(POLICY CMP0020)
   CMAKE_POLICY(SET CMP0020 NEW)
ENDIF(POLICY CMP0020)

IF(POLICY CMP0063)
   CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)

IF(POLICY CMP0023)
  CMAKE_POLICY(SET CMP0023 OLD)
ENDIF(POLICY CMP0023)

INCLUDE(GNUInstallDirs)
INCLUDE(CMakePackageConfigHelpers)
INCLUDE(GenerateExportHeader)
INCLUDE(CheckLibraryExists)

SET(CMAKE_CXX_STANDARD 14)

ADD_DEFINITIONS(
   ${QT_DEFINITIONS}
   -fexceptions
)

PROJECT(ringclient)

OPTION(BUILD_SHARED_LIBS   "Build the shared library (instead of static)" ON )
OPTION(MUTE_DRING          "Do not print libring debug output"            OFF)
OPTION(VERBOSE_IPC         "Print all dring function calls (for debug)"   OFF)
OPTION(ENABLE_TEST_ASSERTS "Enable extra asserts (cpu intensive)"         OFF)

IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
   OPTION(ENABLE_LIBWRAP    "Disable the daemon and use libring directly"  OFF)
   OPTION(DISABLE_EXPORT    "Do not install anything"                      OFF)
ELSE()
   OPTION(ENABLE_LIBWRAP    "Disable the daemon and use libring directly"  ON )
   OPTION(DISABLE_EXPORT    "Do not install anything"                      ON )
ENDIF()

# First, check is the compiler is new enough, most version of Clang are fine
# until problems arise, checking for GCC is enough
IF (CMAKE_COMPILER_IS_GNUCC)
   EXECUTE_PROCESS(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
   IF (GCC_VERSION VERSION_GREATER 4.8 OR GCC_VERSION VERSION_EQUAL 4.8)
      MESSAGE(STATUS "Found GCC version >= 4.8: " ${GCC_VERSION})
   ELSE()
      MESSAGE(FATAL_ERROR "Your version of GCC is too old, please install GCC 4.8 or later")
   ENDIF()
ENDIF()

SET(CMAKE_AUTOMOC TRUE)

SET(LOCAL_CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${LOCAL_CMAKE_MODULE_PATH})

SET(QT5_MODULE_PATH ${QT5_PATH}/lib/cmake)

FIND_PACKAGE(Ring REQUIRED)
FIND_PACKAGE(Qt5Core REQUIRED)
FIND_PACKAGE(Qt5LinguistTools QUIET) # translations

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux" AND NOT ENABLE_LIBWRAP)
   FIND_PACKAGE(Qt5DBus)
ELSE()
   SET(ENABLE_LIBWRAP true)
ENDIF()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/extrawarnings.cmake)

# OS X
IF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

   SET(CMAKE_MACOSX_RPATH ON)
   SET(CMAKE_SKIP_BUILD_RPATH FALSE)
   SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
   SET(CMAKE_INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}")
   SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")

IF(ENABLE_LIBWRAP)
   INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/wraplibring.cmake)
ENDIF()

IF(NOT "${ENABLE_VIDEO}" MATCHES false)
   MESSAGE(STATUS "VIDEO enabled")
   SET(ENABLE_VIDEO 1 CACHE BOOLEAN "Enable video")
   add_definitions( -DENABLE_VIDEO=true )
ENDIF(NOT "${ENABLE_VIDEO}" MATCHES false)

# ADD_DEFINITIONS(-DQT_DISABLE_DEPRECATED_BEFORE=1)

SET(GENERIC_LIB_VERSION "1.0.0")

INCLUDE_DIRECTORIES(SYSTEM ${QT_INCLUDES} )

# Build dbus interfaces
IF(DEFINED RING_XML_INTERFACES_DIR)
    SET (dbus_xml_introspecs_path ${RING_XML_INTERFACES_DIR})
ELSEIF(EXISTS "${RING_BUILD_DIR}/../bin/dbus")
    SET (dbus_xml_introspecs_path ${RING_BUILD_DIR}/../bin/dbus)
ELSE()
    SET (dbus_xml_introspecs_path ${CMAKE_INSTALL_PREFIX}/share/dbus-1/interfaces)
ENDIF()

#File to compile
SET( libringclient_LIB_SRCS
  ${libringclient_LIB_SRCS}

  #Data objects
  src/call.cpp
  src/uri.cpp
  src/ringdevice.cpp
  src/account.cpp
  src/credential.cpp
  src/person.cpp
  src/contactmethod.cpp
  src/numbercategory.cpp
  src/macro.cpp
  src/collectionextensioninterface.cpp
  src/video/rate.cpp
  src/video/device.cpp
  src/video/renderer.cpp
  src/certificate.cpp
  src/securityflaw.cpp
  src/ringtone.cpp
  src/profile.cpp
  src/contactrequest.cpp
  src/media/media.cpp
  src/media/audio.cpp
  src/media/video.cpp
  src/media/text.cpp
  src/media/file.cpp
  src/media/recording.cpp
  src/media/avrecording.cpp
  src/media/textrecording.cpp
  src/media/mimemessage.cpp

  #Models
  src/bootstrapmodel.cpp
  src/ringdevicemodel.cpp
  src/accountmodel.cpp
  src/availableaccountmodel.cpp
  src/callmodel.cpp
  src/categorizedhistorymodel.cpp
  src/categorizedbookmarkmodel.cpp
  src/credentialmodel.cpp
  src/categorizedcontactmodel.cpp
  src/useractionmodel.cpp
  src/presencestatusmodel.cpp
  src/phonedirectorymodel.cpp
  src/historytimecategorymodel.cpp
  src/numbercategorymodel.cpp
  src/macromodel.cpp
  src/keyexchangemodel.cpp
  src/tlsmethodmodel.cpp
  src/protocolmodel.cpp
  src/numbercompletionmodel.cpp
  src/profilemodel.cpp
  src/ringtonemodel.cpp
  src/lastusednumbermodel.cpp
  src/securityevaluationmodel.cpp
  src/personmodel.cpp
  src/collectionmodel.cpp
  src/collectionextensionmodel.cpp
  src/collectionmanagerinterface.cpp
  src/networkinterfacemodel.cpp
  src/certificatemodel.cpp
  src/ciphermodel.cpp
  src/accountstatusmodel.cpp
  src/codecmodel.cpp
  src/chainoftrustmodel.cpp
  src/pendingcontactrequestmodel.cpp
  src/peerstimelinemodel.cpp
  src/peertimelinemodel.cpp
  src/video/devicemodel.cpp
  src/video/sourcemodel.cpp
  src/video/channel.cpp
  src/video/resolution.cpp
  src/video/configurationproxy.cpp
  src/audio/alsapluginmodel.cpp
  src/audio/inputdevicemodel.cpp
  src/audio/managermodel.cpp
  src/audio/outputdevicemodel.cpp
  src/audio/ringtonedevicemodel.cpp
  src/audio/settings.cpp
  src/media/recordingmodel.cpp
  src/bannedcontactmodel.cpp

  #Data collections
  src/transitionalpersonbackend.cpp
  src/collectioninterface.cpp
  src/collectioneditor.cpp
  src/fallbackpersoncollection.cpp
  src/daemoncertificatecollection.cpp
  src/foldercertificatecollection.cpp
  src/localrecordingcollection.cpp
  src/localtextrecordingcollection.cpp
  src/localhistorycollection.cpp
  src/localprofilecollection.cpp
  src/localmacrocollection.cpp
  src/localbookmarkcollection.cpp
  src/localringtonecollection.cpp
  src/peerprofilecollection2.cpp

  #Communication
  src/dbus/configurationmanager.cpp
  src/dbus/callmanager.cpp
  src/dbus/instancemanager.cpp
  src/dbus/videomanager.cpp
  src/dbus/presencemanager.cpp

  #Default interface implementations
  src/globalinstances.cpp
  src/accountlistcolorizerdefault.cpp
  src/presenceserializerdefault.cpp
  src/pixmapmanipulatordefault.cpp
  src/shortcutcreatordefault.cpp
  src/actionextenderdefault.cpp
  src/dbuserrorhandlerdefault.cpp

  #Other
  src/hookmanager.cpp
  src/namedirectory.cpp
  src/itembase.cpp
  src/private/vcardutils.cpp
  src/private/textrecordingcache.cpp
  src/private/textrecordingmodel.cpp
  src/private/videorenderermanager.cpp
  src/video/previewmanager.cpp
  src/private/sortproxies.cpp
  src/private/threadworker.cpp
  src/private/cmcallsmodel.cpp
  src/private/personcmmodel.cpp
  src/private/addressmodel.cpp
  src/mime.cpp
  src/smartinfohub.cpp

  #Extension
  src/extensions/presencecollectionextension.cpp
  src/extensions/securityevaluationextension.cpp
)

IF(ENABLE_LIBWRAP)
   SET(libringclient_LIB_SRCS ${libringclient_LIB_SRCS}
      src/private/directrenderer.cpp
   )
ELSE()
   SET(libringclient_LIB_SRCS ${libringclient_LIB_SRCS}
      src/private/shmrenderer.cpp
   )
ENDIF(ENABLE_LIBWRAP)

# Public API
SET( libringclient_LIB_HDRS
  src/account.h
  src/ringdevice.h
  src/credential.h
  src/accountmodel.h
  src/availableaccountmodel.h
  src/call.h
  src/call.hpp
  src/callmodel.h
  src/categorizedhistorymodel.h
  src/person.h
  src/bootstrapmodel.h
  src/ringdevicemodel.h
  src/collectioninterface.h
  src/collectioninterface.hpp
  src/categorizedbookmarkmodel.h
  src/credentialmodel.h
  src/categorizedcontactmodel.h
  src/useractionmodel.h
  src/presencestatusmodel.h
  src/contactmethod.h
  src/phonedirectorymodel.h
  src/historytimecategorymodel.h
  src/numbercategorymodel.h
  src/keyexchangemodel.h
  src/codecmodel.h
  src/tlsmethodmodel.h
  src/protocolmodel.h
  src/numbercompletionmodel.h
  src/profilemodel.h
  src/numbercategory.h
  src/ringtonemodel.h
  src/localrecordingcollection.h
  src/localbookmarkcollection.h
  src/localringtonecollection.h
  src/localmacrocollection.h
  src/localhistorycollection.h
  src/localprofilecollection.h
  src/localtextrecordingcollection.h
  src/peerprofilecollection2.h
  src/lastusednumbermodel.h
  src/securityevaluationmodel.h
  src/certificate.h
  src/personmodel.h
  src/transitionalpersonbackend.h
  src/collectionmodel.h
  src/collectionextensionmodel.h
  src/collectionextensionmodel.hpp
  src/macromodel.h
  src/hookmanager.h
  src/namedirectory.h
  src/uri.h
  src/macro.h
  src/ringtone.h
  src/profile.h
  src/itembase.h
  src/itembase.hpp
  src/mime.h
  src/collectionextensioninterface.h
  src/collectionmanagerinterface.h
  src/collectionmanagerinterface.hpp
  src/networkinterfacemodel.h
  src/certificatemodel.h
  src/ciphermodel.h
  src/accountstatusmodel.h
  src/collectionmediator.h
  src/collectionmediator.hpp
  src/collectioneditor.h
  src/collectioneditor.hpp
  src/fallbackpersoncollection.h
  src/daemoncertificatecollection.h
  src/foldercertificatecollection.h
  src/securityflaw.h
  src/collectioncreationinterface.h
  src/collectionconfigurationinterface.h
  src/peerstimelinemodel.h
  src/peertimelinemodel.h
  src/chainoftrustmodel.h
  src/pendingcontactrequestmodel.h
  src/contactrequest.h
  src/globalinstances.h
  src/accountlistcolorizerdefault.h
  src/presenceserializerdefault.h
  src/pixmapmanipulatordefault.h
  src/shortcutcreatordefault.h
  src/dbuserrorhandlerdefault.h
  src/itemdataroles.h
  src/smartinfohub.h
  src/usagestatistics.h
  src/bannedcontactmodel.h
)

SET(libringclient_video_LIB_HDRS
  src/video/device.h
  src/video/devicemodel.h
  src/video/sourcemodel.h
  src/video/renderer.h
  src/video/resolution.h
  src/video/channel.h
  src/video/rate.h
  src/video/previewmanager.h
  src/video/configurationproxy.h
  #The renderer implementations are not exported on purpose
)

SET(libringclient_audio_LIB_HDRS
  src/audio/alsapluginmodel.h
  src/audio/inputdevicemodel.h
  src/audio/managermodel.h
  src/audio/outputdevicemodel.h
  src/audio/ringtonedevicemodel.h
  src/audio/settings.h
)

SET(libringclient_extensions_LIB_HDRS
  src/extensions/presencecollectionextension.h
  src/extensions/securityevaluationextension.h
)

SET(libringclient_media_LIB_HDRS
  src/media/media.h
  src/media/audio.h
  src/media/video.h
  src/media/text.h
  src/media/file.h
  src/media/recording.h
  src/media/avrecording.h
  src/media/textrecording.h
  src/media/recordingmodel.h
)

SET(libringclient_interface_LIB_HDRS
  src/interfaces/accountlistcolorizeri.h
  src/interfaces/contactmethodselectori.h
  src/interfaces/presenceserializeri.h
  src/interfaces/itemmodelstateserializeri.h
  src/interfaces/pixmapmanipulatori.h
  src/interfaces/shortcutcreatori.h
  src/interfaces/actionextenderi.h
  src/interfaces/dbuserrorhandleri.h
)

SET( libringclient_extra_LIB_HDRS
  src/typedefs.h
)

IF(NOT ENABLE_LIBWRAP)
   # presence manager interface
   SET ( presencemanager_xml  ${dbus_xml_introspecs_path}/cx.ring.Ring.PresenceManager.xml )

   SET( dbus_metatype_path "${CMAKE_CURRENT_SOURCE_DIR}/src/dbus/metatypes.h")

   SET_SOURCE_FILES_PROPERTIES(
      ${presencemanager_xml}
      PROPERTIES
      CLASSNAME PresenceManagerInterface
      INCLUDE ${dbus_metatype_path}
   )

   QT5_ADD_DBUS_INTERFACE(
      libringclient_LIB_SRCS
      ${presencemanager_xml}
      presencemanager_dbus_interface
   )

   # configuration manager interface
   SET ( configurationmanager_xml  ${dbus_xml_introspecs_path}/cx.ring.Ring.ConfigurationManager.xml )

   SET_SOURCE_FILES_PROPERTIES(
      ${configurationmanager_xml}
      PROPERTIES
      CLASSNAME ConfigurationManagerInterface
      INCLUDE ${dbus_metatype_path}
   )

   QT5_ADD_DBUS_INTERFACE(
      libringclient_LIB_SRCS
      ${configurationmanager_xml}
      configurationmanager_dbus_interface
   )

   # call manager interface
   SET ( callmanager_xml  ${dbus_xml_introspecs_path}/cx.ring.Ring.CallManager.xml )

   SET_SOURCE_FILES_PROPERTIES(
      ${callmanager_xml}
      PROPERTIES
      CLASSNAME CallManagerInterface
      INCLUDE ${dbus_metatype_path}
   )

   QT5_ADD_DBUS_INTERFACE(
      libringclient_LIB_SRCS
      ${callmanager_xml}
      callmanager_dbus_interface
   )

   # video manager interface
   SET ( video_xml  ${dbus_xml_introspecs_path}/cx.ring.Ring.VideoManager.xml )

   SET_SOURCE_FILES_PROPERTIES(
      ${video_xml}
      PROPERTIES
      CLASSNAME VideoManagerInterface
      INCLUDE ${dbus_metatype_path}
   )

   QT5_ADD_DBUS_INTERFACE(
      libringclient_LIB_SRCS
      ${video_xml}
      video_dbus_interface
   )


   # instance interface
   SET ( instance_xml  ${dbus_xml_introspecs_path}/cx.ring.Ring.Instance.xml )

   SET_SOURCE_FILES_PROPERTIES(
      ${instance_xml}
      PROPERTIES
      CLASSNAME InstanceManagerInterface
      INCLUDE ${dbus_metatype_path}
   )

   QT5_ADD_DBUS_INTERFACE(
      libringclient_LIB_SRCS
      ${instance_xml}
      instance_dbus_interface
   )

ENDIF()

# Manually wrap private files and interfaces
SET(libringclient_PRIVATE_HDRS
   src/private/call_p.h
   src/private/ringdevicemodel_p.h
   src/private/namedirectory_p.h
   src/private/account_p.h
   src/private/sortproxies.h
   src/private/accountmodel_p.h
   src/private/phonedirectorymodel_p.h
   src/private/videorenderer_p.h
   src/private/textrecording_p.h
   src/private/videodevice_p.h
   src/private/collectionmodel_p.h
   src/private/securityflaw_p.h
   src/private/macromodel_p.h
   src/private/person_p.h
   src/collectioncreationinterface.h
   src/private/securityevaluationmodel_p.h
   src/collectionconfigurationinterface.h
   src/private/imconversationmanagerprivate.h
   src/private/smartInfoHub_p.h
   src/private/certificatemodel_p.h
   src/usagestatistics.h
   src/private/personstatistics.hpp
)

QT5_WRAP_CPP(LIB_HEADER_MOC ${libringclient_PRIVATE_HDRS})

ADD_LIBRARY( ringclient ${libringclient_LIB_SRCS} ${LIB_HEADER_MOC} )

IF (DEFINED ring_INCLUDE_DIRS)
   TARGET_INCLUDE_DIRECTORIES( ringclient PRIVATE
      ${ring_INCLUDE_DIRS}
   )
ENDIF()

IF(ENABLE_LIBWRAP)
   TARGET_INCLUDE_DIRECTORIES( ringclient PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src/qtwrapper/
   )
ENDIF()

QT5_USE_MODULES(ringclient Core)

IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  QT5_USE_MODULES(ringclient DBus)
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
   FIND_PACKAGE(Qt5Concurrent)

   #For some reason, QSemaphore wont compile on Windows without QtConcurrent
   QT5_USE_MODULES(ringclient Concurrent)
ENDIF()

SET(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/include)

SET(libringclient_CONFIG_PATH "${CMAKE_CURRENT_BINARY_DIR}/LibRingClientConfig.cmake")

CONFIGURE_PACKAGE_CONFIG_FILE(
   "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibRingClientConfig.cmake.in" ${libringclient_CONFIG_PATH}
   INSTALL_DESTINATION ${LIB_INSTALL_DIR}/libringclient/cmake
   PATH_VARS INCLUDE_INSTALL_DIR
)

SET_TARGET_PROPERTIES(ringclient PROPERTIES
    PUBLIC_HEADER "${libringclient_LIB_HDRS};${libringclient_extra_LIB_HDRS}"
)

TARGET_LINK_LIBRARIES( ringclient
   -lpthread
   Qt5::Core
)

# Include as PRIVATE because of the /src/ component
#TODO figure out how to make them public without an error. This
#allows some extra macros to start working
TARGET_INCLUDE_DIRECTORIES( ringclient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

TARGET_INCLUDE_DIRECTORIES( ringclient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/private/
)

TARGET_INCLUDE_DIRECTORIES( ringclient PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Allow building with undefined symbols when only the daemon headers are provided
# It speeds up our CI builds
IF(${ring_BIN} MATCHES "ring_BIN-NOTFOUND" AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    SET_TARGET_PROPERTIES( ringclient PROPERTIES LINK_FLAGS "-undefined dynamic_lookup" )
ENDIF()

# Use the daemon when libringclient doesn't link directly to libring
IF(NOT ENABLE_LIBWRAP)
   TARGET_LINK_LIBRARIES( ringclient
      Qt5::DBus
   )
ELSE()
   TARGET_LINK_LIBRARIES( ringclient
      ${ring_BIN}
   )
ENDIF()

# Make LRC slow, but catches issues
IF(${ENABLE_TEST_ASSERTS} MATCHES "ON")
   MESSAGE(STATUS "Adding more debug output")
   ADD_DEFINITIONS(-DENABLE_TEST_ASSERTS=true)
ENDIF()

# Fix some issues on Linux and Android
CHECK_LIBRARY_EXISTS(rt clock_gettime "time.h" NEED_LIBRT)
IF(NEED_LIBRT)
   TARGET_LINK_LIBRARIES(ringclient rt)

   IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
      ADD_DEFINITIONS(-D__ANDROID__=1)
   ELSE()
      ADD_DEFINITIONS(-D__ANDROID__=0)
   ENDIF()

ELSE()
   ADD_DEFINITIONS(-D__ANDROID__=0)
ENDIF()

EXPORT(TARGETS ringclient
   FILE "${PROJECT_BINARY_DIR}/LibRingClientTargets.cmake"
)

SET_TARGET_PROPERTIES( ringclient
   PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_VERSION}
)

IF("${DISABLE_EXPORT}" MATCHES "OFF")
    INSTALL(TARGETS ringclient
        EXPORT LibRingClientTargets
        PUBLIC_HEADER DESTINATION "${INCLUDE_INSTALL_DIR}/libringclient"
        LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}" COMPONENT ringclient
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        COMPONENT Devel
    )

    INSTALL(EXPORT LibRingClientTargets
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/LibRingClient
        COMPONENT Devel
    )

    INSTALL( FILES ${libringclient_video_LIB_HDRS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/libringclient/video
    COMPONENT Devel
    )

    INSTALL( FILES ${libringclient_audio_LIB_HDRS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/libringclient/audio
    COMPONENT Devel
    )

    INSTALL( FILES ${libringclient_media_LIB_HDRS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/libringclient/media
    COMPONENT Devel
    )

    INSTALL( FILES ${libringclient_extensions_LIB_HDRS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/libringclient/extensions
    COMPONENT Devel
    )

    INSTALL( FILES ${libringclient_interface_LIB_HDRS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/libringclient/interfaces
    COMPONENT Devel
    )

    INSTALL( TARGETS ringclient
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
    )

    INSTALL( FILES ${libringclient_CONFIG_PATH}
      DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/LibRingClient
      COMPONENT Devel
    )
ENDIF()

# translations
IF( Qt5LinguistTools_FOUND )
   # translation template file
   SET(TS_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/translations/lrc_en.ts)

   # get all translation files except the template
   FILE(GLOB TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts)
   LIST(REMOVE_ITEM TS_FILES ${TS_TEMPLATE})

   # compiled .qm files
   QT5_ADD_TRANSLATION(QM_FILES ${TS_FILES})

   ADD_CUSTOM_TARGET(translations ALL DEPENDS ${QM_FILES})

   IF("${DISABLE_EXPORT}" MATCHES "OFF")
      INSTALL(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/libringclient/translations")
   ENDIF()
ENDIF()

# uninstall target
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
