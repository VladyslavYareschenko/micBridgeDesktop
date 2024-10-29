cmake_minimum_required(VERSION 3.14)

project(micBridgeDesktop)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(3rdParty/Config.cmake)
add_subdirectory(ServiceDiscovery)
add_subdirectory(Broadcast)

set(GUI_SRC "UI_DriverControl.cpp"
            "UI_MainWindow.cpp"
            "UI_AudioLevelsIODevice.cpp"
            "UI_AvailableServicesList.cpp"
            "UI_AvailableServicesListModel.cpp"
             "UI_DriverControl.h"
             "UI_MainWindow.h"
             "UI_AudioLevelsIODevice.h"
             "UI_AvailableServicesList.h"
             "UI_AvailableServicesListModel.h"
             "UI_CommonResources.qrc")

# Uncomment following line to remove console while running
add_executable(micBridgeDesktop WIN32 "main.cpp"
# add_executable(micBridgeDesktop        "main.cpp"
                                       "WMFUtils.h"
                                       "WMFUtils.cpp"
                                       "WMFAACDecoder.h"
                                       "WMFAACDecoder.cpp"

                                       ${GUI_SRC}

                                       "${CMAKE_CURRENT_SOURCE_DIR}/Icons/application_icon.rc")

set_property(TARGET micBridgeDesktop PROPERTY AUTOMOC ON)
set_property(TARGET micBridgeDesktop PROPERTY AUTORCC ON)
set_target_properties(micBridgeDesktop
                     PROPERTIES COMPILE_DEFINITIONS BUILDER_STATIC_DEFINE)
target_link_libraries(micBridgeDesktop  ServiceDiscovery
                                        Broadcast
                                        Qt5::Widgets
                                        Qt5::Multimedia)

add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
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
                           --dir ${CMAKE_BINARY_DIR}/qtDeploy $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
)

if (WIN32)
    target_link_libraries(micBridgeDesktop Cfgmgr32)
endif()

if (NOT DRIVER_ROOT)
    message(FATAL_ERROR "Windows driver root not specified. Aborting build.")
endif()

install(TARGETS ${CMAKE_PROJECT_NAME}
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

include(InstallRequiredSystemLibraries)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
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
string (REPLACE ";" "\n" CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}")

list ( APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS " nsExec::ExecToLog `\\\"$INSTDIR\\\\driver\\\\setup.exe\\\" remove \\\"$INSTDIR\\\\driver\\\\MicBridge.inf\\\" Root\\\\MicBridge`")
list ( APPEND CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS " SetRebootFlag true")
string (REPLACE ";" "\n" CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}")

include(CPack)
   configure_file(
   ${PROJECT_SOURCE_DIR}/assets/nsis/NSIS.definitions.nsh.in
   ${CMAKE_CURRENT_BINARY_DIR}/assets/nsis/NSIS.definitions.nsh
   )
