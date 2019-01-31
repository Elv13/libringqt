CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

PROJECT(ringqt)
SET(GENERIC_LIB_VERSION "1.0.0")
SET(QT_MIN_VERSION  "5.9.0" )

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
SET(CMAKE_AUTOMOC TRUE)

OPTION(BUILD_SHARED_LIBS   "Build the shared library (instead of static)" ON )
OPTION(ENABLE_VIDEO        "Enable video support"                         ON )
OPTION(MUTE_DRING          "Do not print libring debug output"            OFF)
OPTION(VERBOSE_IPC         "Print all dring function calls (for debug)"   OFF)
OPTION(ENABLE_TEST_ASSERTS "Enable extra asserts (cpu intensive)"         OFF)
OPTION(USE_STATIC_LIBRING  "Always prefer the static libring (buggy)"     OFF)

# DBus is the default on Linux, LibRing on anything else
IF (${CMAKE_SYSTEM_NAME} MATCHES "Linux" OR ${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
   OPTION(ENABLE_LIBWRAP    "Disable the daemon and use libring directly"  OFF)
   OPTION(DISABLE_EXPORT    "Do not install anything"                      OFF)
ELSE()
   OPTION(ENABLE_LIBWRAP    "Disable the daemon and use libring directly"  ON )
   OPTION(DISABLE_EXPORT    "Do not install anything"                      ON )
ENDIF()

# Detect when added with `add_subdirectory`
GET_DIRECTORY_PROPERTY(USES_ADD_SUBDIRECTORY PARENT_DIRECTORY)
IF(USES_ADD_SUBDIRECTORY OR DEFINED LibRingQt_SRC)
   SET(BUILD_SHARED_LIBS OFF)
   SET(DISABLE_EXPORT ON)
ENDIF()

# First, check is the compiler is new enough, most version of Clang are fine
# until problems arise, checking for GCC is enough
IF (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
   MESSAGE(FATAL_ERROR "Your version of GCC is too old, please install GCC 4.9 or later")
ELSEIF(CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5)
   MESSAGE(WARNING "Your compiler doesn't support C++14, slower code will be used")
   ADD_DEFINITIONS(-DCPP11_COMPAT=1)
ENDIF()

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

FIND_PACKAGE(Ring REQUIRED)
FIND_PACKAGE(Qt5Core ${QT_MIN_VERSION} REQUIRED)
FIND_PACKAGE(Qt5Quick ${QT_MIN_VERSION})
FIND_PACKAGE(Qt5LinguistTools QUIET   ) # translations

# Right now libringqt uses a few exceptions, it should be removed, but right
# now they exits.
add_definitions(-fexceptions)

IF(NOT ENABLE_LIBWRAP)
   FIND_PACKAGE(Qt5DBus REQUIRED)
ENDIF()

# Create a proper Qt plugin for QtQuick
IF (Qt5Quick_FOUND)
   add_subdirectory(libringqtquick)
ELSE()
   message(STATUS "QtQuick has not been found, QML support is disabled")
ENDIF()

# Make sure that the d_ptr work as intended
SET(CMAKE_C_VISIBILITY_PRESET hidden)
SET(CMAKE_CXX_VISIBILITY_PRESET hidden)
SET(CMAKE_VISIBILITY_INLINES_HIDDEN 1)

# Always print as many warning as possible
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/extrawarnings.cmake)

# Skip the daemon and use LibRing directly (require adding signal and slots support)
IF(ENABLE_LIBWRAP)
   INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/wraplibring.cmake)
ENDIF()

IF(ENABLE_VIDEO)
   MESSAGE(STATUS "Video enabled")
   add_definitions( -DENABLE_VIDEO=true )
ELSE()
   MESSAGE(STATUS "Video disabled")
ENDIF()

#File to compile
SET( libringqt_LIB_SRCS
  ${libringqt_LIB_SRCS}

  #Data objects
  src/call.cpp
  src/uri.cpp
  src/ringdevice.cpp
  src/account.cpp
  src/address.cpp
  src/credential.cpp
  src/person.cpp
  src/contactmethod.cpp
  src/collections/numbercategory.cpp
  src/macro.cpp
  src/video/rate.cpp
  src/video/device.cpp
  src/video/renderer.cpp
  src/certificate.cpp
  src/securityflaw.cpp
  src/ringtone.cpp
  src/infotemplate.cpp
  src/contactrequest.cpp
  src/presencestatus.cpp
  src/media/media.cpp
  src/media/audio.cpp
  src/media/video.cpp
  src/media/text.cpp
  src/media/file.cpp
  src/media/recording.cpp
  src/media/avrecording.cpp
  src/media/attachment.cpp
  src/media/textrecording.cpp
  src/media/mimemessage.cpp
  src/media/availabilitytracker.cpp

  #Models
  src/private/notificationstatemodel_p.cpp
  src/bootstrapmodel.cpp
  src/individualeditor.cpp
  src/ringdevicemodel.cpp
  src/accountmodel.cpp
  src/availableaccountmodel.cpp
  src/callmodel.cpp
  src/callhistorymodel.cpp
  src/bookmarkmodel.cpp
  src/credentialmodel.cpp
  src/contactmodel.cpp
  src/useractionmodel.cpp
  src/presencestatusmodel.cpp
  src/individualdirectory.cpp
  src/historytimecategorymodel.cpp
  src/numbercategorymodel.cpp
  src/macromodel.cpp
  src/keyexchangemodel.cpp
  src/tlsmethodmodel.cpp
  src/protocolmodel.cpp
  src/numbercompletionmodel.cpp
  src/profilemodel.cpp
  src/ringtonemodel.cpp
  src/infotemplatemanager.cpp
  src/eventmodel.cpp
  src/securityevaluationmodel.cpp
  src/persondirectory.cpp
  src/networkinterfacemodel.cpp
  src/certificatemodel.cpp
  src/ciphermodel.cpp
  src/accountstatusmodel.cpp
  src/codecmodel.cpp
  src/chainoftrustmodel.cpp
  src/pendingcontactrequestmodel.cpp
  src/peerstimelinemodel.cpp
  src/individualtimelinemodel.cpp
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
  src/recentfilemodel.cpp
  src/individual.cpp
  src/private/timelinelens_p.cpp

  # iCal/vCard/WebDav collections
  src/libcard/event.cpp
  src/libcard/calendar.cpp
  src/libcard/eventaggregate.cpp
  src/libcard/historyimporter.cpp
  src/libcard/private/icsloader.cpp
  src/libcard/private/icsbuilder.cpp

  # Error handling requiring user intervention
  src/troubleshoot/base.cpp
  src/troubleshoot/dispatcher.cpp
  src/troubleshoot/handshake.cpp
  src/troubleshoot/videostuck.cpp
  src/troubleshoot/generic.cpp
  src/troubleshoot/crequest.cpp
  src/troubleshoot/callstate.cpp
  src/troubleshoot/absent.cpp
  src/troubleshoot/unhold.cpp

  # Content Management System
  src/picocms/collectioninterface.cpp
  src/picocms/collectioneditor.cpp
  src/picocms/collectionextensioninterface.cpp
  src/picocms/collectionmodel.cpp
  src/picocms/collectionextensionmodel.cpp
  src/picocms/collectionmanagerinterface.cpp
  src/picocms/itembase.cpp

  #Data collections
  src/collections/transitionalpersonbackend.cpp
  src/collections/fallbackpersoncollection.cpp
  src/collections/daemoncertificatecollection.cpp
  src/collections/foldercertificatecollection.cpp
  src/collections/localrecordingcollection.cpp
  src/collections/localtextrecordingcollection.cpp
  src/collections/localhistorycollection.cpp
  src/collections/localprofilecollection.cpp
  src/collections/localmacrocollection.cpp
  src/collections/localbookmarkcollection.cpp
  src/collections/localnameservicecache.cpp
  src/collections/localringtonecollection.cpp
  src/collections/localinfotemplatecollection.cpp
  src/collections/peerprofilecollection2.cpp
  src/collections/notificationstatecollection.cpp

  #Communication
  src/dbus/configurationmanager.cpp
  src/dbus/callmanager.cpp
  src/dbus/instancemanager.cpp
  src/dbus/videomanager.cpp
  src/dbus/presencemanager.cpp

  #Default interface implementations
  src/interfaces/default/accountlistcolorizerdefault.cpp
  src/interfaces/default/presenceserializerdefault.cpp
  src/interfaces/default/pixmapmanipulatordefault.cpp
  src/interfaces/default/shortcutcreatordefault.cpp
  src/interfaces/default/actionextenderdefault.cpp
  src/interfaces/default/dbuserrorhandlerdefault.cpp

  #Other
  src/itemdataroles.cpp # Because namespaced MOC need this as of CMake 3.10
  src/hookmanager.cpp
  src/namedirectory.cpp
  src/private/vcardutils.cpp
  src/private/textrecordingcache.cpp
  src/private/textrecordingmodel.cpp
  src/private/videorenderermanager.cpp
  src/video/previewmanager.cpp
  src/private/sortproxies.cpp
  src/private/threadworker.cpp
  src/private/addressmodel.cpp
  src/mime.cpp
  src/session.cpp
  src/smartinfohub.cpp
  src/private/lensmanager_p.cpp
  src/globalinstances.cpp

  #Extension
  src/extensions/presencecollectionextension.cpp
  src/extensions/securityevaluationextension.cpp
)

IF(ENABLE_LIBWRAP)
   SET(libringqt_LIB_SRCS ${libringqt_LIB_SRCS}
      src/private/directrenderer.cpp
   )
ELSE()
   SET(libringqt_LIB_SRCS ${libringqt_LIB_SRCS}
      src/private/shmrenderer.cpp
   )
ENDIF(ENABLE_LIBWRAP)

# Public API
SET( libringqt_LIB_HDRS
  src/account.h
  src/session.h
  src/address.h
  src/ringdevice.h
  src/credential.h
  src/accountmodel.h
  src/availableaccountmodel.h
  src/individual.h
  src/call.h
  src/call.hpp
  src/callmodel.h
  src/callhistorymodel.h
  src/person.h
  src/bootstrapmodel.h
  src/individualeditor.h
  src/ringdevicemodel.h
  src/bookmarkmodel.h
  src/credentialmodel.h
  src/contactmodel.h
  src/useractionmodel.h
  src/presencestatusmodel.h
  src/presencestatus.h
  src/contactmethod.h
  src/individualdirectory.h
  src/historytimecategorymodel.h
  src/numbercategorymodel.h
  src/keyexchangemodel.h
  src/codecmodel.h
  src/tlsmethodmodel.h
  src/protocolmodel.h
  src/recentfilemodel.h
  src/numbercompletionmodel.h
  src/profilemodel.h
  src/ringtonemodel.h
  src/infotemplatemanager.h
  src/eventmodel.h
  src/securityevaluationmodel.h
  src/certificate.h
  src/persondirectory.h
  src/macromodel.h
  src/hookmanager.h
  src/namedirectory.h
  src/uri.h
  src/macro.h
  src/ringtone.h
  src/infotemplate.h
  src/mime.h
  src/networkinterfacemodel.h
  src/certificatemodel.h
  src/ciphermodel.h
  src/accountstatusmodel.h
  src/securityflaw.h
  src/peerstimelinemodel.h
  src/individualtimelinemodel.h
  src/chainoftrustmodel.h
  src/pendingcontactrequestmodel.h
  src/contactrequest.h
  src/globalinstances.h
  src/itemdataroles.h
  src/smartinfohub.h
  src/usagestatistics.h
  src/bannedcontactmodel.h
)

SET(libringqt_cms_LIB_HDRS
  src/picocms/collectionmediator.h
  src/picocms/collectionmediator.hpp
  src/picocms/collectioneditor.h
  src/picocms/collectioneditor.hpp
  src/picocms/collectionmodel.h
  src/picocms/collectionextensionmodel.h
  src/picocms/collectionextensionmodel.hpp
  src/picocms/collectioninterface.h
  src/picocms/collectioninterface.hpp
  src/picocms/collectionextensioninterface.h
  src/picocms/collectionmanagerinterface.h
  src/picocms/collectionmanagerinterface.hpp
  src/picocms/itembase.h
  src/picocms/itembase.hpp
  src/picocms/collectioncreationinterface.h
  src/picocms/collectionconfigurationinterface.h
)

SET(libringqt_collections_LIB_HDRS
  src/collections/numbercategory.h
  src/collections/transitionalpersonbackend.h
  src/collections/localrecordingcollection.h
  src/collections/localbookmarkcollection.h
  src/collections/localringtonecollection.h
  src/collections/localinfotemplatecollection.h
  src/collections/localmacrocollection.h
  src/collections/localhistorycollection.h
  src/collections/localprofilecollection.h
  src/collections/localtextrecordingcollection.h
  src/collections/peerprofilecollection2.h
  src/collections/fallbackpersoncollection.h
  src/collections/daemoncertificatecollection.h
  src/collections/foldercertificatecollection.h
)

SET(libringqt_video_LIB_HDRS
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

SET(libringqt_audio_LIB_HDRS
  src/audio/alsapluginmodel.h
  src/audio/inputdevicemodel.h
  src/audio/managermodel.h
  src/audio/outputdevicemodel.h
  src/audio/ringtonedevicemodel.h
  src/audio/settings.h
)

SET(libringqt_extensions_LIB_HDRS
  src/extensions/presencecollectionextension.h
  src/extensions/securityevaluationextension.h
)

SET(libringqt_media_LIB_HDRS
  src/media/media.h
  src/media/audio.h
  src/media/video.h
  src/media/text.h
  src/media/file.h
  src/media/attachment.h
  src/media/recording.h
  src/media/avrecording.h
  src/media/textrecording.h
  src/media/mimemessage.h
  src/media/recordingmodel.h
  src/media/availabilitytracker.h
)

SET(libringqt_card_LIB_HDRS
  src/libcard/flagutils.h
  src/libcard/event.h
  src/libcard/calendar.h
  src/libcard/eventaggregate.h
  src/libcard/historyimporter.h
)

SET(libringqt_troubleshoot_LIB_HDRS
  src/troubleshoot/dispatcher.h
  src/troubleshoot/base.h
)

SET(libringqt_interface_LIB_HDRS
  src/interfaces/accountlistcolorizeri.h
  src/interfaces/fileprovideri.h
  src/interfaces/contactmethodselectori.h
  src/interfaces/presenceserializeri.h
  src/interfaces/itemmodelstateserializeri.h
  src/interfaces/pixmapmanipulatori.h
  src/interfaces/shortcutcreatori.h
  src/interfaces/actionextenderi.h
  src/interfaces/dbuserrorhandleri.h
)

SET( libringqt_extra_LIB_HDRS
  src/typedefs.h
)

# Build all dbus IPC interfaces
IF(NOT ENABLE_LIBWRAP)

   # Build dbus interfaces
   IF(DEFINED RING_XML_INTERFACES_DIR)
      SET (dbus_xml_introspecs_path ${RING_XML_INTERFACES_DIR})
   ELSEIF(EXISTS "${RING_BUILD_DIR}/../bin/dbus")
      SET (dbus_xml_introspecs_path ${RING_BUILD_DIR}/../bin/dbus)
   ELSE()
      SET (dbus_xml_introspecs_path ${CMAKE_INSTALL_PREFIX}/share/dbus-1/interfaces)
   ENDIF()

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
      libringqt_LIB_SRCS
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
      libringqt_LIB_SRCS
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
      libringqt_LIB_SRCS
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
      libringqt_LIB_SRCS
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
      libringqt_LIB_SRCS
      ${instance_xml}
      instance_dbus_interface
   )

ENDIF()

# Manually wrap private files and interfaces
SET(libringqt_PRIVATE_HDRS
   src/private/call_p.h
   src/private/ringdevicemodel_p.h
   src/private/namedirectory_p.h
   src/private/account_p.h
   src/private/sortproxies.h
   src/private/accountmodel_p.h
   src/private/individualdirectory_p.h
   src/private/videorenderer_p.h
   src/private/textrecording_p.h
   src/private/videodevice_p.h
   src/private/collectionmodel_p.h
   src/private/contactmethod_p.h
   src/private/securityflaw_p.h
   src/private/macromodel_p.h
   src/private/person_p.h
   src/private/securityevaluationmodel_p.h
   src/private/imconversationmanagerprivate.h
   src/private/smartInfoHub_p.h
   src/private/certificatemodel_p.h
   src/usagestatistics.h
   src/private/personstatistics.hpp
   src/itemdataroles.h
   src/picocms/collectionconfigurationinterface.h
   src/picocms/collectioncreationinterface.h
)

QT5_WRAP_CPP(LIB_HEADER_MOC ${libringqt_PRIVATE_HDRS})

ADD_LIBRARY( ringqt ${libringqt_LIB_SRCS} ${LIB_HEADER_MOC} )

IF (DEFINED ring_INCLUDE_DIRS)
   TARGET_INCLUDE_DIRECTORIES( ringqt PRIVATE
      ${ring_INCLUDE_DIRS}
   )
ENDIF()

IF(ENABLE_LIBWRAP)
   TARGET_INCLUDE_DIRECTORIES( ringqt PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src/qtwrapper/
   )
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
   FIND_PACKAGE(Qt5Concurrent)

   #For some reason, QSemaphore won't compile on Windows without QtConcurrent
   TARGET_LINK_LIBRARIES( ringqt
      Qt5::Concurrent
   )
ENDIF()

SET(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/include)

SET(libringqt_CONFIG_PATH "${CMAKE_CURRENT_BINARY_DIR}/LibRingQtConfig.cmake")

CONFIGURE_PACKAGE_CONFIG_FILE(
   "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LibRingQtConfig.cmake.in" ${libringqt_CONFIG_PATH}
   INSTALL_DESTINATION ${LIB_INSTALL_DIR}/libringqt/cmake
   PATH_VARS INCLUDE_INSTALL_DIR
)

SET_TARGET_PROPERTIES(ringqt PROPERTIES
    PUBLIC_HEADER "${libringqt_LIB_HDRS};${libringqt_extra_LIB_HDRS}"
)

TARGET_LINK_LIBRARIES( ringqt
   Qt5::Core
)

# Include as PRIVATE because of the /src/ component
#TODO figure out how to make them public without an error. This
#allows some extra macros to start working
TARGET_INCLUDE_DIRECTORIES( ringqt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/private/
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Use the daemon when libringqt doesn't link directly to libring
IF(NOT ENABLE_LIBWRAP)
   TARGET_LINK_LIBRARIES( ringqt
      Qt5::DBus
   )
ELSE()
   TARGET_LINK_LIBRARIES( ringqt
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
   TARGET_LINK_LIBRARIES(ringqt rt)
ENDIF()

IF(${CMAKE_SYSTEM_NAME} MATCHES "Android")
    message(STATUS Android detected)
ELSEIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    TARGET_LINK_LIBRARIES( ringqt -lpthread )
ENDIF()

EXPORT(TARGETS ringqt
   FILE "${PROJECT_BINARY_DIR}/LibRingQtTargets.cmake"
)

SET_TARGET_PROPERTIES( ringqt
   PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_VERSION}
)

IF("${DISABLE_EXPORT}" MATCHES "OFF")
    INSTALL(TARGETS ringqt
        EXPORT LibRingQtTargets
        PUBLIC_HEADER DESTINATION "${INCLUDE_INSTALL_DIR}/libringqt"
        LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}" COMPONENT ringqt
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        COMPONENT Devel
    )

    INSTALL(EXPORT LibRingQtTargets
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/LibRingQt
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_video_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/video
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_audio_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/audio
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_media_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/media
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_card_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/libcard
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_troubleshoot_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/troubleshoot
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_extensions_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/extensions
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_interface_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/interfaces
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_cms_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/picocms
        COMPONENT Devel
    )

    INSTALL( FILES ${libringqt_collections_LIB_HDRS}
        DESTINATION ${INCLUDE_INSTALL_DIR}/libringqt/collections
        COMPONENT Devel
    )

    INSTALL( TARGETS ringqt
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
    )

    INSTALL( FILES ${libringqt_CONFIG_PATH}
      DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/LibRingQt
      COMPONENT Devel
    )
ELSE()
    INSTALL(TARGETS ringqt
        EXPORT LibRingQtTargets
        PUBLIC_HEADER DESTINATION "${INCLUDE_INSTALL_DIR}/libringqt"
        LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}" COMPONENT ringqt
        ARCHIVE DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
        COMPONENT Devel
    )

    INSTALL(EXPORT LibRingQtTargets
        DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/cmake/LibRingQt
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
      INSTALL(FILES ${QM_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/share/libringqt/translations")
   ENDIF()
ENDIF()

# uninstall target
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
)

ADD_CUSTOM_TARGET(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
)
