cmake_minimum_required(VERSION 3.14)

project(micBridgeDesktop)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set(CMAKE_CXX_FLAGS "-Wall")
#set(CMAKE_CXX_FLAGS_RELEASE "/Ox")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

#set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

#set (BUILD_SHARED_LIBS FALSE) # should force the libraries to be static

#if (WIN32)
#        if (BUILD_SHARED_LIBS)
#            set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MDd")
#            set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MDd")
#            set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MD")
#            set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MD")
#        else()
#            set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MT")
#            set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -MT")
#            set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")
#            set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -MT")
#        endif()
#endif()

set(Boost_USE_STATIC_LIBS        ON)  # only find static libs
set(Boost_USE_DEBUG_LIBS        OFF)  # ignore debug libs and
set(Boost_USE_RELEASE_LIBS       ON)  # only find release libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
if (NOT DEFINED BOOST_ROOT)
        message(FATAL_ERROR "BOOST_ROOT dir is not specified. Can not continue project parsing.")
endif()
find_package(Boost)

find_package(Qt5 REQUIRED COMPONENTS Core Widgets Multimedia)

set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/Icons/application_icon.rc")

add_subdirectory(ServiceDiscovery)
add_subdirectory(Broadcast)

# Uncomment following line to remove console while running
add_executable(micBridgeDesktop WIN32 "main.cpp"
#add_executable(micBridgeDesktop WIN32 "main.cpp"
                                       "WMFUtils.h"
                                       "WMFUtils.cpp"
                                       "WMFAACDecoder.h"
                                       "WMFAACDecoder.cpp"

                                       "UI_DriverControl.h"
                                       "UI_DriverControl.cpp"
                                       "UI_MainWindow.h"
                                       "UI_MainWindow.cpp"
                                       "UI_AudioLevelsIODevice.h"
                                       "UI_AudioLevelsIODevice.cpp"
                                       "UI_AvailableServicesList.h"
                                       "UI_AvailableServicesList.cpp"
                                       "UI_AvailableServicesListModel.h"
                                       "UI_AvailableServicesListModel.cpp"

                                       "UI_CommonResources.qrc"
                                       ${APP_ICON_RESOURCE_WINDOWS})

set_target_properties(micBridgeDesktop PROPERTIES COMPILE_DEFINITIONS BUILDER_STATIC_DEFINE)

target_link_libraries(micBridgeDesktop  ServiceDiscovery
                                        Broadcast
                                        Qt5::Widgets
                                        Qt5::Multimedia)

if (WIN32)
    target_link_libraries(micBridgeDesktop Cfgmgr32)
endif()

get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

add_custom_command(TARGET micBridgeDesktop POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/qtDeploy/
                   COMMAND ${WINDEPLOYQT_EXECUTABLE}
                           --release
                           --verbose 1
                           --no-compiler-runtime
                           --no-angle
                           --no-opengl
                           --no-opengl-sw
                           --no-webkit2
                           --no-quick-import
                           --no-svg
                           --no-translations
                           --dir ${CMAKE_BINARY_DIR}/qtDeploy $<TARGET_FILE:micBridgeDesktop>
)
install(
    DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/
    DESTINATION .
    FILES_MATCHING PATTERN "*.*"
)

install(TARGETS micBridgeDesktop
        DESTINATION .
       )

install(
        DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/styles
        DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/platforms
        DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/imageformats
        DESTINATION .
        FILES_MATCHING PATTERN "*.*"
    )

install(
        FILES ${CMAKE_BINARY_DIR}/qtDeploy/Qt5Widgets.dll
        ${CMAKE_BINARY_DIR}/qtDeploy/Qt5Gui.dll
        ${CMAKE_BINARY_DIR}/qtDeploy/Qt5Core.dll
        DESTINATION .
    )



configure_file(${DRIVER_ROOT}/micbridge.cat
               ${CMAKE_BINARY_DIR}/driver/micbridge.cat
               COPYONLY)

configure_file(${DRIVER_ROOT}/MicBridge.sys
               ${CMAKE_BINARY_DIR}/driver/MicBridge.sys
               COPYONLY)

configure_file(${DRIVER_ROOT}/MicBridge.inf
               ${CMAKE_BINARY_DIR}/driver/MicBridge.inf
               COPYONLY)

# setup.exe = devcon.exe
configure_file(${DRIVER_ROOT}/setup.exe
               ${CMAKE_BINARY_DIR}/driver/setup.exe
               COPYONLY)

install(
    DIRECTORY ${CMAKE_BINARY_DIR}/driver
    DESTINATION .
)

set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_NAME "MicBridge Desktop")
set(CPACK_PACKAGE_VERSION "1.0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_VENDOR "YVV Software")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "MicBridge Desktop")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}")
set(CPACK_SOURCE_IGNORE_FILES ${CPACK_IGNORE_FILES})
set(CMAKE_MODULE_PATH  ${PROJECT_SOURCE_DIR}/assets/nsis ${CMAKE_MODULE_PATH})
set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\MicBridge Desktop.lnk' '$INSTDIR\\\\micBridgeDesktop.exe' ''"
)
set(CPACK_NSIS_DELETE_ICONS_EXTRA
"Delete '$SMPROGRAMS\\\\$START_MENU\\\\MicBridge Desktop.lnk'"
)

#prepare start menu links
LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA "  CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\micBridge.lnk' '$INSTDIR\\\\micBridgeDesktop.exe' ''")
LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA "  Delete '$SMPROGRAMS\\\\$START_MENU\\\\MicBridge Desktop.lnk'")
#prepare desktop links
LIST(APPEND CPACK_NSIS_CREATE_ICONS_EXTRA  "  CreateShortCut '$DESKTOP\\\\MicBridge Desktop.lnk' '$INSTDIR\\\\micBridgeDesktop.exe' ''")
LIST(APPEND CPACK_NSIS_DELETE_ICONS_EXTRA  "  Delete '$DESKTOP\\\\MicBridge Desktop.lnk'")
string (REPLACE ";" "\n" CPACK_NSIS_CREATE_ICONS_EXTRA "${CPACK_NSIS_CREATE_ICONS_EXTRA}")
string (REPLACE ";" "\n" CPACK_NSIS_DELETE_ICONS_EXTRA "${CPACK_NSIS_DELETE_ICONS_EXTRA}")

list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " !include x64.nsh")
list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " \\\${DisableX64FSRedirection}")
list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " DetailPrint `\\\"$INSTDIR\\\\driver\\\\setup.exe\\\" \\\"$INSTDIR\\\\driver\\\\MicBridge.inf\\\"`")
list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " nsExec::ExecToLog `\\\"$INSTDIR\\\\driver\\\\setup.exe\\\" remove \\\"$INSTDIR\\\\driver\\\\MicBridge.inf\\\" Root\\\\MicBridge`")
list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " nsExec::ExecToLog `\\\"$INSTDIR\\\\driver\\\\setup.exe\\\" install \\\"$INSTDIR\\\\driver\\\\MicBridge.inf\\\" Root\\\\MicBridge`")
list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " \\\${EnableX64FSRedirection}")
list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " SetRebootFlag true")
#list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " Sleep 5000")
#list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " MessageBox MB_YESNO|MB_ICONQUESTION \\\"Application requires restart. Reboot now?\\\" IDNO +2")
#list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS "     Reboot")
string (REPLACE ";" "\n" CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}")

list ( APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS " nsExec::ExecToLog `\\\"$INSTDIR\\\\driver\\\\setup.exe\\\" remove \\\"$INSTDIR\\\\driver\\\\MicBridge.inf\\\" Root\\\\MicBridge`")
#list ( APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS " Sleep 5000")
#list ( APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS " MessageBox MB_YESNO|MB_ICONQUESTION \\\"Application requires restart. Reboot now?\\\" IDNO +2")
list ( APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS " SetRebootFlag true")
string (REPLACE ";" "\n" CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}")
#list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " ExecWait ./tmp/some-other-installer.exe")
#list ( APPEND CPACK_NSIS_EXTRA_INSTALL_COMMANDS " RMDir /r ./tmp/")

include(CPack)
   configure_file(
   ${PROJECT_SOURCE_DIR}/assets/nsis/NSIS.definitions.nsh.in
   ${CMAKE_CURRENT_BINARY_DIR}/assets/nsis/NSIS.definitions.nsh
   )
